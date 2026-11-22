#!/usr/bin/env python3
"""fdiff36.py ELF_A FILE_A ELF_B FILE_B FRAMES [N] (gl36 look b; python3 -I): two PspProf windows of two ELFs, per function by name, B - A a
picture, ranked by |ms| on the cost model ms = (1.65 x instr + 68 x I-miss + 86 x D-miss) / 333,000 [C model]; then the same by group:
I-miss, D-miss and instruction parts of the total difference, and the functions only one side has."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def load(elf, p, fr):
    S = []
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
    S.sort(); st = [s[0] for s in S]; F = collections.defaultdict(lambda: [0.0, 0.0, 0.0])
    for ln in open(p):
        if ln.startswith('#'): continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else '(outside)'
        x = F[n]; x[0] += int(f[1]) / fr; x[1] += int(f[2]) / fr; x[2] += int(f[3]) / fr
    return F
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
ea, fa, eb, fb, fr = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]); top = int(sys.argv[6]) if len(sys.argv) > 6 else 30
A = load(ea, fa, fr); B = load(eb, fb, fr)
names = set(A) | set(B); z = [0.0, 0.0, 0.0]
D = {n: [B.get(n, z)[j] - A.get(n, z)[j] for j in range(3)] for n in names}
tA = [sum(A[n][j] for n in A) for j in range(3)]; tB = [sum(B[n][j] for n in B) for j in range(3)]; td = [tB[j] - tA[j] for j in range(3)]
print('A %s: %.0f instr, %.0f I-miss, %.0f D-miss = %.2f ms a picture' % (fa, tA[0], tA[1], tA[2], ms(tA)))
print('B %s: %.0f instr, %.0f I-miss, %.0f D-miss = %.2f ms a picture' % (fb, tB[0], tB[1], tB[2], ms(tB)))
print('B - A: instr %+.0f (%+.2f ms), I-miss %+.0f (%+.2f ms), D-miss %+.0f (%+.2f ms) = %+.2f ms a picture [C model]' %
      (td[0], 1.65 * td[0] / 333000, td[1], 68 * td[1] / 333000, td[2], 86 * td[2] / 333000, ms(td)))
onlyA = [n for n in A if n not in B]; onlyB = [n for n in B if n not in A]
print('functions only in A: %d (%.3f ms), only in B: %d (%.3f ms): %s' % (len(onlyA), sum(ms(A[n]) for n in onlyA), len(onlyB),
      sum(ms(B[n]) for n in onlyB), ', '.join(sorted(onlyB, key=lambda n: -ms(B[n]))[:12])))
# how the difference spreads: the top N's share and the I-miss difference's sign split
pos = sum(ms(d) for d in D.values() if ms(d) > 0); neg = sum(ms(d) for d in D.values() if ms(d) < 0)
ip = sum(d[1] for d in D.values() if d[1] > 0); im = sum(d[1] for d in D.values() if d[1] < 0)
print('spread: functions slower in B sum %+.2f ms, faster %+.2f ms; I-miss up %+.0f in the functions that gained, %+.0f in those that lost' % (pos, neg, ip, im))
print('%-32s %9s %9s %9s %9s | %9s %9s %9s' % ('function (B - A a picture)', 'ms', 'instr', 'I-miss', 'D-miss', 'A instr', 'A I-miss', 'A D-miss'))
for n, d in sorted(D.items(), key=lambda x: -abs(ms(x[1])))[:top]:
    a = A.get(n, z)
    print('%-32s %+9.3f %+9.0f %+9.0f %+9.0f | %9.0f %9.0f %9.0f' % (n[:32], ms(d), d[0], d[1], d[2], a[0], a[1], a[2]))
if len(sys.argv) > 7:
    print('functions whose instructions changed by more than %s a picture:' % sys.argv[7])
    for n, d in sorted(D.items(), key=lambda x: -abs(x[1][0])):
        if abs(d[0]) > float(sys.argv[7]): print('   %-32s instr %+9.0f (%+.3f ms by instructions), I-miss %+7.0f, D-miss %+7.0f, total %+.3f ms' % (n[:32], d[0], 1.65 * d[0] / 333000, d[1], d[2], ms(d)))
