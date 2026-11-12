"""Count saved examples and inspect a JPEG alongside its board-produced input."""
import argparse
import csv
from collections import Counter
from pathlib import Path

import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dataset', type=Path, default=Path('gesture_dataset'))
    parser.add_argument('--sample', help='sample_id from labels.csv; defaults to the first row')
    args = parser.parse_args()
    with (args.dataset / 'labels.csv').open(newline='', encoding='utf-8-sig') as f:
        rows = list(csv.DictReader(f))
    if not rows:
        parser.error('No saved samples in labels.csv')
    counts = Counter((r['label'], r['split']) for r in rows)
    print(f'{"label":20} {"train":>7} {"val":>7} {"test":>7} {"total":>7}')
    for label in sorted({r['label'] for r in rows}):
        values = [counts[label, split] for split in ('train', 'val', 'test')]
        print(f'{label:20} {values[0]:7d} {values[1]:7d} {values[2]:7d} {sum(values):7d}')
    selected = next((r for r in rows if r['sample_id'] == args.sample), None) if args.sample else rows[0]
    if selected is None:
        parser.error('sample_id was not found in labels.csv')
    print('Selected row:', selected)
    gray = np.load(args.dataset / selected['input_file'], allow_pickle=False)
    print('Input:', gray.shape, gray.dtype, 'range', int(gray.min()), int(gray.max()))
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    fig, axes = plt.subplots(1, 2, figsize=(9, 4))
    with Image.open(args.dataset / selected['raw_file']) as original:
        axes[0].imshow(original)
    axes[0].add_patch(Rectangle((40, 0), 240, 240, fill=False, edgecolor='lime', linewidth=2))
    axes[0].set_title('Saved JPEG and model crop')
    axes[1].imshow(gray, cmap='gray', vmin=0, vmax=255)
    axes[1].set_title('Saved board input 64 x 64')
    for axis in axes:
        axis.axis('off')
    fig.suptitle(f"{selected['sample_id']} | {selected['label']} | {selected['session_id']} | {selected['split']}")
    fig.tight_layout()
    plt.show()


if __name__ == '__main__':
    main()
