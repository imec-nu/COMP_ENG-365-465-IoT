"""Request two-second clips and save board-computed features plus original WAV."""
import argparse,time
from pathlib import Path
from common import receive,save_pair,ensure_config

def check_session_split(rows, session, split):
    # TODO 1.2: Each row has session_id and split fields.
    # Reject a row whose session_id equals session but whose split differs
    # from split: raise ValueError with a useful message. Otherwise return.
    # For example, train_01 already in train cannot later be used for test.
    raise NotImplementedError('TODO 1.2: check the session split')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port');p.add_argument('--list-ports',action='store_true')
    p.add_argument('--output',type=Path,default=Path('dataset'))
    p.add_argument('--label',default='');p.add_argument('--session')
    p.add_argument('--split',choices=['train','val','test'],default='train')
    p.add_argument('--count',type=int,default=10)
    a=p.parse_args()
    import serial
    from serial.tools import list_ports
    if a.list_ports:
        for port in list_ports.comports(): print(port.device,port.description)
        return
    if not a.port or not a.session:p.error('--port and --session are required')
    if a.count<1:p.error('--count must be positive')
    ensure_config(a.output)
    import csv
    rows = []
    if (a.output / 'labels.csv').exists():
        with (a.output / 'labels.csv').open(newline='', encoding='utf-8-sig') as f:
            rows = list(csv.DictReader(f))
    check_session_split(rows, a.session, a.split)
    with serial.Serial(a.port,115200,timeout=0.2,write_timeout=5) as s:
        time.sleep(2);s.reset_input_buffer();s.write(b'PING\n')
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            line=s.readline().decode(errors='replace').strip()
            if line.startswith('# ERROR'):raise RuntimeError(line)
            if line.startswith('# READY KWS7'):break
        else:raise TimeoutError('No READY response. Check firmware, port and PSRAM.')
        for i in range(a.count):
            input(f'[{i+1}/{a.count}] label={a.label or "UNLABELED"}. Enter to arm; speak after RECORDING: ')
            s.write(b'CAPTURE\n')
            pcm,x,meta=receive(s,lambda msg:print(msg,flush=True))
            sid=save_pair(a.output,pcm,x,a.label,a.session,a.split,meta)
            print(f'[SAVED] {sid} WAV + {x.shape} features; frontend {meta["frontend_us"]/1000:.1f} ms')
if __name__=='__main__':
    try:main()
    except KeyboardInterrupt: print('\nStopped. Previously saved pairs are retained.')
