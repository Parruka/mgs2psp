#!/usr/bin/env python3
"""topfn.py <elf> <ovl elf> <dump> <elf v24> <ovl elf v24> <dump v24>: per-function I-misses, instructions, distinct 64-byte code lines,
v25f window against v24's tutorial window (function names from the symbol tables)."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def syms(*elfs):
    S = {}
    for e in elfs:
        for l in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tTwW': S[int(p[0], 16)] = p[2]
    S = sorted(S.items()); return [a for a, _ in S], [n for _, n in S]
def load(f):
    d = {}
    for l in open(f):
        if l[0] == '#': continue
        a, c, i, m = l.split(); d[int(a, 16)] = (int(c), int(i), int(m))
    return d
def per_fn(elf, ovl, dump):
    A, N = syms(elf, ovl); C = load(dump)
    F = collections.defaultdict(lambda: [0, 0, 0, set()])
    for a, v in C.items():
        i = bisect.bisect_right(A, a) - 1; n = N[i] if i >= 0 else '?'
        f = F[n]; f[0] += v[0]; f[1] += v[1]; f[2] += v[2]; f[3].add(a >> 6)
    return F
a = per_fn(*sys.argv[1:4]); b = per_fn(*sys.argv[4:7])
def tot(F): return [sum(f[k] for f in F.values()) for k in range(3)] + [sum(len(f[3]) for f in F.values())]
ta, tb = tot(a), tot(b)
print('lines of code touched in the window: free %d (%d KB), tutorial %d (%d KB)' % (ta[3], ta[3] * 64 // 1024, tb[3], tb[3] * 64 // 1024))
print('functions run: free %d, tutorial %d; only in free %d, only in tutorial %d' % (len(a), len(b), len(set(a) - set(b)), len(set(b) - set(a))))
# functions only in free play: their share
only = set(a) - set(b)
s = [sum(a[n][k] for n in only) for k in range(3)] + [sum(len(a[n][3]) for n in only)]
print('only in free play: %.3f M instr, %.1f k I-miss, %.1f k D-miss per step, %d KB of code' % (s[0] / 100e6, s[1] / 100e3, s[2] / 100e3, s[3] * 64 // 1024))
both = set(a) & set(b)
s1 = [sum(a[n][k] for n in both) for k in range(3)]; s2 = [sum(b[n][k] for n in both) for k in range(3)]
print('in both: free %.3f M instr %.1f k I-miss; tutorial %.3f M instr %.1f k I-miss per step' % (s1[0] / 100e6, s1[1] / 100e3, s2[0] / 100e6, s2[1] / 100e3))
print('top I-miss increases (per step: free - tutorial; instr free/tutorial):')
D = sorted(set(a) | set(b), key=lambda n: -((a[n][1] if n in a else 0) - (b[n][1] if n in b else 0)))
for n in D[:40]:
    x = a.get(n, [0, 0, 0, set()]); y = b.get(n, [0, 0, 0, set()])
    print('  %-34s %+8.0f I-miss  (%6.0f / %6.0f)  instr %8.0f / %8.0f  lines %4d / %4d' % (n, (x[1] - y[1]) / 100, x[1] / 100, y[1] / 100, x[0] / 100, y[0] / 100, len(x[3]), len(y[3])))
