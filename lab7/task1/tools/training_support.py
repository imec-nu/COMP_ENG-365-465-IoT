"""Train six-class CNN from ESP32-computed inputs; export float and INT8 TFLite."""
import argparse,csv,json,os
from pathlib import Path
os.environ.setdefault('TF_CPP_MIN_LOG_LEVEL','2')
import numpy as np
from common import CONFIG

def load_dataset(root):
    if json.loads((root/'dataset_config.json').read_text())!=CONFIG:
        raise ValueError('Frontend configuration mismatch')
    with (root/'labels.csv').open(newline='',encoding='utf-8-sig') as f: rows=list(csv.DictReader(f))
    if not rows:raise ValueError('Empty dataset')
    labels=sorted({r['label'].strip() for r in rows})
    if '' in labels:raise ValueError('Unlabeled rows: edit labels.csv before training')
    if len(labels)!=6 or 'unknown' not in labels:
        raise ValueError('Expected five keywords and the label unknown (six classes total)')
    sessions={};ids=set(); data={k:([],[]) for k in ('train','val','test')}
    hashes={}
    import hashlib,wave
    for r in rows:
        split=r['split'];session=r['session_id'].strip()
        if split not in data or not session:raise ValueError('Invalid split or missing session')
        if session in sessions and sessions[session]!=split:raise ValueError('Session leakage: '+session)
        sessions[session]=split
        if r['sample_id'] in ids:raise ValueError('Duplicate sample ID')
        ids.add(r['sample_id'])
        def resolve(name):
            path=(root/name).resolve()
            if not path.is_relative_to(root.resolve()):raise ValueError('Path outside dataset')
            return path
        with wave.open(str(resolve(r['raw_file'])),'rb') as w:
            if (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())!=(1,2,16000,32000):
                raise ValueError('Invalid WAV '+r['raw_file'])
        x=np.load(resolve(r['input_file']),allow_pickle=False)
        if x.shape!=(124,32) or x.dtype!=np.float32 or not np.isfinite(x).all():
            raise ValueError('Invalid features '+r['input_file'])
        digest=hashlib.sha256(x.tobytes()).hexdigest()
        if digest in hashes and hashes[digest]!=split:raise ValueError('Identical features cross splits')
        hashes[digest]=split
        data[split][0].append(x);data[split][1].append(labels.index(r['label'].strip()))
    for split,(xs,ys) in data.items():
        if set(ys)!=set(range(6)):raise ValueError(f'All six classes must appear in {split}')
        data[split]=(np.asarray(xs,dtype=np.float32)[...,None],np.asarray(ys,dtype=np.int32))
    return data,labels

