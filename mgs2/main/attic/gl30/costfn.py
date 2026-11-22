import sys, subprocess, collections
# costfn.py ELF OBJ MODEL FRAMES [label] (gl30 G4, 2026-11-20): PspProf's per-PC counts (MODEL: '# total ...' then 'pc count imiss dmiss')
# summed over the functions OBJ defines (ge_gelist.o: the GE list builder; its functions found in ELF by name and size), with the port's
# fitted cost model: ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 [C model]; per frame = / FRAMES.
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
elf, obj, mod, frames = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]); lab = sys.argv[5] if len(sys.argv) > 5 else ''
def syms(p):
    out = subprocess.run([NM, '-S', '--defined-only', p], capture_output=True, text=True).stdout.split('\n')
    R = []
    for l in out:
        f = l.split()
        if len(f) == 4 and f[2] in 'tT': R.append((f[3], int(f[0], 16), int(f[1], 16)))
    return R
want = {}
for n, a, s in syms(obj): want.setdefault(n, set()).add(s)
rng = []; miss = []
E = collections.defaultdict(list)
for n, a, s in syms(elf): E[n].append((a, s))
for n, ss in want.items():
    c = [(a, s) for (a, s) in E.get(n, []) if s in ss]
    if len(c) != 1: miss.append((n, len(c))); continue
    rng.append((c[0][0], c[0][0] + c[0][1], n))
rng.sort()
import bisect
st = [r[0] for r in rng]
F = collections.defaultdict(lambda: [0, 0, 0]); tot = None
for l in open(mod):
    if l.startswith('#'): tot = l.strip(); continue
    f = l.split()
    if len(f) < 4: continue
    pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
    if k >= 0 and pc < rng[k][1]:
        x = F[rng[k][2]]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
S = [sum(F[n][j] for n in F) for j in range(3)]
print('%s %s: gelist functions %d found in the ELF (not found or not unique: %s)' % (lab, mod, len(rng), miss))
print('%s window total: %s' % (lab, tot))
print('%s gelist: instructions %d, I-misses %d, D-misses %d = %.2f ms in the window, %.3f ms a frame over %d frames [C model]' % (
    lab, S[0], S[1], S[2], ms(S), ms(S) / frames, frames))
for n in sorted(F, key=lambda n: -ms(F[n]))[:12]:
    x = F[n]; print('   %-14s instr %9d  I-miss %7d  D-miss %7d  %.3f ms/frame' % (n, x[0], x[1], x[2], ms(x) / frames))
