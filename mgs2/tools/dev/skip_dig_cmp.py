#!/usr/bin/env python3
"""skip_dig_cmp.py <all frames digest> <shown frames only digest>: the translator's digest lines (xldig: c per call, v per vblank) of every
vblank the skip run translated must equal the all-frames run's lines of that vblank"""
import sys, collections
def load(fn):
    d = collections.defaultdict(list)
    for l in open(fn):
        p = l.split(None, 2)
        if len(p) >= 2 and p[0] in ('c', 'v'): d[int(p[1])].append(l)
    return d
A, S = load(sys.argv[1]), load(sys.argv[2])
shown = [vb for vb in S if not S[vb][-1].startswith('v %d calls 0 prims 0 ' % vb)]
skipped = [vb for vb in S if vb not in shown]
eq = sum(1 for vb in shown if A.get(vb) == S[vb]); lines = sum(len(S[vb]) for vb in shown)
calls_skipped = sum(len(A.get(vb, [])) - 1 for vb in skipped)
print('%s: %d vblanks translated, %d equal (%d lines); %d skipped (all-frames run: %d calls in them); vblanks only in one run: %d' % (
    sys.argv[2].split('/')[-1], len(shown), eq, lines, len(skipped), calls_skipped, len(set(A) ^ set(S))))
for vb in shown:
    if A.get(vb) != S[vb]: print('  differ at vb', vb); break
