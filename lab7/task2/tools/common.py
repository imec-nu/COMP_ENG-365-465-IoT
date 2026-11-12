"""GST7 protocol; save the camera JPEG and board-produced grayscale, without recomputing inputs."""
import csv,io,json,re,struct,time,zlib
from pathlib import Path
import numpy as np
from PIL import Image
HEADER=struct.Struct('<14I')
FIELDS=['sample_id','raw_file','input_file','label','session_id','split']
CONFIG=dict(frontend='gesture_gray_v1',raw_width=320,raw_height=240,
            crop_x=40,crop_y=0,crop_size=240,input_width=64,input_height=64,
            resize='center_sampled_nearest',grayscale='rounded_arithmetic_rgb_mean',
            dtype='uint8',training_scale=255.0)

def exact(stream,n):
    data=bytearray();start=last=time.monotonic()
    while len(data)<n:
        now=time.monotonic()
        if now-last>15 or now-start>120:
            raise TimeoutError(f'Incomplete packet: {len(data):,}/{n:,} bytes. Reset board and recapture.')
        chunk=stream.read(min(4096,n-len(data)))
        if chunk:data.extend(chunk);last=time.monotonic()
        elif not hasattr(stream,'timeout'):raise EOFError('Truncated packet')
    return bytes(data)

def receive(stream,status=print):
    prefix=bytearray();start=time.monotonic()
    while True:
        if time.monotonic()-start>30:raise TimeoutError('No packet header')
        prefix.extend(exact(stream,1))
        if prefix.endswith(b'GST7'):break
        if prefix.endswith(b'\n'):
            line=prefix.decode(errors='replace').strip();status(line);prefix.clear()
            if line.startswith('# ERROR'):raise RuntimeError(line)
        if len(prefix)>1024:raise ValueError('Unexpected serial bytes; reset board')
    v,seq,rw,rh,iw,ih,cx,cy,cs,jb,gb,cu,pu,crc=HEADER.unpack(exact(stream,HEADER.size))
    if (v,rw,rh,iw,ih,cx,cy,cs,gb)!=(1,320,240,64,64,40,0,240,4096) or not 4<=jb<=300000:
        raise ValueError('Unexpected packet configuration/length')
    payload=exact(stream,jb+gb)
    if zlib.crc32(payload)!=crc:raise ValueError('CRC mismatch; sample NOT saved')
    jpg=payload[:jb]
    with Image.open(io.BytesIO(jpg)) as im:
        if im.format!='JPEG' or im.size!=(320,240):raise ValueError('Unexpected JPEG')
        im.load()
    gray=np.frombuffer(payload[jb:],dtype=np.uint8).reshape(64,64).copy()
    return jpg,gray,dict(sequence=seq,capture_us=cu,preprocess_us=pu,crc32=crc)

def ensure_config(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=True);path=root/'dataset_config.json'
    if path.exists():
        if json.loads(path.read_text())!=CONFIG:raise ValueError('Dataset frontend mismatch; use another directory')
    else:path.write_text(json.dumps(CONFIG,indent=2))

def save_pair(root,jpg,gray,label,session,split,meta):
    root=Path(root);ensure_config(root)
    label=label.strip()
    if label and not re.fullmatch('[a-z0-9_-]+',label):raise ValueError('Use lowercase letters, digits, _ or - in labels')
    for sub in ['raw','inputs','previews','metadata']:(root/sub).mkdir(exist_ok=True)
    prefix=f'{label or "unlabeled"}_{split}_';pattern=re.compile(re.escape(prefix)+r'(\d+)$')
    used=[]
    for sub in ['raw','inputs','previews','metadata']:
        for p in (root/sub).iterdir():
            m=pattern.fullmatch(p.stem)
            if m:used.append(int(m.group(1)))
    csvpath=root/'labels.csv'
    if csvpath.exists():
        with csvpath.open(newline='',encoding='utf-8-sig') as f:
            for r in csv.DictReader(f):
                m=pattern.fullmatch(r['sample_id'])
                if m:used.append(int(m.group(1)))
    sid=f'{prefix}{max(used,default=0)+1:02d}'
    raw=Path('raw')/(sid+'.jpg');inp=Path('inputs')/(sid+'.npy')
    (root/raw).write_bytes(jpg)
    np.save(root/inp,np.asarray(gray,dtype=np.uint8),allow_pickle=False)
    Image.fromarray(gray).save(root/'previews'/(sid+'.png'))
    (root/'metadata'/(sid+'.json')).write_text(json.dumps(meta,indent=2))
    existed=csvpath.exists()
    with csvpath.open('a',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=FIELDS)
        if not existed:w.writeheader()
        w.writerow(dict(zip(FIELDS,[sid,raw.as_posix(),inp.as_posix(),label,session,split])))
    return sid
