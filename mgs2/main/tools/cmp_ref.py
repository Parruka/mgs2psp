#!/usr/bin/env python3
# cmp_ref.py <ref hashref txt> <log A (base)> <log B> [<out hashref for B>]: gl18's x86 references (task #179).  Per log: hash lines
# (ee raw, iop, gs = the masked hash, vfb), time lines, picture lines (list, frame, content per vblank).  A's ee against the given
# reference file; B against A: masked hash, iop, time and every picture digest; B's raw ee written as "<vb> <ee> <vfb>" if asked.
import sys, re, gzip
def rd(fn):
    op = gzip.open if fn.endswith('.gz') else open
    L = op(fn, 'rt', errors='replace').read().split('\n'); out = []
    for ln in L:
        if ln.startswith(' gs ') and out and out[-1].startswith('hash: vb'):
            out[-1] = re.sub(r'gs mask:.*$', '', out[-1]) + ln
        else: out.append(ln)
    H = {}; T = {}; P = {}
    for ln in out:
        m = re.match(r'hash: vb (\d+) ee (\w+) iop (\w+)\s*gs (\w+) gsd (\d+) vfb (\d+)', ln)
        if m: H[int(m.group(1))] = (m.group(2), m.group(3), m.group(4), m.group(6)); continue
        m = re.match(r'time: vb (\d+) k_time (\d+)', ln)
        if m: T[int(m.group(1))] = m.group(2); continue
        m = re.match(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+):', ln)
        if m: P.setdefault(int(m.group(1)), []).append(('lf', m.group(2), m.group(3))); continue
        m = re.match(r'picture: vb (\d+) content (\w+)', ln)
        if m: P.setdefault(int(m.group(1)), []).append(('c', m.group(2)))
    return H, T, P, [l for l in out if l.startswith('native rain:') or l.startswith('h3r rain:') or 'g_opt2' in l][:8]
ref = {}
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 2: ref[int(p[0])] = (p[1], p[2] if len(p) > 2 else None)
A = rd(sys.argv[2]); B = rd(sys.argv[3])
for nm, X in (('A', A), ('B', B)):
    print('%s: %d hashes, %d time lines, %d picture vblanks (%d lines)' % (nm, len(X[0]), len(X[1]), len(X[2]), sum(len(v) for v in X[2].values())))
    for l in X[3]: print('   ', l[:200])
n = d = 0; first = None
for vb, (ee, iop, gs, vfb) in sorted(A[0].items()):
    if vb in ref:
        n += 1
        if ref[vb][0] != ee or (ref[vb][1] is not None and ref[vb][1] != vfb):
            d += 1; first = first or vb
print('A ee+vfb against %s: %d compared, %d differ (first %s); reference lines %d' % (sys.argv[1].split('/')[-1], n, d, first, len(ref)))
for k, name in ((0, 'ee raw'), (1, 'iop'), (2, 'gs (masked)'), (3, 'vfb')):
    ks = sorted(set(A[0]) | set(B[0])); dd = [vb for vb in ks if A[0].get(vb, (None,) * 4)[k] != B[0].get(vb, (None,) * 4)[k]]
    print('B vs A %-12s: %d hashes, %d differ%s' % (name, len(ks), len(dd), (' (first %d, last %d)' % (dd[0], dd[-1])) if dd else ''))
ks = sorted(set(A[1]) | set(B[1])); dd = [vb for vb in ks if A[1].get(vb) != B[1].get(vb)]
print('B vs A time        : %d lines, %d differ' % (len(ks), len(dd)))
ks = sorted(set(A[2]) | set(B[2])); dd = [vb for vb in ks if A[2].get(vb) != B[2].get(vb)]
print('B vs A pictures    : %d vblanks, %d differ%s' % (len(ks), len(dd), (' (first %d)' % dd[0]) if dd else ''))
if len(sys.argv) > 4:
    with open(sys.argv[4], 'w') as f:
        for vb, (ee, iop, gs, vfb) in sorted(B[0].items()): f.write('%d %s %s\n' % (vb, ee, vfb))
    print('wrote', sys.argv[4], len(B[0]), 'lines')
