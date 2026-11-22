"""batch5: random free-play sessions (fixed seeds) + a death / game over / continue session, as fork-server lines (fork at vb 30500)."""
import random, sys
sys.path.insert(0, '/home/claude/play')
from scen import *
F = 30500
DIRS = [0, U, D, L, R, U | L, U | R, D | L, D | R]
def rand_presses(seed, length):
    rnd = random.Random(seed); out = []; v = 10
    while v < length - 50:
        n = rnd.randint(40, 200); d = rnd.choice(DIRS)
        if d: out.append((v, n, d))
        for _ in range(rnd.randint(0, 3)):                       # actions during the segment
            k = rnd.choice([X, O, O, SQ, TR, TR, R1, L1, R2, L2, X | U])
            s = v + rnd.randint(0, n); h = rnd.choice([6, 6, 6, 30, 90]) if k in (SQ, R1, L1) else 6
            out.append((s, h, k))
        v += n + rnd.randint(0, 60)
    return out
lines = []
for i, length in [(1, 3000), (2, 3000), (3, 3000), (4, 3000), (5, 6000), (6, 6000)]:
    lines.append(line('rand%d' % i, F, length, rand_presses(1000 + i, length), cap_every=500 if length <= 3000 else 1000))
tour2 = [(10, 600, L), (610, 600, U), (1210, 600, R), (1810, 600, D)]
death = tour2 + taps(X, 4000, 300, 6) + taps(O, 4150, 300, 6)
lines.append(line('death', F, 6000, death, cap_every=500))
print('\n'.join(lines))
