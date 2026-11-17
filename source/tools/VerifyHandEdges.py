"""Catch a forearm spike where the posed hand meets the original arm."""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('atlas', nargs='?', type=Path,
                        default=ROOT/'assets/milk-frog/four-finger/hands-8k.png')
    args = parser.parse_args()
    atlas = Image.open(args.atlas).convert('RGBA')
    width, height = 1189, 669
    assert atlas.size == (width*4, height*8), atlas.size
    for mask in range(1, 16):
        x, y = mask % 4*width, mask // 4*height
        alpha = np.asarray(atlas.crop((x, y, x+width, y+height)))[:, :, 3]
        # Native x=751 corresponds to the old source x=890 patch boundary.
        # This whole lower-forearm arc must rise smoothly toward the elbow.
        for threshold in (32, 128, 224):
            edge = [np.flatnonzero(alpha[:, column] > threshold)[-1]
                    for column in range(734, 808)]
            steps = np.diff(edge)
            assert steps.max() <= 1 and steps.min() >= -2, (
                f'mask={mask}, alpha={threshold}, '
                f'edge step range={steps.min()}..{steps.max()}')
    print('PASS: continuous lower forearm in all 15 pressed poses at 3 alpha thresholds.')


if __name__ == '__main__':
    main()
