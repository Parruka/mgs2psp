#!/usr/bin/env python3
"""pairmean36.py MODEL-DIR BASE NEW [BASE2 NEW2 ...] (gl36, look b's scoring; python3 -I): each scene's two windows -- an ME turn and the
main turn after it, which alternate on the PSP -- as one figure: the pair's mean ms a step [C model], NEW against BASE.  A pair is given
as TAG:SUFFIX_ME,SUFFIX_MAIN, e.g. g35pc2:,.1 (the steady deck d01t: 27,410-27,490 ME, 27,510-27,590 main) or g35p:.2,.3 (the walk).
Model: ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 a picture, 40 pictures a window."""
import sys
d = sys.argv[1]; a = sys.argv[2:]
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def tot(p):
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 40, int(k['imiss']) / 40, int(k['dmiss']) / 40]
def pair(s):
    t, _, x = s.partition(':'); sm, sn = x.split(','); return t, [tot('%s/%s%s' % (d, t, sm)), tot('%s/%s%s' % (d, t, sn))]
for i in range(0, len(a), 2):
    (tb, B), (tn, N) = pair(a[i]), pair(a[i + 1])
    dm = [ms(N[k]) - ms(B[k]) for k in (0, 1)]
    di = [N[k][1] - B[k][1] for k in (0, 1)]; dd = [N[k][2] - B[k][2] for k in (0, 1)]; dn = [N[k][0] - B[k][0] for k in (0, 1)]
    print('%-14s -> %-14s ME turn %+6.2f, main turn %+6.2f, PAIR MEAN %+6.2f ms a step [C model]  (pair means: instr %+7.0f, I-miss %+6.0f, D-miss %+6.0f)' % (
        a[i], a[i + 1], dm[0], dm[1], (dm[0] + dm[1]) / 2, (dn[0] + dn[1]) / 2, (di[0] + di[1]) / 2, (dd[0] + dd[1]) / 2))
