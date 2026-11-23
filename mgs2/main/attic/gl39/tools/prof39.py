#!/usr/bin/env python3
"""prof39.py OVL FILE [BASEFILE] [TOP] (gl39 plan; python3 -I): one PspProf window of gl38's final build (build_gl38/psp) by function,
ms a PSP STEP [C model] = (1.65 instr + 68 I-miss + 86 D-miss) / 333,000 / 80 steps.  OVL: w00a or d01t (the stage's overlay ELF).
Side: ME = ge_* objects but fxthin (the picture code), wait = hw, game = the recompiled game (P<addr>, S_<ovl>_P<addr>, named from
our function-name lists: the last HD symbol at or before the piece), main-rt = everything else.  With BASEFILE: FILE - BASEFILE."""
import sys, subprocess, bisect, collections, glob, os, re
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; P = '/home/claude/main/build_gl38/psp'; RE = '/mnt/project-files/mgs2/re/'
ovl = sys.argv[1]; fa = sys.argv[2]; fb = sys.argv[3] if len(sys.argv) > 3 and not sys.argv[3].isdigit() else None
top = int(sys.argv[-1]) if sys.argv[-1].isdigit() else 40
S = []
for elf in (P + '/MGS2T.elf', P + '/ovl_%s.elf' % ovl):
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]
OBJ = {}
for o in glob.glob(P + '/*.o'):
    for l in subprocess.run([NM, '--defined-only', o], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 3 and f[1] in 'tTwW': OBJ.setdefault(f[2], os.path.basename(o)[:-2])
HD = []
for fn in ('our function-name lists', 'our function-name lists'):
    for l in open(RE + fn):
        f = l.split()
        if len(f) >= 3 and f[0].startswith('0x'): HD.append((int(f[0], 16), f[1], f[2]))
HD.sort(); hs = [h[0] for h in HD]
AN = []
for l in open('/home/claude/main/rainstudy/ana/actnames.txt'):
    a, n = l.split(); a = int(a, 16)
    if a: AN.append((a, n))
AN.sort(); ans = [a[0] for a in AN]
def gname(n):
    m = re.match(r'^(?:S_\w+?_)?P([0-9a-f]{6})(\..*)?$', n)
    if not m: return None
    a = int(m.group(1), 16); k = bisect.bisect_right(hs, a) - 1
    if k >= 0 and a - HD[k][0] < 0x1800: return '%s %s%s [%s]' % (m.group(1), HD[k][1], '+%x' % (a - HD[k][0]) if a != HD[k][0] else '', HD[k][2])
    k = bisect.bisect_right(ans, a) - 1
    if k >= 0 and a - AN[k][0] < 0x3000: return '%s (%s+%x)' % (m.group(1), AN[k][1], a - AN[k][0])
    return m.group(1)
def side(n):
    if n == 'hw': return 'wait'
    if gname(n): return 'game'
    b = n.split('.')[0]; o = OBJ.get(n, OBJ.get(b, '?'))
    if o.startswith('ge_') and o != 'ge_fxthin': return 'ME'
    return 'main-rt'
def load(p):
    F = collections.defaultdict(lambda: [0, 0, 0])
    for ln in open(p):
        if ln.startswith('#'): continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else '(outside %07x)' % (pc & ~0xfffff)
        x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
    return F
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
A = load(fa); B = load(fb) if fb else {}
D = {n: [(A.get(n, [0, 0, 0])[j] - B.get(n, [0, 0, 0])[j]) / 80.0 for j in range(3)] for n in set(A) | set(B)}
sd = collections.defaultdict(lambda: [0, 0, 0])
for n, d in D.items():
    s = side(n)
    for j in range(3): sd[s][j] += d[j]
print('# %s%s, ms a PSP step [C model]' % (fa, ' MINUS ' + fb if fb else ''))
for s in sorted(sd): print('  %-8s %+9.3f ms  (instr %+11.0f  I-miss %+8.0f  D-miss %+8.0f a step)' % (s, ms(sd[s]), *sd[s]))
print('  %-8s %+9.3f ms' % ('all', sum(ms(v) for v in sd.values())))
for want in ('game', 'main-rt', 'ME'):
    rows = sorted(((n, d) for n, d in D.items() if side(n) == want), key=lambda kv: -abs(ms(kv[1])))
    print(' -- %s, top %d (ms: all = instr + I-miss + D-miss parts; object)' % (want, top))
    for n, d in rows[:top]:
        g = gname(n) or n
        print('   %+7.3f = %+6.3f %+6.3f %+6.3f  %-52s %s' % (ms(d), 1.65 * d[0] / 333000, 68 * d[1] / 333000, 86 * d[2] / 333000, g[:52], OBJ.get(n, OBJ.get(n.split('.')[0], ''))))
