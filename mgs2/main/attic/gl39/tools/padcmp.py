#!/usr/bin/env python3
"""padcmp.py TAG... (python3 -I): the padded relinks' windows (delivery cfg: 29,510 K0 digests on, 29,610 K0 digest-off, 29,710 K7) against the
unpadded build's (model/ka39, .1, .2): whole windows, ms a PSP step [C model] and the I- and D-misses a step."""
import os, sys
R = '/home/claude/main/build_gl39/model/'
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def head(p):
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 80.0, int(k['imiss']) / 80.0, int(k['dmiss']) / 80.0]
for i, n in enumerate(['29,510 K0 digests on', '29,610 K0 digest-off', '29,710 K7']):
    s = '' if i == 0 else '.%d' % i; b = head(R + 'ka39' + s)
    print('%-22s psp %7.2f (I %6.0f D %6.0f)' % (n, ms(b), b[1], b[2]), end='')
    for t in sys.argv[1:]:
        if os.path.isfile(R + t + s): x = head(R + t + s); print(' | %s %+5.2f (I %+5.0f D %+6.0f)' % (t, ms(x) - ms(b), x[1] - b[1], x[2] - b[2]), end='')
    print()
