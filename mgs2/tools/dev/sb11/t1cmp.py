# t1cmp.py <new log> <reference log>: task #133 T1 on x86: frame lines, content digests, hashes, vuh equal; every other line listed if it differs
import sys, re, difflib
TIMING = re.compile(r'( in [0-9.]+ ms| [0-9.]+ ms|[0-9.]+ s\b|seconds|ms a |ms per|time|took|elapsed|wall)', re.I)
def load(p):
    fr, ct, hs, vu, other = {}, {}, {}, [], []
    for l in open(p, errors='replace'):
        l = l.rstrip('\n')
        if l.startswith('picture: vb ') and ' content ' in l: q = l.split(); ct[int(q[2])] = l; continue
        if l.startswith('picture: vb ') and ' buffer ' in l: q = l.split(); fr[int(q[2])] = l; continue
        if l.startswith('hash: vb '): hs[l.split()[2]] = l; continue
        if l.startswith('vuh'): vu.append(l); continue
        other.append(l)
    return fr, ct, hs, vu, other
A, R = load(sys.argv[1]), load(sys.argv[2])
for name, i in (('frame lines', 0), ('content lines', 1), ('hash lines', 2)):
    ks = sorted(set(A[i]) | set(R[i])); d = [k for k in ks if A[i].get(k) != R[i].get(k)]
    print('%s: new %d, reference %d, differ %d %s' % (name, len(A[i]), len(R[i]), len(d), d[:5]))
print('vuh lines: new %d, reference %d, equal %s' % (len(A[3]), len(R[3]), A[3] == R[3]))
a = [l for l in A[4]]; r = [l for l in R[4]]
n = 0
for l in difflib.unified_diff(r, a, 'reference', 'new', n=0, lineterm=''):
    if l.startswith(('---', '+++', '@@')): continue
    n += 1
    if n <= 60: print(l[:260])
print('other lines differing (- reference, + new): %d' % n)
