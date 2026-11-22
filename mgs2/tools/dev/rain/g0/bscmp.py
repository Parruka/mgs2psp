#!/usr/bin/env python3
"""g0cmp.py <log a> <log b>: two x86 twin runs compared line for line: hash lines (ee, iop, gs, gsd, vfb), time lines, picture lines
(list and frame digests, counts), and the native line_sph and body_sph reports; prints the first differences of each kind and the counts"""
import sys, re
def load(fn):
    H, T, P, N = {}, {}, {}, []
    for l in open(fn, errors='replace'):
        if l.startswith('hash: vb '):
            p = l.split(); v = int(p[2]); d = {}
            for i in range(3, len(p) - 1, 2): d[p[i]] = p[i + 1]
            H[v] = d
        elif l.startswith('time: vb '):
            p = l.split(); T[int(p[2])] = p[4]
        elif l.startswith('picture: vb '):
            m = re.match(r'picture: vb (\d+) buffer (\d+) list (\w+) frame (\w+): (.*)', l)
            if m: P.setdefault(int(m.group(1)), []).append((m.group(3), m.group(4), m.group(5).strip()))
        elif 'line_sph' in l or 'body_sph' in l or l.startswith('natchk'):
            N.append(l.strip())
    return H, T, P, N
a, b = load(sys.argv[1]), load(sys.argv[2])
for k in ('ee', 'iop', 'gs', 'gsd', 'vfb'):
    vs = sorted(set(a[0]) & set(b[0])); diff = [v for v in vs if a[0][v].get(k) != b[0][v].get(k)]
    print('%-4s %d hash lines in both (%d / %d), %d differ%s' % (k, len(vs), len(a[0]), len(b[0]), len(diff), (': first vb %d .. last %d' % (diff[0], diff[-1])) if diff else ''))
vs = sorted(set(a[1]) & set(b[1])); diff = [v for v in vs if a[1][v] != b[1][v]]
print('time %d lines in both (%d / %d), %d differ%s' % (len(vs), len(a[1]), len(b[1]), len(diff), (': first vb %d' % diff[0]) if diff else ''))
vs = sorted(set(a[2]) & set(b[2])); dl = [v for v in vs if [x[0] for x in a[2][v]] != [x[0] for x in b[2][v]]]
df = [v for v in vs if [x[1] for x in a[2][v]] != [x[1] for x in b[2][v]]]; dc = [v for v in vs if [x[2] for x in a[2][v]] != [x[2] for x in b[2][v]]]
print('picture %d vblanks in both (%d / %d): lists differ %d, frames differ %d, counts differ %d' % (len(vs), len(a[2]), len(b[2]), len(dl), len(df), len(dc)))
for v in (dl + df + dc)[:6]: print('  vb %d\n    a %s\n    b %s' % (v, a[2][v], b[2][v]))
print('a:', *a[3], sep='\n  '); print('b:', *b[3], sep='\n  ')
