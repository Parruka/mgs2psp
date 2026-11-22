"""mix.py: the free-play session for the PSP timing build (v25f): 2 minutes of ordinary play from the fork at vb 30500:
idle 10 s, walk the deck 36 s (tour1's four directions), crawl 8 s, punch and fire 8 s, first-person look 12 s, walk into the guards' view
(tour2's route; alert) 46 s.  Also as x86 GI slices of 500 vblanks (mix_a..mix_l) to relate the PSP's times to instruction counts."""
import sys
sys.path.insert(0, '/home/claude/play')
from scen import *
F = 30500
P = []
o = 500                                                           # 0-500 idle
for d in (U, R, D, L): P.append((o, 450, d)); o += 450            # 500-2300 walk
P += [(2300, 6, X), (2340, 360, U), (2700, 6, X)]                 # 2300-2700 crawl (X down, walk, X up)
P += taps(O, 2720, 30, 6) + [(2900, 120, SQ), (3020, 30, 0)]      # punches, then fire (hold square)
P += [(3100, 600, R1), (3150, 100, L), (3300, 100, R), (3450, 100, U), (3550, 100, D)]   # first person: look around
o = 3700
for d in (L, U, R, D): P.append((o, 575, d)); o += 575            # 3700-6000 tour2's route (alert)
if __name__ == '__main__':
    print(line('mix', F, 6000, P, cap_every=500))
