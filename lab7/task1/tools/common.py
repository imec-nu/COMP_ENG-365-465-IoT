"""Wire protocol and dataset persistence. No PC feature extraction in collection."""
import csv, json, struct, time, wave, zlib
from pathlib import Path
import numpy as np
import re

HEADER = struct.Struct('<12I')
FIELDS = ['sample_id','raw_file','input_file','label','session_id','split']
CONFIG = dict(frontend='logmel_v1', sample_rate=16000, samples=32000,
              n_fft=512, hop=256, frames=124, mel_bands=32,
              fmin=80, fmax=7600, log_floor=1e-12)

def exact(stream, n, deadline):
    data=bytearray()
    while len(data)<n:
        if time.monotonic()>deadline: raise TimeoutError('Incomplete packet; reconnect and recapture.')
        chunk=stream.read(n-len(data))
        if chunk: data.extend(chunk)
        elif not hasattr(stream,'timeout'): raise EOFError('Truncated packet')
    return bytes(data)

def receive(stream, status=print):
    deadline=time.monotonic()+30
    # Text lines precede binary magic; read one byte at a time to avoid overread.
    prefix=bytearray()
    while True:
        prefix.extend(exact(stream,1,deadline))
        if prefix.endswith(b'KWS7'): break
        if prefix.endswith(b'\n'):
            line=prefix.decode('utf-8',errors='replace').strip(); status(line)
            if line.startswith('# ERROR'): raise RuntimeError(line)
            prefix.clear()
        if len(prefix)>1024: raise ValueError('Unexpected serial data; reset board.')
    h=HEADER.unpack(exact(stream,HEADER.size,deadline))
    ver,seq,sr,n,nfft,hop,frames,mels,ab,fb,us,crc=h
    if (ver,sr,n,nfft,hop,frames,mels,ab,fb)!=(1,16000,32000,512,256,124,32,64000,15872):
        raise ValueError('Firmware/header configuration mismatch')
    payload=exact(stream,ab+fb,deadline)
    if zlib.crc32(payload)!=crc: raise ValueError('CRC mismatch; sample was not saved')
    pcm=np.frombuffer(payload[:ab],dtype='<i2').copy()
    x=np.frombuffer(payload[ab:],dtype='<f4').reshape(frames,mels).copy()
    if not np.isfinite(x).all(): raise ValueError('Non-finite features')
    return pcm,x,dict(sequence=seq,frontend_us=us,crc32=crc)

def ensure_config(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=True)
    path=root/'dataset_config.json'
    if path.exists():
        if json.loads(path.read_text())!=CONFIG: raise ValueError('Different frontend in dataset')
    else: path.write_text(json.dumps(CONFIG,indent=2))

def save_pair(root,pcm,x,label,session,split,meta):
    root=Path(root);ensure_config(root)
    for sub in ('raw','inputs','metadata'): (root/sub).mkdir(exist_ok=True)
    # Use the keyword and dataset split in the filename.
    # Missing labels are saved with the prefix "unlabeled".
    file_label = label.strip() or "unlabeled"

    # Reject characters that are unsafe in Windows/macOS filenames.
    if not re.fullmatch(r"[A-Za-z0-9_-]+", file_label):
        raise ValueError(
            "Use only letters, numbers, underscores, or hyphens in labels."
        )

    prefix = f"{file_label}_{split}_"
    pattern = re.compile(rf"^{re.escape(prefix)}(\d+)$")

    # Continue numbering for this keyword and split.
    # Check all three directories to avoid overwriting partially saved pairs.
    max_index = 0

    for folder, suffix in (
        ("raw", ".wav"),
        ("inputs", ".npy"),
        ("metadata", ".json"),
    ):
        for path in (root / folder).glob(f"{prefix}*{suffix}"):
            match = pattern.fullmatch(path.stem)
            if match:
                max_index = max(max_index, int(match.group(1)))

    sid = f"{prefix}{max_index + 1:02d}"
    raw=Path('raw')/(sid+'.wav'); inp=Path('inputs')/(sid+'.npy')
    # CSV is committed last. Interrupted unlisted files are ignored by training.
    with wave.open(str(root/raw),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(16000)
        w.writeframes(np.asarray(pcm,dtype='<i2').tobytes())
    np.save(root/inp,np.asarray(x,dtype=np.float32),allow_pickle=False)
    (root/'metadata'/(sid+'.json')).write_text(json.dumps(meta,indent=2))
    row=dict(zip(FIELDS,[sid,raw.as_posix(),inp.as_posix(),label,session,split]))
    path=root/'labels.csv';exists=path.exists()
    with path.open('a',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=FIELDS)
        if not exists: writer.writeheader()
        writer.writerow(row)
    return sid
