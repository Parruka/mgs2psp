#!/usr/bin/env python3
"""l41_score.py PSPDIR_A FILE_A PSPDIR_B FILE_B [OBJDIR] (gl41s; python3 -I): window FILE_A minus window FILE_B, ms a PSP step [C model]
= (1.65 instr + 68 I-miss + 86 D-miss) / 333,000 / 80 steps (a window = vblanks 10-90 of a turn), as build_gl39/tools/score39.py.
Each file is named with its own build's symbols (MGS2T.elf + ovl_w00a.elf of its PSPDIR).  Side: ME = ge_* objects but ge_fxthin (the
picture code), game = the recompiled game (P<addr>), main-rt = the rest (OBJDIR's objects name the files; default: PSPDIR_A).
Then the gl41 groups: the functions each lever touches (boot_rt.o's VU0 call path, the box cull, the rehash), summed by name."""
import sys, subprocess, bisect, collections, glob, os, re
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
pa, fa, pb, fb = sys.argv[1:5]; od = sys.argv[5] if len(sys.argv) > 5 else pa


def syms(P):
    S = []
    for elf in (P + '/MGS2T.elf', P + '/ovl_w00a.elf'):
        for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
            f = l.split()
            if len(f) == 4 and f[2] in 'tTwW':
                S.append((int(f[0], 16), int(f[1], 16), f[3]))
    S.sort(); return S, [s[0] for s in S]


OBJ = {}
for o in glob.glob(od + '/*.o'):
    for l in subprocess.run([NM, '--defined-only', o], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 3 and f[1] in 'tTwW':
            OBJ.setdefault(f[2], os.path.basename(o)[:-2])
GAME = re.compile(r'^(?:S_\w+?_)?P[0-9a-f]{6}(\..*)?$')


def side(n):
    if GAME.match(n):
        return 'game'
    o = OBJ.get(n, OBJ.get(n.split('.')[0], '?'))
    return 'ME' if o.startswith('ge_') and o != 'ge_fxthin' else 'main-rt'


def load(P, p):
    S, st = syms(P); F = collections.defaultdict(lambda: [0, 0, 0]); hdr = ''
    for ln in open(p):
        if ln.startswith('#'):
            hdr = ln.strip(); continue
        f = ln.split()
        if len(f) < 4:
            continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else '(outside %07x)' % (pc & ~0xfffff)
        x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
    return F, hdr


ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
A, ha = load(pa, fa); B, hb = load(pb, fb)
D = {n: [(A.get(n, [0, 0, 0])[j] - B.get(n, [0, 0, 0])[j]) / 80.0 for j in range(3)] for n in set(A) | set(B)}
sd = collections.defaultdict(lambda: [0, 0, 0])
for n, d in D.items():
    s = side(n)
    for j in range(3):
        sd[s][j] += d[j]
print('# %s MINUS %s, ms a PSP step [C model]' % (fa, fb))
for s in ('game', 'main-rt', 'ME'):
    print('  %-8s %+8.3f ms  (instr %+10.0f  I-miss %+7.0f  D-miss %+7.0f a step)' % (s, ms(sd[s]), *sd[s]))
mn = [sd['game'][j] + sd['main-rt'][j] for j in range(3)]
print('  %-8s %+8.3f ms  (instr %+10.0f  I-miss %+7.0f  D-miss %+7.0f a step)' % ('main', ms(mn), *mn))
G = {'VU0 calls': re.compile(r'^(vu_run_shared|vu_run_p1_miss|vu_run|vu_aot_?|vuc_entry|vuc_dispatch|vuc_fns|vupv?_[0-9a-f]{8}_\d+(_p3)?)(\..*)?$'),
     ' of it box cull': re.compile(r'^(vupv_96936c0f_0|vupv_96936c0f_0_p3|vup_96936c0f_0)(\..*)?$'),
     'rehash (E6)': re.compile(r'^(vu_hash|vu_hash_e6|vu_fnv|vif0_mscal|vif0_run|memcpy)(\..*)?$')}
for g, rx in G.items():
    t = [0, 0, 0]; parts = []
    for n, d in sorted(D.items(), key=lambda kv: -abs(ms(kv[1]))):
        if rx.match(n):
            for j in range(3):
                t[j] += d[j]
            if abs(ms(d)) >= 0.0005:
                parts.append('%s %+.3f' % (n, ms(d)))
    print('  %-14s %+8.3f ms (instr %+9.0f I %+6.0f D %+6.0f): %s' % (g, ms(t), *t, ', '.join(parts[:8])))
rows = sorted(((n, d) for n, d in D.items() if side(n) != 'ME'), key=lambda kv: -abs(ms(kv[1])))
print('  -- main side, top 12: ms = instr + I-miss + D-miss parts')
for n, d in rows[:12]:
    print('   %+7.3f = %+6.3f %+6.3f %+6.3f  %s' % (ms(d), 1.65 * d[0] / 333000, 68 * d[1] / 333000, 86 * d[2] / 333000, n[:60]))
