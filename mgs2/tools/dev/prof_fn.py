#!/usr/bin/env python3
"""fnprof.py <elf> <dump> <steps> [top]: PspProf dump summed per function (symbol table of the ELF), model ms per step
(1.4 x instructions + 70 x I/D misses) / 333,000"""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
elf, dump, steps = sys.argv[1], sys.argv[2], int(sys.argv[3]); top = int(sys.argv[4]) if len(sys.argv) > 4 else 40
S = {}
for l in subprocess.run([NM, '-n', elf], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': S[int(p[0], 16)] = p[2]
S = sorted(S.items()); A = [a for a, _ in S]; N = [n for _, n in S]
F = collections.defaultdict(lambda: [0, 0, 0]); hdr = ''
for l in open(dump):
    if l[0] == '#': hdr = l.strip(); continue
    a, i, im, dm = l.split(); a = int(a, 16)
    k = bisect.bisect_right(A, a) - 1; n = N[k] if k >= 0 else '?'
    f = F[n]; f[0] += int(i); f[1] += int(im); f[2] += int(dm)
ti = sum(f[0] for f in F.values()); tim = sum(f[1] for f in F.values()); tdm = sum(f[2] for f in F.values())
def ms(i, m): return (1.4 * i + 70 * m) / 333000 / steps
print(hdr); print('per step: %.2f M instructions, %.1f k I-misses, %.1f k D-misses: model %.2f ms' % (ti / steps / 1e6, tim / steps / 1e3, tdm / steps / 1e3, ms(ti, tim + tdm)))
for n, f in sorted(F.items(), key=lambda kv: -(1.4 * kv[1][0] + 70 * (kv[1][1] + kv[1][2])))[:top]:
    print('%7.2f ms %5.1f%%  %6.3f M instr %6.1f k im %6.1f k dm  %s' % (ms(f[0], f[1] + f[2]), 100 * ms(f[0], f[1] + f[2]) / ms(ti, tim + tdm), f[0] / steps / 1e6, f[1] / steps / 1e3, f[2] / steps / 1e3, n))
