#!/usr/bin/env python3
"""fn39.py OVL FILE FUNC... (python3 -I): a function in one PspProf window of gl38's final build: calls (count at its first
instruction), instructions, I- and D-misses a PSP step and a call, and the hottest instruction count (the inner loop's trip count)."""
import sys, subprocess
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; P = '/home/claude/main/build_gl38/psp'
S = {}
for elf in (P + '/MGS2T.elf', P + '/ovl_%s.elf' % sys.argv[1]):
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'tTwW': S.setdefault(f[3], (int(f[0], 16), int(f[1], 16)))
C = {}
for ln in open(sys.argv[2]):
    if ln[0] == '#': continue
    f = ln.split(); C[int(f[0], 16)] = (int(f[1]), int(f[2]), int(f[3]))
for fn in sys.argv[3:]:
    if fn not in S: print(fn, 'not found'); continue
    a, sz = S[fn]; rows = [(pc, C[pc]) for pc in range(a, a + sz, 4) if pc in C]
    calls = C.get(a, (0, 0, 0))[0]; ins = sum(r[1][0] for r in rows); im = sum(r[1][1] for r in rows); dm = sum(r[1][2] for r in rows)
    mx = max((r[1][0] for r in rows), default=0); c = max(calls, 1)
    print('%-26s size %6d B  calls/step %8.1f  instr/step %9.0f (%7.1f a call)  I-miss/step %6.0f  D-miss/step %6.0f (%5.2f a call)  hottest instr %8.0f/step' % (fn, sz, calls / 80.0, ins / 80.0, ins / c, im / 80.0, dm / 80.0, dm / c, mx / 80.0))
    top = sorted(rows, key=lambda r: -r[1][2])[:4]
    print('      D-miss by instruction: ' + ', '.join('+%x: %.0f' % (pc - a, v[2] / 80.0) for pc, v in top if v[2]))
