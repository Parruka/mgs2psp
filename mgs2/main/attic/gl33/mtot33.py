#!/usr/bin/env python3
"""mtot32.py OUT-PREFIX-DIR BASE [TAG ...]   (gl32 step 6; run with python3 -I)
The totals of PspProf windows (scripts/pgo.sh model: the '# total' line of each window's file) for several runs, a picture (40 pictures
a window of 80 vblanks), with the port's cost model ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 [C model],
and each run's difference from BASE.  TAG is NAME or NAME=suffixes (e.g. g32r0=,.1 for a run whose first two windows are the rain's).
'all' windows count waits too: only differences mean anything.  Not a PSP time."""
import sys
d = sys.argv[1]; tags = sys.argv[2:]
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def tot(p):
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 40, int(k['imiss']) / 40, int(k['dmiss']) / 40, int(k['fa_imiss']) / 40]
W = sys.stdin.read().strip().split('\n')   # the windows' names, one a line, in the files' order
R = {}
for t in tags:
    n, _, s = t.partition('='); suf = s.split(',') if _ else ['', '.1', '.2', '.3'][:len(W)]
    R[n] = [tot('%s/%s%s' % (d, n, x)) for x in suf]
names = list(R); b = names[0]
for i, w in enumerate(W):
    print(w)
    for n in names:
        x = R[n][i]; y = R[b][i]
        print('   %-7s %9.0f instr %7.0f I-miss (fully associative %7.0f) %7.0f D-miss = %7.2f ms a picture; against %s %+7.2f ms (instr %+8.0f, I-miss %+6.0f, D-miss %+6.0f)' % (
            n, x[0], x[1], x[3], x[2], ms(x), b, ms(x) - ms(y), x[0] - y[0], x[1] - y[1], x[2] - y[2]))
