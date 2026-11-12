"""Collect paired JPEG/64x64 NPY from ESP32S3. Preview: s saves, n discards, q quits."""
import argparse,csv,io,re,time
from pathlib import Path
from common import receive,save_pair,ensure_config

def review(jpg,gray):
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    from PIL import Image
    result={'key':'n'}
    fig,axes=plt.subplots(1,2,figsize=(9,4))
    axes[0].imshow(Image.open(io.BytesIO(jpg)))
    axes[0].add_patch(Rectangle((40,0),240,240,fill=False,edgecolor='lime',linewidth=2))
    axes[0].set_title('Original JPEG; green = model crop')
    axes[1].imshow(gray,cmap='gray',vmin=0,vmax=255);axes[1].set_title('ESP32 model input (64x64)')
    for a in axes:a.axis('off')
    fig.suptitle('Click this window: s = save, n = discard, q = quit. Closing discards.')
    def key(event):
        if event.key in ['s','n','q']:
            result['key']=event.key;plt.close(fig)
    fig.canvas.mpl_connect('key_press_event',key)
    # Disable matplotlib's own 's' save-dialog shortcut for this preview.
    manager=plt.get_current_fig_manager()
    if hasattr(manager,'key_press_handler_id'):fig.canvas.mpl_disconnect(manager.key_press_handler_id)
    plt.tight_layout();plt.show()
    return result['key']

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port');p.add_argument('--list-ports',action='store_true')
    p.add_argument('--output',type=Path,default=Path('gesture_dataset'))
    p.add_argument('--label',default='');p.add_argument('--session')
    p.add_argument('--split',choices=['train','val','test'],default='train')
    p.add_argument('--count',type=int,default=10);p.add_argument('--delay',type=float,default=2)
    p.add_argument('--no-preview',action='store_true',help='Save valid packets immediately; inspect saved JPG/PNG later')
    a=p.parse_args()
    import serial
    from serial.tools import list_ports
    if a.list_ports:
        for port in list_ports.comports():print(port.device,port.description)
        return
    if not a.port or not a.session:p.error('--port and --session required')
    a.label=a.label.strip()
    if a.label and not re.fullmatch('[a-z0-9_-]+',a.label):p.error('Labels: lowercase letters/digits/_/- only')
    if a.count<1 or not 0<=a.delay<=30:p.error('count >= 1 and delay between 0 and 30 seconds')
    ensure_config(a.output)
    if (a.output/'labels.csv').exists():
        with (a.output/'labels.csv').open(newline='',encoding='utf-8-sig') as f:
            if any(r['session_id']==a.session and r['split']!=a.split for r in csv.DictReader(f)):
                p.error('This session is already assigned to another split')
    with serial.Serial(a.port,115200,timeout=.2,write_timeout=5) as s:
        time.sleep(2);s.reset_input_buffer();s.write(b'PING\n');deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            line=s.readline().decode(errors='replace').strip()
            if line.startswith('# ERROR'):raise RuntimeError(line)
            if line.startswith('# READY GST7'):break
        else:raise TimeoutError('No GST7 READY. Check board sketch, port and OPI PSRAM.')
        saved=0
        while saved<a.count:
            input(f'[{saved+1}/{a.count}] label={a.label or "UNLABELED"}. Enter to arm: ')
            print(f'Position hand in center; capturing in {a.delay:g} seconds...',flush=True)
            time.sleep(a.delay);s.write(b'CAPTURE\n')
            jpg,gray,meta=receive(s,lambda msg:print(msg,flush=True))
            choice='s' if a.no_preview else review(jpg,gray)
            if choice=='q':break
            if choice!='s':print('[DISCARDED] Try another pose.');continue
            sid=save_pair(a.output,jpg,gray,a.label,a.session,a.split,meta);saved+=1
            print(f'[SAVED] {sid} JPG + NPY + PNG; preprocess {meta["preprocess_us"]/1000:.1f} ms')
if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:print('\nStopped; saved samples retained.')
