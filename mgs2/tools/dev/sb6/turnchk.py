# turnchk.py <turns log> <all-on log(.gz)> <hashref>: hash and vuh lines present in both, compared; the hashref entries; the other lines
# that differ once the dense 15,700-16,010 lines (in the reference only) and timing lines are left out
import sys, re, gzip
def op(p): return gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')
TIM = re.compile(r'( in [0-9.]+ ms| [0-9.]+ ms|[0-9.]+ s\b|seconds|ms a |ms per|time|took|elapsed|wall)', re.I)
def load(p):
    h, v, o = {}, {}, []
    for l in op(p):
        l = l.rstrip('\n')
        if l.startswith('hash: vb '): q = l.split(); h[int(q[2])] = ' '.join(q[3:7]); continue
        if l.startswith('vuh: vb '): q = l.split(); v[int(q[2])] = q[3]; continue
        if l.startswith('picture: vb ') or TIM.search(l) or l.startswith('xlive mode: the lanes follow'): continue
        o.append(l)
    return h, v, o
A = load(sys.argv[1]); B = load(sys.argv[2])
hb = [k for k in A[0] if k in B[0]]; print('hash lines: turns %d, all-on %d, in both %d, differ %d' % (len(A[0]), len(B[0]), len(hb), sum(A[0][k] != B[0][k] for k in hb)))
vb = [k for k in A[1] if k in B[1]]; print('vuh lines: turns %d, all-on %d, in both %d, differ %d' % (len(A[1]), len(B[1]), len(vb), sum(A[1][k] != B[1][k] for k in vb)))
ref = {}
for l in open(sys.argv[3]):
    q = l.split()
    if len(q) >= 2 and q[0].isdigit(): ref[int(q[0])] = q[1:]
n = 0; bad = 0
for k, s in A[0].items():
    if k in ref:
        n += 1; bad += s.split()[1] != ref[k][0] if ref[k] else 0
print('hashref: %d entries met, ee hash differs in %d' % (n, bad))
import difflib
d = [l for l in difflib.unified_diff(B[2], A[2], lineterm='', n=0) if l[:1] in '+-' and not l.startswith(('+++', '---'))]
print('other lines differ: %d' % len(d)); print('\n'.join(d[:12]))