def run_training(normalize_inputs, build_model, fit_model):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dataset',type=Path,default=Path('dataset'))
    p.add_argument('--output',type=Path,default=Path('trained_model'))
    p.add_argument('--epochs',type=int,default=50);p.add_argument('--batch-size',type=int,default=16)
    p.add_argument('--seed',type=int,default=42)
    a=p.parse_args()
    if a.epochs<1 or a.batch_size<1:p.error('epochs and batch-size must be positive')
    data,labels=load_dataset(a.dataset)
    if a.output.exists() and any(a.output.iterdir()):p.error('Output must be empty; choose a new --output')
    a.output.mkdir(parents=True,exist_ok=True)
    import tensorflow as tf
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    tf.keras.utils.set_random_seed(a.seed)
    data, mean, std = normalize_inputs(data)
    xt, yt = data['train']
    xv, yv = data['val']
    xe, ye = data['test']
    model = build_model(tf)
    callbacks = [tf.keras.callbacks.EarlyStopping(
        monitor='val_loss', patience=8, restore_best_weights=True)]
    history = fit_model(model, xt, yt, xv, yv, a.epochs, a.batch_size, callbacks)
    model.save(a.output/'model.keras')
    (a.output/'history.json').write_text(json.dumps(history.history,indent=2))
    fig,ax=plt.subplots(1,2,figsize=(10,4))
    for axis,key in zip(ax,['loss','accuracy']):
        axis.plot(history.history[key],label='train');axis.plot(history.history['val_'+key],label='val')
        axis.set(xlabel='Epoch (zero-based)',ylabel=key);axis.legend()
    fig.tight_layout();fig.savefig(a.output/'training_curves.png');plt.close(fig)
    # SavedModel export avoids Keras 3 variable conversion issues in TF 2.16.
    model.export(str(a.output/'saved_model'), input_signature=[tf.TensorSpec([1,124,32,1],tf.float32)])
    float_converter=tf.lite.TFLiteConverter.from_saved_model(str(a.output/'saved_model'))
    (a.output/'model_float.tflite').write_bytes(float_converter.convert())
    converter=tf.lite.TFLiteConverter.from_saved_model(str(a.output/'saved_model'))
    converter.optimizations=[tf.lite.Optimize.DEFAULT]
    # Only training samples calibrate quantization. Shuffle to cover all labels.
    indices=np.random.default_rng(a.seed).permutation(len(xt))[:300]
    def representative():
        for i in indices:yield [xt[i:i+1].astype(np.float32)]
    converter.representative_dataset=representative
    converter.target_spec.supported_ops=[tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type=tf.int8;converter.inference_output_type=tf.int8
    blob=converter.convert();(a.output/'model_int8.tflite').write_bytes(blob)
    interpreter=tf.lite.Interpreter(model_content=blob);interpreter.allocate_tensors()
    inp=interpreter.get_input_details()[0];out=interpreter.get_output_details()[0]
    ins,inz=inp['quantization'];outs,outz=out['quantization']
    if ins<=0 or outs<=0:raise ValueError('Invalid INT8 quantization scales')
    def predict_int8(x):
        scores=[]
        for sample in x:
            q=np.clip((np.rint(sample[None]/ins)+inz),-128,127).astype(np.int8)
            interpreter.set_tensor(inp['index'],q);interpreter.invoke()
            scores.append((interpreter.get_tensor(out['index'])[0].astype(np.float32)-outz)*outs)
        return np.asarray(scores)
    unknown=labels.index('unknown');pv=predict_int8(xv)
    # Tune threshold on validation only, balancing all six class recalls.
    def accepted(scores,threshold):
        pred=scores.argmax(1);pred[scores.max(1)<threshold]=unknown;return pred
    def balanced(y,pred):return float(np.mean([np.mean(pred[y==i]==i) for i in range(6)]))
    thresholds=np.linspace(0,0.95,96)
    threshold=float(max(thresholds,key=lambda t:balanced(yv,accepted(pv,t))))
    pf=model.predict(xe,verbose=0);pi=predict_int8(xe);pred=accepted(pi,threshold)
    cm=np.zeros((6,6),dtype=int);np.add.at(cm,(ye,pred),1)
    with (a.output/'confusion_matrix.csv').open('w',newline='') as f:
        w=csv.writer(f);w.writerow(['true/predicted']+labels)
        for label,row in zip(labels,cm):w.writerow([label]+row.tolist())
    fig,ax=plt.subplots(figsize=(7,6));ax.imshow(cm,cmap='Blues')
    ax.set(xticks=range(6),yticks=range(6),xticklabels=labels,yticklabels=labels,xlabel='Predicted',ylabel='True')
    plt.setp(ax.get_xticklabels(),rotation=35,ha='right')
    for i in range(6):
        for j in range(6):ax.text(j,i,str(cm[i,j]),ha='center',va='center')
    fig.tight_layout();fig.savefig(a.output/'confusion_matrix.png');plt.close(fig)
    metadata=dict(frontend=CONFIG,labels=labels,mean=mean,std=std,
        input_scale=float(ins),input_zero_point=int(inz),output_scale=float(outs),output_zero_point=int(outz),
        unknown_index=unknown,accept_threshold=threshold,
        input_shape=inp['shape'].tolist(),tensorflow_version=tf.__version__,seed=a.seed,
        counts={s:np.bincount(y,minlength=6).tolist() for s,(x,y) in data.items()},
        float_test_accuracy=float(np.mean(pf.argmax(1)==ye)),
        int8_test_accuracy=float(np.mean(pi.argmax(1)==ye)),
        int8_thresholded_test_accuracy=float(np.mean(pred==ye)),
        int8_thresholded_balanced_accuracy=balanced(ye,pred),model_bytes=len(blob))
    (a.output/'metadata.json').write_text(json.dumps(metadata,indent=2))
    (a.output/'labels.json').write_text(json.dumps(labels,indent=2))
    lines=[', '.join(f'0x{b:02x}' for b in blob[i:i+16]) for i in range(0,len(blob),16)]
    (a.output/'model_data.h').write_text('#pragma once\n#include <stdint.h>\nalignas(16) const unsigned char g_model[] = {\n'+',\n'.join(lines)+'\n};\nconst unsigned int g_model_len = sizeof(g_model);\n')
    floats=dict(INPUT_MEAN=mean,INPUT_STD=std,INPUT_SCALE=float(ins),OUTPUT_SCALE=float(outs),ACCEPT_THRESHOLD=threshold)
    header='#pragma once\n#include <stdint.h>\n#include <math.h>\n'
    for k,v in floats.items():header+=f'constexpr float {k} = {v:.9e}f;\n'
    header+=f'constexpr int INPUT_ZERO_POINT={inz}, OUTPUT_ZERO_POINT={outz}, UNKNOWN_INDEX={unknown};\n'
    header+='const char* const CLASS_LABELS[] = {'+', '.join(json.dumps(s) for s in labels)+'};\n'
    (a.output/'model_config.h').write_text(header)
    print(json.dumps(metadata,indent=2))
