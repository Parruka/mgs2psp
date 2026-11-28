import sys, re, hashlib
a, b = sys.argv[1], sys.argv[2]
def load(t):
    P = []; T = {}
    for l in open('/home/claude/v44/act_%s.txt' % t):
        f = l.split()
        if f[0] == 'P': P.append(f[1:])
        elif f[0] == 'T': T[(f[1], f[2])] = (int(f[3]), int(f[4]))
    return P, T
Pa, Ta = load(a); Pb, Tb = load(b)
print('passes', len(Pa), len(Pb))
n1 = n2 = 0; first1 = first2 = None
for x, y in zip(Pa, Pb):
    if x[:4] != y[:4]:
        n1 += 1; first1 = first1 or (x, y)
    if x != y:
        n2 += 1; first2 = first2 or (x, y)
print('pass lines differing in (pass, vb, acts, act+seeds hash):', n1, first1)
print('pass lines differing at all (with the time hash):', n2, first2)
def md5(P): return hashlib.md5('\n'.join(' '.join(p) for p in P).encode()).hexdigest()[:12]
print('md5 of the pass lines', md5(Pa), md5(Pb))
# hash lines
def hl(t):
    H = {}; X = []
    for l in open('/home/claude/v44/out_%s.log' % t, errors='replace'):
        if l.startswith('hash: vb '):
            m = re.match(r'hash: vb (\d+) ee (\w+) iop (\w+)(.*)', l)
            if m: H[int(m.group(1))] = (m.group(2), m.group(3), m.group(4).strip())
        elif l.startswith('xlive: vb '):
            X.append(l.strip())
    return H, X
Ha, Xa = hl(a); Hb, Xb = hl(b)
vbs = sorted(set(Ha) & set(Hb))
de = [v for v in vbs if Ha[v][0] != Hb[v][0]]; di = [v for v in vbs if Ha[v][1] != Hb[v][1]]; dr = [v for v in vbs if Ha[v][2] != Hb[v][2]]
print('hash lines', len(Ha), len(Hb), 'common', len(vbs))
print('ee differs at', len(de), (de[0], de[-1]) if de else '')
print('iop differs at', len(di), (di[0], di[-1]) if di else '')
print('rest of the line (gs, vfb, ...) differs at', len(dr), (dr[0], dr[-1]) if dr else '')
print('xlive picture lines', len(Xa), len(Xb), 'equal' if Xa == Xb else 'DIFFER: %d' % sum(1 for x, y in zip(Xa, Xb) if x != y))
if Xa != Xb:
    for x, y in zip(Xa, Xb):
        if x != y: print(' first', x[:100], '|', y[:100]); break
# ticks
print('act            calls       ticks a call: %s / %s' % (a, b))
rows = []
for k in Ta:
    if k in Tb: rows.append((Ta[k][1], k, Ta[k], Tb[k]))
rows.sort(reverse=True)
tot_a = sum(v[1] for v in Ta.values()); tot_b = sum(v[1] for v in Tb.values())
for t, k, x, y in rows[:28]:
    print('%s %-18s %8d %9.0f %9.0f  share %5.2f%% %5.2f%%' % (k[0], k[1], x[0], x[1] / x[0], y[1] / y[0], 100.0 * x[1] / tot_a, 100.0 * y[1] / tot_b))
print('all acts ticks', tot_a, tot_b)
# each arm against the reference twin run r18a (same binary family, no act log)
def hl2(p):
    H = {}
    for l in open(p, errors='replace'):
        m = re.match(r'hash: vb (\d+) ee (\w+) iop (\w+) gs (\w+) gsd (\w+) vfb (\w+)', l)
        if m: H[int(m.group(1))] = m.groups()[1:]
    return H
R = hl2('/home/claude/main/build/x86/out_r18a.log')
for nm in (a, b):
    X = hl2('/home/claude/v44/out_%s.log' % nm)
    for j, f in ((0, 'ee'), (1, 'iop'), (2, 'gs'), (4, 'vfb')):
        d = [v for v in sorted(X) if v in R and R[v][j] != X[v][j]]
        print(nm, 'vs reference r18a (%d lines):' % len(X), f, len(d), (d[0], d[-1]) if d else '')
A2 = hl2('/home/claude/v44/out_%s.log' % a); B2 = hl2('/home/claude/v44/out_%s.log' % b)
for j, f in ((0, 'ee'), (1, 'iop'), (2, 'gs'), (4, 'vfb')):
    d = [v for v in sorted(A2) if A2[v][j] != B2[v][j]]
    print(a, 'vs', b, f, len(d), (d[0], d[-1]) if d else '')
