#!/usr/bin/env python3
"""armA32.py ELF_A FILE_A ELF_B FILE_B FRAMES LABEL [--top N]   (gl32 step 6; run with python3 -I)
Two PspProf windows of the same vblanks made with two different ELFs (gl31's arm 3 and gl32's arm A, or two gl32 arms with the same
ELF given twice): each window's counts summed per function of its own ELF, then B - A per function name, a picture.
ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 [C model]: a model, not a PSP time."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
ea, fa, eb, fb, fr, label = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), sys.argv[6]
top = int(sys.argv[sys.argv.index('--top') + 1]) if '--top' in sys.argv else 30
def load(elf, p):
    S = []
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
    S.sort(); st = [s[0] for s in S]; F = collections.defaultdict(lambda: [0, 0, 0]); Z = {}
    for ln in open(p):
        if ln.startswith('#'): continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else "(not in the ELF's functions)"
        if n[0] != '(': Z[n] = S[k][1]
        x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
    return F, Z
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
(A, ZA), (B, ZB) = load(ea, fa), load(eb, fb)
ta = [sum(A[n][j] for n in A) / fr for j in range(3)]; tb = [sum(B[n][j] for n in B) / fr for j in range(3)]
print('%s: B %s - A %s, %d pictures' % (label, fb, fa, fr))
print('  A: %.0f instructions, %.0f I-misses, %.0f D-misses = %.2f ms a picture;  B: %.0f, %.0f, %.0f = %.2f ms;  B - A: %+.0f, %+.0f, %+.0f = %+.2f ms a picture [C model]' % (
    ta[0], ta[1], ta[2], ms(ta), tb[0], tb[1], tb[2], ms(tb), tb[0] - ta[0], tb[1] - ta[1], tb[2] - ta[2], ms(tb) - ms(ta)))
D = {n: [(B.get(n, [0, 0, 0])[j] - A.get(n, [0, 0, 0])[j]) / fr for j in range(3)] for n in set(A) | set(B)}
hw = D.get('hw', [0, 0, 0]); print('  of it the waits (hw): %+.2f ms; the work: %+.2f ms' % (ms(hw), ms(tb) - ms(ta) - ms(hw)))
r = sorted(D.items(), key=lambda x: -abs(ms(x[1])))
print('  by function (ms a picture; instructions, I-misses, D-misses a picture; the function\'s size in A / B):')
for n, d in r[:top]:
    print('   %-30s %+7.3f ms  (%+9.0f %+7.0f %+7.0f)  size %s / %s' % (n[:30], ms(d), d[0], d[1], d[2], ZA.get(n, '-'), ZB.get(n, '-')))
rest = [sum(d[j] for n, d in r[top:]) for j in range(3)]; print('   %-30s %+7.3f ms  (%+9.0f %+7.0f %+7.0f)' % ('(all the others)', ms(rest), rest[0], rest[1], rest[2]))
