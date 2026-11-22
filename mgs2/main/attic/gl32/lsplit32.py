#!/usr/bin/env python3
"""lsplit32.py ELF FRAMES LABEL PROFILE   (gl32 plan step 1, the list build; run with python3 -I)
One PspProf window's cost by function [C model: ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000], the functions of
ge/gelist.c (the frame's batches into the GE list: gx_*, xf_*, set_*, the dz_* shadow pairing) named one by one, per picture."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
elf, fr, label, prof = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
S = []
for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
    f = l.split()
    if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]
F = collections.defaultdict(lambda: [0, 0, 0])
for ln in open(prof):
    if ln.startswith('#'): continue
    f = ln.split()
    if len(f) < 4: continue
    pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
    n = (S[k][2], S[k][1]) if k >= 0 and pc < S[k][0] + S[k][1] else ('(other)', 0)
    x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
# gelist.c's functions: by the object's symbols
G = set()
for l in subprocess.run([NM, '-S', '--defined-only', '/home/claude/main/build_gl31/stage/ge_gelist.o'], capture_output=True, text=True).stdout.split('\n'):
    f = l.split()
    if len(f) == 4 and f[2] in 'tT': G.add((f[3], int(f[1], 16)))   # (name, size): other objects have functions of the same names
tot = [sum(F[n][j] for n in F) for j in range(3)]; lt = [sum(F[n][j] for n in F if n in G) for j in range(3)]
print('%s (%s, %d pictures): all %.2f ms a picture; gelist.c %.2f ms a picture (%.0f instructions, %.0f I-misses, %.0f D-misses a picture)' % (
    label, prof, fr, ms(tot) / fr, ms(lt) / fr, lt[0] / fr, lt[1] / fr, lt[2] / fr))
for n in sorted(G, key=lambda n: -ms(F[n])):
    if ms(F[n]) / fr < 0.02: continue
    print('   %-22s %7.3f ms a picture (instr %9.0f, I-miss %6.0f, D-miss %6.0f)' % (n[0], ms(F[n]) / fr, F[n][0] / fr, F[n][1] / fr, F[n][2] / fr))
