#!/usr/bin/env python3
"""dumpdiff.py <dump a> <dump b> [max lines]: the 32-bit words that differ, as 'address a-word b-word' (little-endian), plus a range summary"""
import sys, struct
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read(); mx = int(sys.argv[3]) if len(sys.argv) > 3 else 400
n = min(len(a), len(b)); print('sizes', len(a), len(b))
d = []
step = 1 << 16
for o in range(0, n, step):
    if a[o:o + step] != b[o:o + step]:
        for w in range(o, min(o + step, n), 4):
            if a[w:w + 4] != b[w:w + 4]: d.append(w)
print('differing words', len(d))
rs = []
for w in d:
    if rs and w - rs[-1][1] <= 64: rs[-1][1] = w; rs[-1][2] += 1
    else: rs.append([w, w, 1])
for lo, hi, k in rs: print('range %07x-%07x %d words' % (lo, hi + 3, k))
for w in d[:mx]: print('%07x %08x %08x' % (w, struct.unpack_from('<I', a, w)[0], struct.unpack_from('<I', b, w)[0]))
