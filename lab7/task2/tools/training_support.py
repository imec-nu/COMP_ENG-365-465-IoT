"""Train a four-class gesture CNN; export float/INT8 TFLite and Arduino headers."""
import argparse,csv,hashlib,json,os
from pathlib import Path
os.environ.setdefault('TF_CPP_MIN_LOG_LEVEL','2')
import numpy as np
from PIL import Image
from common import CONFIG

def load_dataset(root):
    if json.loads((root/'dataset_config.json').read_text())!=CONFIG:raise ValueError('Frontend mismatch')
    with (root/'labels.csv').open(newline='',encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
    if not rows:raise ValueError('Empty labels.csv')
    labels=sorted({r['label'].strip() for r in rows})
    if '' in labels:raise ValueError('Unlabeled samples: fill label column or remove unwanted rows')
    if len(labels)!=4 or 'background' not in labels:
        raise ValueError(f'Expected three gestures + background. Found: {labels}')
    data={s:([],[]) for s in ['train','val','test']};sessions={};ids=set();hashes={}
    for r in rows:
        split=r['split'];session=r['session_id'].strip()
        if split not in data or not session:raise ValueError('Missing session or invalid split')
        if session in sessions and sessions[session]!=split:raise ValueError('Session crosses splits: '+session)
        sessions[session]=split
        if r['sample_id'] in ids:raise ValueError('Duplicate ID: '+r['sample_id'])
        ids.add(r['sample_id'])
        def path(name):
            v=(root/name).resolve()
            if not v.is_relative_to(root.resolve()):raise ValueError('Path outside dataset')
            return v
        with Image.open(path(r['raw_file'])) as im:
            if im.format!='JPEG' or im.size!=(320,240):raise ValueError('Invalid original JPEG')
            im.load()
        x=np.load(path(r['input_file']),allow_pickle=False)
        if x.shape!=(64,64) or x.dtype!=np.uint8:raise ValueError('Expected uint8 (64,64): '+r['input_file'])
        h=hashlib.sha256(x.tobytes()).hexdigest()
        if h in hashes and hashes[h]!=split:raise ValueError('Identical image inputs cross dataset splits')
        hashes[h]=split
        data[split][0].append(x);data[split][1].append(labels.index(r['label'].strip()))
    for split,(xs,ys) in data.items():
        if set(ys)!=set(range(4)):raise ValueError(f'All four classes required in {split}')
        # Board input is stored verbatim. The only model-input scaling is /255.
        data[split]=(np.asarray(xs,dtype=np.float32)[...,None]/255.0,np.asarray(ys,np.int32))
    return data,labels

def run_training(build_model, train_model):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dataset',type=Path,default=Path('gesture_dataset'))
    p.add_argument('--output',type=Path,default=Path('trained_gesture_model'))
    p.add_argument('--epochs',type=int,default=50);p.add_argument('--batch-size',type=int,default=8)
    p.add_argument('--seed',type=int,default=42)
    p.add_argument('--threshold',type=float,default=.6,help='User-selected confidence threshold; inspect validation metrics, not test, when tuning')
    a=p.parse_args()
    if a.epochs<1 or a.batch_size<1 or not 0<=a.threshold<=1:p.error('Invalid epochs/batch-size/threshold')
    data,labels=load_dataset(a.dataset)
    if a.output.exists() and any(a.output.iterdir()):p.error('Output directory must be empty; choose a new --output')
    a.output.mkdir(parents=True,exist_ok=True)
    import tensorflow as tf
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    tf.keras.utils.set_random_seed(a.seed)
    xt,yt=data['train'];xv,yv=data['val'];xe,ye=data['test']
    for split,(x,y) in data.items():print(split,dict(zip(labels,np.bincount(y,minlength=4).tolist())))
    model=build_model(tf)
    history = train_model(tf, model, xt, yt, xv, yv, a.epochs, a.batch_size)
    model.save(a.output/'model.keras');model.export(str(a.output/'saved_model'), input_signature=[tf.TensorSpec([1,64,64,1],tf.float32)])
    (a.output/'history.json').write_text(json.dumps(history.history,indent=2))
    fig,ax=plt.subplots(1,2,figsize=(10,4))
    for axis,key in zip(ax,['loss','accuracy']):
        axis.plot(history.history[key],label='train');axis.plot(history.history['val_'+key],label='val')
        axis.set(xlabel='Epoch (zero-based)',ylabel=key);axis.legend()
    fig.tight_layout();fig.savefig(a.output/'training_curves.png');plt.close(fig)
    converter=tf.lite.TFLiteConverter.from_saved_model(str(a.output/'saved_model'))
    (a.output/'model_float.tflite').write_bytes(converter.convert())
    converter=tf.lite.TFLiteConverter.from_saved_model(str(a.output/'saved_model'))
    converter.optimizations=[tf.lite.Optimize.DEFAULT]
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
    if ins<=0 or outs<=0:raise ValueError('Invalid INT8 scales')
    def predict(x):
        scores=[]
        for sample in x:
            q=np.clip(np.rint(sample[None]/ins)+inz,-128,127).astype(np.int8)
            interpreter.set_tensor(inp['index'],q);interpreter.invoke()
            scores.append((interpreter.get_tensor(out['index'])[0].astype(np.float32)-outz)*outs)
        return np.asarray(scores)
    def metrics(scores,y):
        pred=scores.argmax(1);accepted=scores.max(1)>=a.threshold
        return dict(accuracy=float(np.mean(pred==y)),coverage=float(accepted.mean()),
          thresholded_accuracy=float(np.mean((pred==y)&accepted)),
          accepted_accuracy=float(np.mean(pred[accepted]==y[accepted])) if accepted.any() else None)
    pv=predict(xv);pi=predict(xe);pf=model.predict(xe,verbose=0)
    # Closed-set 4x4 confusion; rejection-aware 4x5 matrix also supplied.
    def report_matrix(name,pred,reject=False):
        cols=labels+(['UNKNOWN'] if reject else [])
        cm=np.zeros((4,len(cols)),dtype=int);np.add.at(cm,(ye,pred),1)
        with (a.output/(name+'.csv')).open('w',newline='') as f:
            w=csv.writer(f);w.writerow(['true/predicted']+cols)
            for label,row in zip(labels,cm):w.writerow([label]+row.tolist())
        fig,ax=plt.subplots(figsize=(8,5));ax.imshow(cm,cmap='Blues')
        ax.set(xticks=range(len(cols)),yticks=range(4),xticklabels=cols,yticklabels=labels,xlabel='Predicted',ylabel='True')
        plt.setp(ax.get_xticklabels(),rotation=30,ha='right')
        for i in range(4):
            for j in range(len(cols)):ax.text(j,i,str(cm[i,j]),ha='center',va='center')
        fig.tight_layout();fig.savefig(a.output/(name+'.png'));plt.close(fig)
    report_matrix('confusion_matrix',pi.argmax(1))
    pred=pi.argmax(1);pred[pi.max(1)<a.threshold]=4
    report_matrix('confusion_matrix_with_rejection',pred,True)
    metadata=dict(frontend=CONFIG,labels=labels,background_index=labels.index('background'),
      input_shape=inp['shape'].tolist(),input_scale=float(ins),input_zero_point=int(inz),
      output_scale=float(outs),output_zero_point=int(outz),accept_threshold=a.threshold,
      threshold_source='user_selected',pixel_divisor=255.0,
      model_bytes=len(blob),tensorflow_version=tf.__version__,seed=a.seed,
      counts={s:np.bincount(y,minlength=4).tolist() for s,(x,y) in data.items()},
      float_test_accuracy=float(np.mean(pf.argmax(1)==ye)),
      int8_validation=metrics(pv,yv),int8_test=metrics(pi,ye))
    (a.output/'metadata.json').write_text(json.dumps(metadata,indent=2))
    (a.output/'labels.json').write_text(json.dumps(labels,indent=2))
    lines=[', '.join(f'0x{b:02x}' for b in blob[i:i+16]) for i in range(0,len(blob),16)]
    (a.output/'model_data.h').write_text('#pragma once\n#include <stdint.h>\nalignas(16) const unsigned char g_model[] = {\n'+',\n'.join(lines)+'\n};\nconst unsigned int g_model_len = sizeof(g_model);\n')
    header='#pragma once\n#include <stdint.h>\n#include <math.h>\n'
    for k,v in dict(INPUT_SCALE=ins,OUTPUT_SCALE=outs,ACCEPT_THRESHOLD=a.threshold).items():
        header+=f'constexpr float {k} = {v:.9e}f;\n'
    header+=f'constexpr int INPUT_ZERO_POINT={inz}, OUTPUT_ZERO_POINT={outz}, BACKGROUND_INDEX={labels.index("background")};\n'
    header+='const char* const CLASS_LABELS[] = {'+', '.join(json.dumps(s) for s in labels)+'};\n'
    (a.output/'model_config.h').write_text(header)
    print(json.dumps(metadata,indent=2))
