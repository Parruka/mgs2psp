#!/usr/bin/env python3
"""fncmp.py <elf A> <ovl A> <dump A> <elf B> <ovl B> <dump B> [N]: PspProf dumps of two builds of the same window, per function (names
from the symbol tables): instructions, I-misses and D-misses per step (100 steps), model ms = (1.4 instr + 70 misses) / 333,000, B - A;
the N largest gains and losses by instructions + D-misses (I-misses of plain builds follow the layout, listed apart)."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def syms(*elfs):
    S = {}
    for e in elfs:
        for l in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tTwW': S[int(p[0], 16)] = p[2]
    S = sorted(S.items()); return [a for a, _ in S], [n for _, n in S]
def per_fn(elf, ovl, dump):
    A, N = syms(elf, ovl); F = collections.defaultdict(lambda: [0, 0, 0])
    for l in open(dump):
        if l[0] == '#' or not l.strip(): continue
        a, c, i, m = l.split(); a = int(a, 16)
        k = bisect.bisect_right(A, a) - 1; f = F[N[k] if k >= 0 else '?']
        f[0] += int(c); f[1] += int(i); f[2] += int(m)
    return {n: [v / 100 for v in f] for n, f in F.items()}
def ms(v, imiss=True): return (1.4 * v[0] + 70 * (v[2] + (v[1] if imiss else 0))) / 333000
a = per_fn(*sys.argv[1:4]); b = per_fn(*sys.argv[4:7]); N = int(sys.argv[7]) if len(sys.argv) > 7 else 25
Z = [0, 0, 0]
ta = [sum(f[k] for f in a.values()) for k in range(3)]; tb = [sum(f[k] for f in b.values()) for k in range(3)]
print('A: instr %.3f M  I-miss %.1f k  D-miss %.1f k  %.2f ms' % (ta[0] / 1e6, ta[1] / 1e3, ta[2] / 1e3, ms(ta)))
print('B: instr %.3f M  I-miss %.1f k  D-miss %.1f k  %.2f ms' % (tb[0] / 1e6, tb[1] / 1e3, tb[2] / 1e3, ms(tb)))
print('B - A: instr %+.3f M (%+.2f ms)  D-miss %+.1f k (%+.2f ms)  I-miss %+.1f k (%+.2f ms)' % ((tb[0] - ta[0]) / 1e6,
      1.4 * (tb[0] - ta[0]) / 333000, (tb[2] - ta[2]) / 1e3, 70 * (tb[2] - ta[2]) / 333000, (tb[1] - ta[1]) / 1e3, 70 * (tb[1] - ta[1]) / 333000))
names = set(a) | set(b)
d = {n: ms(b.get(n, Z), False) - ms(a.get(n, Z), False) for n in names}
g = sorted(names, key=lambda n: d[n])
pos = sum(v for v in d.values() if v > 0); neg = sum(v for v in d.values() if v < 0)
print('functions faster: %.2f ms in %d; slower: %+.2f ms in %d (instructions + D-misses)' % (neg, sum(1 for v in d.values() if v < 0),
      pos, sum(1 for v in d.values() if v > 0)))
def row(n):
    x = a.get(n, Z); y = b.get(n, Z)
    return '  %-30s %+7.3f ms  instr %8.0f -> %8.0f  D-miss %6.0f -> %6.0f  I-miss %6.0f -> %6.0f' % (n, d[n], x[0], y[0], x[2], y[2], x[1], y[1])
print('largest gains:'); [print(row(n)) for n in g[:N]]
print('largest losses:'); [print(row(n)) for n in g[::-1][:N] if d[n] > 0]
