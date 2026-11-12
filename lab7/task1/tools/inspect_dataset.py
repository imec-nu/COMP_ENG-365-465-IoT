"""Show saved keyword counts and plot one board-computed feature matrix."""
import argparse
import csv
from collections import Counter
from pathlib import Path
import wave
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dataset', type=Path, default=Path('dataset'))
    parser.add_argument('--sample', help='Sample ID from labels.csv; default: first sample')
    args = parser.parse_args()
    with (args.dataset / 'labels.csv').open(newline='', encoding='utf-8-sig') as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        parser.error('labels.csv contains no samples')
    counts = Counter((row['label'], row['split']) for row in rows)
    print(f'{"label":20s} {"train":>7s} {"val":>7s} {"test":>7s} {"total":>7s}')
    for label in sorted({row['label'] for row in rows}):
        values = [counts[label, split] for split in ('train', 'val', 'test')]
        print(f'{label:20s}' + ''.join(f'{n:8d}' for n in values + [sum(values)]))
    invalid = []
    for row in rows:
        x = np.load(args.dataset / row['input_file'], allow_pickle=False)
        if x.shape != (124, 32) or x.dtype != np.float32 or not np.isfinite(x).all():
            invalid.append(row['sample_id'])
    if invalid:
        parser.error('Invalid feature matrices: ' + ', '.join(invalid))
    print(f'Checked {len(rows)} feature matrices: all float32 with shape (124, 32).')
    matches = [row for row in rows if args.sample is None or row['sample_id'] == args.sample]
    if not matches:
        parser.error('Sample ID not found in labels.csv')
    row = matches[0]
    print('\nSelected sample')
    for key, value in row.items():
        print(f'{key}: {value}')
    wav_path = args.dataset / row['raw_file']
    with wave.open(str(wav_path), 'rb') as wav:
        print(f'WAV: {wav.getnframes()} samples, {wav.getframerate()} Hz, '
              f'{wav.getnframes() / wav.getframerate():.3f} seconds, '
              f'{wav.getnchannels()} channel, {wav.getsampwidth() * 8} bits')
    print(f'Listen to: {wav_path.resolve()}')
    x = np.load(args.dataset / row['input_file'], allow_pickle=False)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    figure, axis = plt.subplots(figsize=(9, 4))
    image = axis.imshow(x.T, origin='lower', aspect='auto', extent=(0, 2, 0, 32))
    axis.set(xlabel='Time (seconds)', ylabel='Mel band', title=f'{row["sample_id"]}: {row["label"]}')
    figure.colorbar(image, ax=axis, label='Log energy')
    figure.tight_layout()
    output = args.dataset / 'inspection'
    output.mkdir(exist_ok=True)
    path = output / (row['sample_id'] + '_features.png')
    figure.savefig(path, dpi=150)
    plt.close(figure)
    print(f'Feature plot: {path.resolve()}')


if __name__ == '__main__':
    main()
