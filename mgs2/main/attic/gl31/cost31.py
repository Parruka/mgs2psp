#!/usr/bin/env python3
"""cost31.py ELF FRAMES LABEL=MODELFILE ... [--diff A B]... [--top N]   (gl31, optional cost model, 2026-11-20; run with python3 -I)
PspProf's per-PC counts of one window each (scripts/pgo.sh model: '# total ...' then 'pc count imiss dmiss' lines) summed per function
of the main ELF (psp-nm; anything outside its functions, the overlay arena included, is '(not in the ELF's functions)'), with the port's
fitted cost model ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 [C model]; per frame = / FRAMES of that window
(FRAMES: one number, or LABEL=N,... ).  --diff A B: the functions whose cost differs most between two windows (B - A), per frame.
Every figure is the emulator's count through a model: [C model], not a PSP time."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
a = sys.argv[1:]; elf = a[0]; fr = a[1]; W = []; D = []; top = 14; i = 2
while i < len(a):
    if a[i] == '--diff': D.append((a[i + 1], a[i + 2])); i += 3
    elif a[i] == '--top': top = int(a[i + 1]); i += 2
    else: l, p = a[i].split('=', 1); W.append((l, p)); i += 1
FR = {}
if '=' in fr:
    for x in fr.split(','): l, n = x.split('='); FR[l] = int(n)
else: FR = {l: int(fr) for l, p in W}
S = []
for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
    f = l.split()
    if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
R = {}
for l, p in W:
    F = collections.defaultdict(lambda: [0, 0, 0]); tot = ''
    for ln in open(p):
        if ln.startswith('#'): tot = ln.strip(); continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else "(not in the ELF's functions)"
        x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
    T = [sum(F[n][j] for n in F) for j in range(3)]; R[l] = (F, T)
    print('%s (%s): %s' % (l, p, tot))
    print('%s: instructions %d, I-misses %d, D-misses %d = %.1f ms in the window, %.2f ms a frame over %d frames [C model]' % (
        l, T[0], T[1], T[2], ms(T), ms(T) / FR[l], FR[l]))
    for n in sorted(F, key=lambda n: -ms(F[n]))[:top]:
        print('   %-34s %10d i %7d im %7d dm  %7.3f ms a frame' % (n[:34], F[n][0], F[n][1], F[n][2], ms(F[n]) / FR[l]))
for x, y in D:
    (Fa, Ta), (Fb, Tb) = R[x], R[y]; fa, fb = FR[x], FR[y]
    print('\n%s - %s, per frame [C model]: instructions %+.0f, I-misses %+.1f, D-misses %+.1f = %+.3f ms a frame (%.2f -> %.2f)' % (
        y, x, Tb[0] / fb - Ta[0] / fa, Tb[1] / fb - Ta[1] / fa, Tb[2] / fb - Ta[2] / fa, ms(Tb) / fb - ms(Ta) / fa, ms(Ta) / fa, ms(Tb) / fb))
    d = {n: ms(Fb.get(n, [0, 0, 0])) / fb - ms(Fa.get(n, [0, 0, 0])) / fa for n in set(Fa) | set(Fb)}
    for n in sorted(d, key=lambda n: -abs(d[n]))[:top]:
        A = Fa.get(n, [0, 0, 0]); Bq = Fb.get(n, [0, 0, 0])
        print('   %-34s %+8.3f ms a frame (i %+9.0f, im %+7.1f, dm %+7.1f a frame)' % (n[:34], d[n], Bq[0] / fb - A[0] / fa, Bq[1] / fb - A[1] / fa, Bq[2] / fb - A[2] / fa))
