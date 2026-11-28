"""cmp44.py A B [FROM] (gl44, python3 -I): two x86 twin arms of build_gl44/tw/x86 (out_<arm>.log.gz, seed_<arm>.txt.gz, act_<arm>.txt.gz) side
by side: the state hashes by part (ee, iop, gs, vfb), the picture lines (xlive per vblank, the digest 'frame' lines, the 'content' lines),
the seed log (sv: every act's address, actor, the three seeds and the time at entry, hashed per vblank; sd: each seed change with the act
that drew it; st: the splash acts' times) from vblank FROM (default 0), the act log's per-pass lines (P: pass, vblank, acts, a hash of
(act, seeds) and one of (act, seed, time)) and the host ticks per act call (T) of the rain's actors."""
import sys, gzip, re, hashlib
X = '/home/claude/main/build_gl44/tw/x86/'
a, b = sys.argv[1], sys.argv[2]; FROM = int(sys.argv[3]) if len(sys.argv) > 3 else 0
def op(p): return gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')
def path(k, t):
    import os
    for p in (X + '%s_%s.%s.gz' % (k, t, 'log' if k == 'out' else 'txt'), X + '%s_%s.%s' % (k, t, 'log' if k == 'out' else 'txt')):
        if os.path.exists(p): return p
    return sys.exit('cmp44: no %s for %s' % (k, t))
def out(t):
    H = {}; XL = {}; F = {}; C = {}; S = []
    for l in op(path('out', t)):
        if l.startswith('hash: vb '):
            w = l.split(); d = dict(zip(w[3::2], w[4::2])); H[int(w[2])] = d
        elif l.startswith('xlive: vb ') and l.split()[2].isdigit(): XL[int(l.split()[2])] = l.strip()
        elif l.startswith('picture: vb '):
            if ' list ' in l: F[int(l.split()[2])] = l.strip()
            elif ' content ' in l: C[int(l.split()[2])] = l.strip()
        elif l.startswith('gl44 study') or l.startswith('native line_sph') or l.startswith('native rain') or l.startswith('v44 turn'): S.append(l.strip())
    return H, XL, F, C, S
def seed(t):
    L = {'sv': [], 'sd': [], 'st': []}
    for l in op(path('seed', t)):
        w = l.split()
        if w and w[0] in L and int(w[1]) >= FROM: L[w[0]].append(l.strip())
    return L
def act(t):
    P = []; T = {}
    for l in op(path('act', t)):
        w = l.split()
        if w[0] == 'P' and int(w[2]) >= FROM: P.append(w[1:])
        elif w[0] == 'T': c, t = T.get(w[2], (0, 0)); T[w[2]] = (c + int(w[3]), t + int(w[4]))
    return P, T
def md5(L): return hashlib.md5('\n'.join(L).encode()).hexdigest()[:12]
(Ha, XLa, Fa, Ca, Sa), (Hb, XLb, Fb, Cb, Sb) = out(a), out(b)
vb = sorted(set(Ha) & set(Hb)); print('cmp44 %s vs %s: %d hash lines in both (%d / %d)' % (a, b, len(vb), len(Ha), len(Hb)))
for k in ('ee', 'iop', 'gs', 'vfb'):
    d = [v for v in vb if Ha[v].get(k) != Hb[v].get(k)]
    print('  %-4s differ at %d of %d%s' % (k, len(d), len(vb), (' (first vb %d, last %d)' % (d[0], d[-1])) if d else ''))
for nm, A, B in (('xlive lines', XLa, XLb), ('digest (frame) lines', Fa, Fb), ('content lines', Ca, Cb)):
    k = sorted(set(A) & set(B)); d = [v for v in k if A[v] != B[v]]
    print('  %-21s %d / %d, in both %d, differ %d%s' % (nm, len(A), len(B), len(k), len(d), (' (first vb %d: %s)' % (d[0], ', '.join(sorted(set(str((v - 24000) // 100) for v in d))[:12]))) if d else ''))
La, Lb = seed(a), seed(b)
for k in ('sv', 'sd', 'st'):
    print('  seed log %s from vb %d: %d / %d lines, md5 %s / %s: %s' % (k, FROM, len(La[k]), len(Lb[k]), md5(La[k]), md5(Lb[k]), 'IDENTICAL' if La[k] == Lb[k] else 'DIFFER'))
    if La[k] != Lb[k]:
        for x, y in zip(La[k], Lb[k]):
            if x != y: print('    first: %s | %s' % (x[:120], y[:120])); break
(Pa, Ta), (Pb, Tb) = act(a), act(b)
n1 = sum(1 for x, y in zip(Pa, Pb) if x[:4] != y[:4]); n2 = sum(1 for x, y in zip(Pa, Pb) if x != y)
print('  act log from vb %d: %d / %d passes; differ in (pass, vb, acts, act+seeds hash) %d, with the time hash %d; md5 %s / %s' % (
    FROM, len(Pa), len(Pb), n1, n2, md5([' '.join(p) for p in Pa]), md5([' '.join(p) for p in Pb])))
print('  host ticks per act call (rdtsc; arms ran side by side on 4 cores: rough):')
for nm in ('line_sph.c', 'rain.c', 'body_sph.c', 'ripple.c', 'd_splash_fall.c', 'dgd.c', 'gvd.c', 'raiden.c'):
    x, y = Ta.get(nm), Tb.get(nm)
    if x and y and x[0] and y[0]:
        pa, pb = x[1] / x[0], y[1] / y[0]
        print('    %-16s calls %d / %d, ticks a call %11.0f / %11.0f  %+.0f%%' % (nm, x[0], y[0], pa, pb, 100.0 * (pb - pa) / pa))
    elif x or y: print('    %-16s %s / %s' % (nm, x, y))
for s in Sa: print('  %s: %s' % (a, s[:220]))
for s in Sb: print('  %s: %s' % (b, s[:220]))
