#!/usr/bin/env python3
"""fdiff39.py BUILD_A FILE_A BUILD_B FILE_B [TOP] (python3 -I): two PspProf windows of two BUILDS (39 = build_gl39/psp, 38 = build_gl38/psp)
by FUNCTION NAME (each file with its own build's symbols; .constprop/.isra/.part suffixes kept): A minus B, ms a PSP step [C model],
instr / I-miss / D-miss a step, the side (score39.py's) and A's and B's own ms."""
import sys, os, collections, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
import subprocess, glob, re
R = '/home/claude/main/build_gl39'; G38 = '/home/claude/main/build_gl38'
BUILD = {'39': (R + '/psp', R + '/u'), '38': (G38 + '/psp', G38 + '/psp')}
GAME = re.compile(r'^(?:S_\w+?_)?P[0-9a-f]{6}(\..*)?$')
def load(b, p):
    P, O = BUILD[b]; S = []
    for elf in (P + '/MGS2T.elf', P + '/ovl_w00a.elf'):
        for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
            f = l.split()
            if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
    S.sort(); st = [s[0] for s in S]; OBJ = {}
    for o in glob.glob(O + '/*.o'):
        for l in subprocess.run([NM, '--defined-only', o], capture_output=True, text=True).stdout.split('\n'):
            f = l.split()
            if len(f) == 3 and f[1] in 'tTwW': OBJ.setdefault(f[2], os.path.basename(o)[:-2])
    F = collections.defaultdict(lambda: [0.0, 0.0, 0.0])
    for ln in open(p):
        if ln[0] == '#': continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else '(outside)'
        x = F[n]; x[0] += int(f[1]) / 80.0; x[1] += int(f[2]) / 80.0; x[2] += int(f[3]) / 80.0
    return F, OBJ
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
A, OA = load(sys.argv[1], sys.argv[2]); B, OB = load(sys.argv[3], sys.argv[4]); top = int(sys.argv[5]) if len(sys.argv) > 5 else 40
def side(n):
    if n == 'hw': return 'wait'
    if n == '(outside)': return 'other'
    if GAME.match(n): return 'game'
    o = OA.get(n, OA.get(n.split('.')[0], OB.get(n, OB.get(n.split('.')[0], '?'))))
    return 'ME:' + o if o.startswith('ge_') and o != 'ge_fxthin' else 'rt:' + o
Z = [0.0, 0.0, 0.0]; D = {n: [A.get(n, Z)[j] - B.get(n, Z)[j] for j in range(3)] for n in set(A) | set(B)}
print('# %s (%s) MINUS %s (%s): all %+.2f ms a PSP step [C model]' % (sys.argv[2], sys.argv[1], sys.argv[4], sys.argv[3], sum(ms(v) for v in D.values())))
for n, v in sorted(D.items(), key=lambda kv: -abs(ms(kv[1])))[:top]:
    print('  %+7.2f  instr %+9.0f I %+6.0f D %+6.0f  A %7.2f B %7.2f  %-14s %s' % (ms(v), v[0], v[1], v[2], ms(A.get(n, Z)), ms(B.get(n, Z)), side(n), n))
