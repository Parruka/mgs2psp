#!/usr/bin/env python3
"""digcmp34.py A.log B.log [label]  (gl34 G3; run with python3 -I): the frame digest lines of two logs by vblank ('picture: vb N buffer B
list L frame F: <counts>; dz <text>'): how many vblanks have digests in each and in both, and in how many of those the list digest, the
frame digest and the dz text differ (the frames differing listed in short), and from vb 24,001 on."""
import sys, re
def rd(p):
    D = {}
    for l in open(p, errors='replace'):
        m = re.match(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+):.*?; dz (.*)$', l.rstrip('\n'))
        if m: D.setdefault(int(m.group(1)), []).append((m.group(2), m.group(3), m.group(4).strip()))
    return D
A, B = rd(sys.argv[1]), rd(sys.argv[2]); lab = sys.argv[3] if len(sys.argv) > 3 else '%s against %s' % (sys.argv[1], sys.argv[2])
both = sorted(set(A) & set(B))
dl = [v for v in both if [x[0] for x in A[v]] != [x[0] for x in B[v]]]
df = [v for v in both if [x[1] for x in A[v]] != [x[1] for x in B[v]]]
dz = [v for v in both if [x[2] for x in A[v]] != [x[2] for x in B[v]]]
late = [v for v in both if v > 24000]; le = [v for v in late if A[v] == B[v]]
early = [v for v in df if v <= 3021]; oth = [v for v in df if v > 3021]
print('%s: vblanks with digests %d / %d, in both %d; LIST digest differs in %d%s; FRAME digest in %d (vb %s: %d up to 3,021, others %s); dz text '
      'in %d%s; from 24,001 on: %d of %d equal in all three' % (lab, len(A), len(B), len(both), len(dl), (' ' + str(dl[:8])) if dl else '', len(df),
      ('%d-%d' % (df[0], df[-1])) if df else '---', len(early), oth[:12], len(dz), (' ' + str(dz[:8])) if dz else '', len(le), len(late)))
