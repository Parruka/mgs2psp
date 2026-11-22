#!/usr/bin/env python3
"""piccmp.py: a PSP picture (480 x 272 or a 512-stride BMP / PPM) against a gsref PS2 frame buffer (512 x 512 PPM), renderer_notes 11.8.
Each PSP pixel is compared with the closest of the 3 x 3 PS2 pixels around the point it samples (the 512 x 512 to 480 x 272 squeeze
alone puts 18% of pixels over 16 levels with a one-pixel comparison); the difference of two pixels is the largest of their channel
differences.  Prints the mean difference in levels and the shares of pixels over 16 and over 32 levels.
  piccmp.py <psp.bmp> <ps2.ppm> [label]     one comparison
  piccmp.py --same <a.bmp> <b.bmp> [label]  two PSP pictures, pixel for pixel
Pictures are game data: container only."""
import sys
import numpy as np
from PIL import Image

W, H = 480, 272


def load_psp(p):
    return np.asarray(Image.open(p).convert('RGB'))[:H, :W].astype(np.int16)


def cmp_ps2(a, ref):
    ref = ref.astype(np.int16)
    cy = np.minimum(511, ((np.arange(H) + 0.5) * 512 / 272).astype(int))
    cx = np.minimum(511, ((np.arange(W) + 0.5) * 512 / 480).astype(int))
    best = None
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            r = ref[np.clip(cy + dy, 0, 511)][:, np.clip(cx + dx, 0, 511)]
            d = np.abs(a - r).max(axis=2)
            best = d if best is None else np.minimum(best, d)
    return best


def report(label, d):
    print('%-44s mean %.2f levels, %.2f%% over 16, %.2f%% over 32' % (label, d.mean(), 100 * (d > 16).mean(), 100 * (d > 32).mean()))


if __name__ == '__main__':
    if sys.argv[1] == '--same':
        a, b = load_psp(sys.argv[2]), load_psp(sys.argv[3])
        d = np.abs(a - b).max(axis=2)
        report(sys.argv[4] if len(sys.argv) > 4 else 'same size', d)
        print('  pixels that differ: %d' % (d > 0).sum())
    else:
        report(sys.argv[3] if len(sys.argv) > 3 else sys.argv[1], cmp_ps2(load_psp(sys.argv[1]), np.asarray(Image.open(sys.argv[2]).convert('RGB'))))
