# cmp6.py <on log> <off log> <reference log> [hashref]: cmp4.py (step c's x86 checks, renderer_notes 11.29) reading .gz logs too; sb6: lane 3
# on against off, and off against the reference (sb5's x1 run with lanes 1, 2 and 4 on, the configuration of sb6's off run)
import sys, re, gzip
on, off, ref = sys.argv[1:4]; href = sys.argv[4] if len(sys.argv) > 4 else None
TIMING = re.compile(r'( in [0-9.]+ ms| [0-9.]+ ms|[0-9.]+ s\b|seconds|ms a |ms per|time|took|elapsed|wall)', re.I)
def load(p):
    pic, cont, hsh, other = {}, {}, {}, []
    for l in (gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')):
        l = l.rstrip('\n')
        if l.startswith('picture: vb ') and ' content ' in l:
            q = l.split(); cont[int(q[2])] = q[4]; continue
        if l.startswith('picture: vb ') and ' buffer ' in l:
            q = l.split(); pic[int(q[2])] = l; continue
        if l.startswith('hash: vb '):
            q = l.split(); hsh[int(q[2])] = ' '.join(q[3:7]); continue
        other.append(l)
    return pic, cont, hsh, other
A, B, R = load(on), load(off), load(ref)
print('frames: on %d, off %d, reference %d' % (len(A[0]), len(B[0]), len(R[0])))
# 1. the content: on == off, every frame
ks = sorted(set(A[1]) | set(B[1])); d = [k for k in ks if A[1].get(k) != B[1].get(k)]
print('content digests: on %d, off %d, differ %d %s' % (len(A[1]), len(B[1]), len(d), d[:5]))
# 2. off == the reference (sb3), the whole picture line
ks = sorted(set(B[0]) | set(R[0])); d = [k for k in ks if B[0].get(k) != R[0].get(k)]
print('picture lines off vs reference: differ %d %s' % (len(d), d[:5]))
# 3. on vs off: the picture line without its two digests (words, aux bytes, batches, PRIMs, textures...)
def strip(l): return re.sub(r'list [0-9a-f]{16} frame [0-9a-f]{16}', 'list - frame -', l)
ks = sorted(set(A[0]) | set(B[0])); d = [k for k in ks if strip(A[0].get(k, '')) != strip(B[0].get(k, ''))]
dl = [k for k in ks if A[0].get(k) != B[0].get(k)]
print('picture lines on vs off: digests differ in %d frames; the rest of the line differs in %d %s' % (len(dl), len(d), d[:5]))
for k in d[:3]: print('  on :', A[0].get(k)); print('  off:', B[0].get(k))
# 4. state hashes: on == off == reference (== hashref)
for nm, X in (('on', A), ('off', B)):
    d = [k for k in sorted(set(X[2]) | set(R[2])) if X[2].get(k) != R[2].get(k)]
    print('state hashes %s vs reference: %d, differ %d %s' % (nm, len(X[2]), len(d), d[:5]))
if href:
    H = {}
    for l in open(href):
        q = l.split()
        if len(q) >= 3 and q[0].isdigit(): H[int(q[0])] = q[1:]
    print('hashref entries', len(H))
# 5. the other lines: on vs off and off vs reference, timings aside
def norm(ls): return [l for l in ls if not TIMING.search(l)]
import difflib
for nm, X, Y in (('on vs off', A, B), ('off vs reference', B, R)):
    a, b = norm(X[3]), norm(Y[3]); dd = [l for l in difflib.unified_diff(b, a, lineterm='', n=0) if not l.startswith(('---', '+++', '@@'))]
    print('other lines %s: %d differ' % (nm, len(dd)))
    for l in dd[:40]: print('   ', l[:300])
