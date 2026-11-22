# fndelta.py <dump off> <dump on> <steps> <elf> [n]: per-function model ms a step (fitted weights) of two PspProf dumps, the n largest changes
import sys, subprocess, bisect, collections
WI, WIM, WDM = 1.647, 68.2, 85.9
d0, d1, steps, elf = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]; n = int(sys.argv[5]) if len(sys.argv) > 5 else 30
syms = []
for l in subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-S', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    t = l.split()
    if len(t) == 4 and t[2] in 'tTwW': syms.append((int(t[0], 16), int(t[1], 16), t[3]))
syms.sort(); starts = [s[0] for s in syms]
def rd(p):
    acc = collections.defaultdict(lambda: [0, 0, 0])
    for l in open(p):
        if l.startswith('#'): continue
        t = l.split(); a = int(t[0], 16); i = bisect.bisect_right(starts, a) - 1
        name = syms[i][2] if i >= 0 and a < syms[i][0] + max(syms[i][1], 4) else '?%x' % a
        v = acc[name]; v[0] += int(t[1]); v[1] += int(t[2]); v[2] += int(t[3])
    return acc
def ms(v): return (WI * v[0] + WIM * v[1] + WDM * v[2]) / 333000.0 / steps
a, b = rd(d0), rd(d1)
rows = sorted(((ms(b.get(k, [0, 0, 0])) - ms(a.get(k, [0, 0, 0])), k) for k in set(a) | set(b)), key=lambda r: -abs(r[0]))
for dlt, k in rows[:n]:
    va, vb = a.get(k, [0, 0, 0]), b.get(k, [0, 0, 0])
    print('%-28s %7.2f -> %7.2f  %+6.2f   on: instr %.2f I %.2f D %.2f; calls-ish execs %d -> %d' % (k, ms(va), ms(vb), dlt, WI * vb[0] / 333000 / steps, WIM * vb[1] / 333000 / steps, WDM * vb[2] / 333000 / steps, va[0], vb[0]))
