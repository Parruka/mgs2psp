#!/usr/bin/env python3
"""padcmp40.py TAG... (python3 -I; gl40): the pad candidates' guard windows (model40.sh g: gl38's guard cfg, 16,110 21,110 23,110
27,510) against gl39 (exp40/model/x39) and gl36 (x36): whole windows, ms a PSP step [C model], I- and D-misses a step."""
import os, sys
R = '/home/claude/main/build_gl40/model/'; X = '/home/claude/main/exp40/model/'
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def head(p):
    if not os.path.isfile(p): return None
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 80.0, int(k['imiss']) / 80.0, int(k['dmiss']) / 80.0]
W = ['16,110', '21,110', '23,110', '27,510']; suf = lambda i: '' if i == 0 else '.%d' % i
print('window  | gl36 x36 ms (I, D) | gl39 x39 ms (I, D) | candidates: ms, minus gl39, minus gl36 (I a step, D a step)')
tot = {}
for i, w in enumerate(W):
    a, b = head(X + 'x36' + suf(i)), head(X + 'x39' + suf(i))
    print('%-7s | %6.2f (%6.0f %6.0f) | %6.2f (%6.0f %6.0f) |' % (w, ms(a), a[1], a[2], ms(b), b[1], b[2]), end='')
    for t in sys.argv[1:]:
        c = head(R + t + suf(i))
        if c is None: print(' %s -' % t, end=''); continue
        tot.setdefault(t, []).append((ms(c) - ms(b), ms(c) - ms(a), c[1] <= 1.02 * a[1]))
        print(' | %s %6.2f %+5.2f %+5.2f (I %6.0f%s D %6.0f)' % (t, ms(c), ms(c) - ms(b), ms(c) - ms(a), c[1], '' if c[1] <= 1.02 * a[1] else ' OVER', c[2]), end='')
    print()
for t, L in tot.items():
    print('%-8s sum minus gl39 %+6.2f, minus gl36 %+6.2f; worst window minus gl39 %+5.2f; I-miss guard (<= gl36 + 2 %%) %d of %d' % (
        t, sum(x[0] for x in L), sum(x[1] for x in L), max(x[0] for x in L), sum(1 for x in L if x[2]), len(L)))
