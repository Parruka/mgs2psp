#!/usr/bin/env python3
"""cmp.py <log a> <log b>: the picture lines of two twin runs compared per picture, by window: "picture: vb N buffer d list X frame Y",
"picture: vb N content Z" and (n0 builds) "natb: vb N df0 K state .. mat .. xf .. vert .. strip .. other M H".  Prints per window the
pictures in both, those whose list / frame / content digests differ, and for natb the pictures and df0 batches differing by field."""
import sys, collections
W = [('opening (vb < 24000)', 0, 23999), ('deck w00a (24000-31400)', 24000, 31400), ('  of it the rain (29300-31400)', 29300, 31400),
     ('w01a (31401-37800)', 31401, 37800), ('all', 0, 1 << 30)]
def load(fn):
    pic, con, nb = {}, {}, {}
    for l in open(fn, errors='replace'):
        if l.startswith('picture: vb '):
            p = l.split(); vb = int(p[2])
            if p[3] == 'buffer': pic[vb] = (p[6], p[8].rstrip(':'), ' '.join(p[9:]))
            elif p[3] == 'content': con[vb] = p[4]
        elif l.startswith('natb: vb '):
            p = l.split(); vb = int(p[2])
            nb[vb] = dict(df0=int(p[4]), state=p[6], mat=p[8], xf=p[10], vert=p[12], strip=p[14], other=(p[16], p[17]))
    return pic, con, nb
a = load(sys.argv[1]); b = load(sys.argv[2])
for name, lo, hi in W:
    ks = [v for v in a[0] if lo <= v <= hi and v in b[0]]
    only = len([v for v in a[0] if lo <= v <= hi and v not in b[0]]) + len([v for v in b[0] if lo <= v <= hi and v not in a[0]])
    dl = sum(a[0][v][0] != b[0][v][0] for v in ks); df = sum(a[0][v][1] != b[0][v][1] for v in ks)
    dc = sum(a[1].get(v) != b[1].get(v) for v in ks)
    line = '%-32s pictures %6d (in one run only %d): list differs %d, frame differs %d, content differs %d' % (name, len(ks), only, dl, df, dc)
    nk = [v for v in ks if v in a[2] and v in b[2]]
    if nk:
        n0 = sum(a[2][v]['df0'] for v in nk); fd = collections.Counter(); bd = collections.Counter(); cnt = sum(a[2][v]['df0'] != b[2][v]['df0'] for v in nk)
        for v in nk:
            for f in ('state', 'mat', 'xf', 'vert', 'strip', 'other'):
                if a[2][v][f] != b[2][v][f]: fd[f] += 1; bd[f] += a[2][v]['df0']
        line += '; df0 batches %d (count differs in %d pictures); pictures differing by field: %s' % (n0, cnt, dict(fd) if fd else 'none')
        if fd: line += ' (their df0 batches %s)' % dict(bd)
    print(line)
    if name == 'all':
        bad = [v for v in ks if a[0][v][1] != b[0][v][1]][:10]
        for v in bad: print('  first differing: vb %d %s / %s' % (v, a[0][v], b[0][v]))
