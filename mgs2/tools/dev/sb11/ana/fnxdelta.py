# fnxdelta.py <dumpA> <elfsA comma> <dumpB> <elfsB comma> <steps> [n]: per-function model ms a step of two builds' PspProf dumps, by name
import sys, subprocess, bisect, collections
WI, WIM, WDM = 1.647, 68.2, 85.9
def syms_of(elfs):
    s = []
    for e in elfs.split(','):
        for l in subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-S', '-n', e], capture_output=True, text=True).stdout.splitlines():
            t = l.split()
            if len(t) == 4 and t[2] in 'tTwW': s.append((int(t[0], 16), int(t[1], 16), t[3]))
    s.sort(); return s, [x[0] for x in s]
def rd(p, elfs):
    syms, starts = syms_of(elfs); acc = collections.defaultdict(lambda: [0, 0, 0])
    for l in open(p):
        if l.startswith('#'): continue
        t = l.split(); a = int(t[0], 16); i = bisect.bisect_right(starts, a) - 1
        name = syms[i][2] if i >= 0 and a < syms[i][0] + max(syms[i][1], 4) else '?'
        v = acc[name]; v[0] += int(t[1]); v[1] += int(t[2]); v[2] += int(t[3])
    return acc
st = float(sys.argv[5]); n = int(sys.argv[6]) if len(sys.argv) > 6 else 40
a, b = rd(sys.argv[1], sys.argv[2]), rd(sys.argv[3], sys.argv[4])
def ms(v): return (WI * v[0] + WIM * v[1] + WDM * v[2]) / 333000.0 / st
def parts(v): return WI * v[0] / 333000 / st, WIM * v[1] / 333000 / st, WDM * v[2] / 333000 / st
rows = sorted(((ms(b.get(k, [0, 0, 0])) - ms(a.get(k, [0, 0, 0])), k) for k in set(a) | set(b)), key=lambda r: -abs(r[0]))
for dlt, k in rows[:n]:
    va, vb = a.get(k, [0, 0, 0]), b.get(k, [0, 0, 0]); pa, pb = parts(va), parts(vb)
    print('%-30s %7.2f -> %7.2f %+6.2f  instr %+5.2f I %+5.2f D %+5.2f  (D-misses %d -> %d, instr %d -> %d)' % (k, ms(va), ms(vb), dlt,
          pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2], va[2], vb[2], va[0], vb[0]))
