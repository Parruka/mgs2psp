#!/usr/bin/env python3
"""piccmp39.py A_DIR B_DIR (python3 -I; container only: the pictures are game data): the emulator's pictures of two runs (shot_*.bmp,
live_*.bmp), image data only, by the kind of gl39's turn the picture's vblank falls in (tools/sched39.py)."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import sched39
def px(p):
    d = open(p, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]; return d[off:]
A, B = sys.argv[1], sys.argv[2]; KN = {0: 'K0', 5: 'K5', 7: 'K7', 8: 'K8'}
eq = []; ne = []; miss = []
for n in sorted(os.listdir(B), key=lambda x: int(x.split('_')[1].split('.')[0])):
    vb = int(n.split('_')[1].split('.')[0]); t = (vb - 24000) // 100
    k = 'before' if vb < 24000 else KN[sched39.S[t][0]] if 0 <= t < sched39.N else 'after'
    if not os.path.isfile(os.path.join(A, n)): miss.append((n, k)); continue
    a, b = px(os.path.join(A, n)), px(os.path.join(B, n))
    if a == b: eq.append((n, k)); continue
    d = [abs(x - y) for x, y in zip(a, b)]; npx = sum(1 for i in range(0, len(d) - 2, 3) if d[i] or d[i + 1] or d[i + 2]) if len(a) == len(b) else -1
    ne.append((n, k, npx, max(d) if d else -1))
print('%s against %s: %d pictures, equal %d, differing %d, not in A %d' % (B, A, len(eq) + len(ne) + len(miss), len(eq), len(ne), len(miss)))
import collections
print('  equal by kind: %s' % dict(collections.Counter(k for _, k in eq)))
for n, k, npx, mx in ne: print('  DIFFERS %-16s %-6s %7d px, at most %d a channel' % (n, k, npx, mx))
if miss: print('  not in A: %s' % ' '.join(n for n, _ in miss))
