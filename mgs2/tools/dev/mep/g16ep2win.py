#!/usr/bin/env python3
# g16ep2win.py <gl16ep2 log.txt> <gl16 log.txt>: probe 2's PSP run (task #163; me_edram_probe_notes.md "Probe 2"), g16epwin.py with mode 1
# read as the ME's copy into the eDRAM (probe 2 reached it; run 1's mode 1 ran at rest) and the MB copied shown for both modes. The game's turns,
# MGS2_MEP_CYCLE=1000:100:0,1,0,2, each window's mode from its own "mep vb W" line: 0 the ME at rest; 1 the copy into the eDRAM (run 1:
# no turn passed, so these ran at rest too: the noise floor); 2 the ME's copy into the 64 KB ring in main RAM as fast as it can (gl14mp's
# burst). Window line vb W = steps W-99..W. Per part of the route: modes 1 and 2 against the mean of their two mode-0 neighbours (median,
# each, and the median of each part of the step); mode 0 against gl16's everything-on windows at the same vblanks (gl16: k even all on;
# the ME started and resting, gl16's code at gl16's addresses). Windows holding a shot left out.
import re, sys, statistics as st
WRE = re.compile(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                 r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms')
MRE = re.compile(r'mep vb (\d+): mode (\d); ME (\d+) blocks, copied ([\d.]+) MB, busy ([\d.]+) ms')
def load(p):
    w, m = {}, {}
    for l in open(p, errors='replace'):
        a = WRE.search(l)
        if a: w[int(a.group(1))] = (int(a.group(2)),) + tuple(float(x) for x in a.groups()[2:]); continue
        b = MRE.search(l)
        if b: m[int(b.group(1))] = (int(b.group(2)), int(b.group(3)), float(b.group(4)), float(b.group(5)))
    return w, m
e, mm = load(sys.argv[1]); g, _ = load(sys.argv[2])
def on16(W): return W > 1000 and ((W - 1001) // 100) % 2 == 0
SHOT = set(((f + 99) // 100) * 100 for f in (8760, 10516, 15761, 23263, 24244, 24784, 29500, 30500, 31112, 31663, 32501, 33613, 34911, 35027, 35589, 36941))
PARTS = [('to 15,600', 1100, 15600), ('call 1', 15700, 19900), ('viewfinder', 20000, 26000), ('call 2', 26100, 28800),
         ('deck waiting', 29500, 30600), ('deck walk', 30700, 31300), ('inside', 31900, 37800)]
NAMES = ('logic', 'draw', 'IOP', 'GS walk', 'translator', 'picture', 'GE wait')
def mean(x): return sum(x) / len(x) if x else float('nan')
print('window lines gl16ep %d (mep lines %d), gl16 %d; minutes gl16ep %.2f, gl16 %.2f' % (len(e), len(mm), len(g),
      sum(v[0] * v[1] for v in e.values()) / 60000, sum(v[0] * v[1] for v in g.values()) / 60000))
md = {}
for W, v in mm.items(): md[W] = v[0]
for name, lo, hi in PARTS:
    out = []
    r0 = [W for W in range(lo, hi + 1, 100) if md.get(W) == 0 and W in e and W in g and on16(W) and W not in SHOT]
    if r0:
        out.append('rest vs gl16 all on n%d: %.2f -> %.2f (%+.2f, %+.1f%%)' % (len(r0), mean([g[W][1] for W in r0]), mean([e[W][1] for W in r0]),
                   mean([e[W][1] - g[W][1] for W in r0]), 100 * mean([e[W][1] - g[W][1] for W in r0]) / mean([g[W][1] for W in r0])))
    for M, lab in ((1, 'eDRAM copy'), (2, 'ring copy')):
        ds = []; rel = []; dp = [[] for _ in NAMES]; mb = []
        for W in range(lo, hi + 1, 100):
            if md.get(W) != M or W not in e or W - 100 not in e or W + 100 not in e: continue
            if md.get(W - 100) != 0 or md.get(W + 100) != 0 or W - 100 < lo or W + 100 > hi or ({W, W - 100, W + 100} & SHOT): continue
            nb = (e[W - 100][1] + e[W + 100][1]) / 2; ds.append(e[W][1] - nb); rel.append(100 * (e[W][1] - nb) / nb)
            for i in range(len(NAMES)): dp[i].append(e[W][2 + i] - (e[W - 100][2 + i] + e[W + 100][2 + i]) / 2)
            if W in mm: mb.append(mm[W][2])
        if ds:
            out.append('%s n%d med %+.2f ms (%+.1f%%) [%s]%s (parts: %s)' % (lab, len(ds), st.median(ds), st.median(rel),
                       ' '.join('%+.1f' % d for d in ds), (', ME copied a median %.0f MB a window' % st.median(mb)) if mb else '',
                       ', '.join('%s %+.2f' % (NAMES[i], st.median(dp[i])) for i in range(len(NAMES)) if abs(st.median(dp[i])) >= 0.005)))
    print(name + ': ' + ('; '.join(out) if out else '-'))
