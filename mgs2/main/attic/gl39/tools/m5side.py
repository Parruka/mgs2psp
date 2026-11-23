#!/usr/bin/env python3
"""m5side.py (python3 -I): M5's own model figure from the SIDE build p5 (the final objects with boot_rt.o compiled again from a copy
of rt/kernel.h that has M5's block; model only, never delivered): windows model/m5a (.1) = M5 on, model/m5b (.1) = M5 off (the same
binary, MGS2_OPT2 bit 18), 29,610 K0 digest-off and 29,710 K7.  Per function of the interrupt path and for the whole window."""
import subprocess, bisect
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; R = '/home/claude/main/build_gl39/'
S = []
for l in subprocess.run([NM, '-S', '--defined-only', R + 'p5/MGS2T.elf'], capture_output=True, text=True).stdout.split('\n'):
    f = l.split()
    if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]
def win(p):
    F = {}; tot = [0.0, 0.0, 0.0]
    for ln in open(p):
        if ln[0] == '#': continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else '?'
        x = F.setdefault(n, [0.0, 0.0, 0.0])
        for j in range(3): x[j] += int(f[1 + j]) / 80.0; tot[j] += int(f[1 + j]) / 80.0
    return F, tot
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
NAMES = ('k_take_interrupts', 'k_take_interrupts.part.0', 'k_intc.part.0', 'k_dmac.part.0', 'k_call_handler', 'k_deliver_pending', 'k_poll_rest', 'k_syscall', 'rt_syscall')
for s, n in (('', '29,610 K0 digest-off'), ('.1', '29,710 K7')):
    A, ta = win(R + 'model/m5b' + s); B, tb = win(R + 'model/m5a' + s)
    print('%s: M5 OFF minus on [C model]: whole window %+.3f ms a step (instr %+.0f I %+.0f D %+.0f)' % (n, ms(ta) - ms(tb), ta[0] - tb[0], ta[1] - tb[1], ta[2] - tb[2]))
    si = [0.0, 0.0, 0.0]
    for f in NAMES:
        a, b = A.get(f, [0, 0, 0]), B.get(f, [0, 0, 0])
        for j in range(3): si[j] += a[j] - b[j]
        print('    %-26s off %8.0f instr (%.3f ms)  on %8.0f instr (%.3f ms)  off - on %+7.0f instr, I %+4.0f, D %+4.0f = %+.3f ms' % (f, a[0], ms(a), b[0], ms(b), a[0] - b[0], a[1] - b[1], a[2] - b[2], ms(a) - ms(b)))
    print('    the interrupt path together: off - on %+.0f instr, I %+.0f, D %+.0f = %+.3f ms a step' % (si[0], si[1], si[2], ms(si)))
