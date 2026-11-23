#!/usr/bin/env python3
"""e6sim2.py LOG A B NLEV (gl41s, investigation only): E6c, the multi-level suffix-state cache, simulated as e6sim.py: levels = the
distinct dirty ends rounded up to 16 (at most NLEV, the one used longest ago replaced; plus 1024 = the final hash).  A level's entries are dropped
when a dirty end above it is written.  A rehash hashes up to the first level >= its end that holds its state, then fills the levels
it passed with the final hash.  Prints words hashed a rehash."""
import sys
p, a, b, NL = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]); POL = sys.argv[5] if len(sys.argv) > 5 else 'top'
seq = []; on = False
for l in open(p, errors='replace'):
    if l.startswith('gl41 levers vb '):
        vb = int(l.split()[3].rstrip(':')); on = a <= vb < b
    elif on and l.startswith('e6tr '):
        f = l.split(); seq.append((f[2], int(f[3]), int(f[4]), int(f[5])))
up = lambda x: min(1024, (x + 15) // 16 * 16)
lev = {}   # level -> set of final hashes (stand-in for states: one-to-one at a level while its suffix is unwritten)
words = 0; hit = 0; ckw = 0; use = {}; t = 0
for h, lo, hi, base in seq:
    t += 1
    if base < 0:
        lev = {}
    L = up(hi)
    for k in [k for k in lev if k < hi]:
        lev[k] = set()   # suffix written: entries dropped
    if POL == 'own' and L not in lev and L < 1024 and len(lev) >= NL and min(lev) > L:
        pass   # (vu0.h's E6: a level below all the levels is not added when full)
    elif L not in lev and L < 1024:
        if len(lev) >= NL:
            if POL in ('min', 'own'):
                del lev[min(lev)]
            else:   # 'top': the highest level stays (the backstop); of the others the one used longest ago goes
                del lev[min((x for x in lev if x != max(lev)), key=lambda x: use.get(x, -1))]
        lev[L] = set(); use[L] = t
    passed = []; found = False
    for k in sorted(x for x in lev if x >= hi):
        passed.append(k); ckw += 1
        if h in lev[k]:
            words += k; found = True; hit += 1; use[k] = t; break
    if not found:
        words += 1024
    for k in passed:
        lev[k].add(h)
n = len(seq)
print(f'vb {a}-{b} levels {NL} {POL}: {n} rehashes; hits {hit / n:.1%}; words hashed a rehash {words / n:.0f} of 1024 (saved {1 - words / n / 1024:.1%}); level checks a rehash {ckw / n:.2f}')
