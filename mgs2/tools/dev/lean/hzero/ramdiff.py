#!/usr/bin/env python3
"""ramdiff.py <dump A> <dump B>: the 32-bit words that differ between two EE RAM dumps, as ranges (and the first words of each)."""
import sys, struct
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read()
assert len(a) == len(b), (len(a), len(b))
n = len(a) // 4; wa = struct.unpack('<%dI' % n, a); wb = struct.unpack('<%dI' % n, b)
d = [i for i in range(n) if wa[i] != wb[i]]
print('%d of %d words differ' % (len(d), n))
rs = []
for i in d:
    if rs and i - rs[-1][1] <= 4: rs[-1][1] = i
    else: rs.append([i, i])
for s, e in rs[:40]:
    w = [i for i in range(s, e + 1) if wa[i] != wb[i]]
    print('%08x-%08x: %d words; ' % (4 * s, 4 * e + 3, len(w)) + ' '.join('%08x:%08x/%08x' % (4 * i, wa[i], wb[i]) for i in w[:6]))
if len(rs) > 40: print('... %d ranges' % len(rs))
