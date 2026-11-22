#!/usr/bin/env python3
# g14win.py <gl13 log> <gl14 log>: gl14's PSP run against gl13's (task #136), g12win.py's way (tools/dev/sb8) with gl14's nine windows.
# Window line vb W holds steps W-99..W, all but the first in the setting of k = (W - 1001) // 100:
#   gl13 MGS2_OPT2_CYCLE (4 windows): k % 4 0 and 2 everything on, 1 the texture memo off, 3 the CLUT loads by identity off
#   gl14 (9 windows): k % 9 0, 3, 5, 7 everything on; 1 GE overlap off (C1 off); 2 GE overlap off with C1 on; 4 T1 off; 6 T2 off; 8 C1 on
# Each part: minutes in both runs; everything on, gl14 - gl13 (gl13's window at the same vblanks when it was all on, else the mean of
# the two around it, which were); T1 off, T2 off and C1 on against the mean of their two all-on neighbours in gl14's own run (what each
# saves; C1 on: what C1 changes); the two GE windows side by side (C1's cost to the GE: F - E) and the first against its all-on
# neighbour before it (the step's cost of not overlapping).  The windows of the three deck shots at vb 30,601, 30,701, 30,801 (30,700,
# 30,800, 30,900) are left out and printed on their own, as before.
import re, sys
FIELDS = ('steps', 'mean', 'logic', 'dp', 'iop', 'gs', 'xl', 'pic', 'gw', 'max')
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                     r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
        if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
    return w
def k(W): return (W - 1001) // 100
def s9(W): return k(W) % 9 if W > 1000 else 0
def on13(W): return W <= 1000 or k(W) % 2 == 0
ON = (0, 3, 5, 7)
def mean(xs): return sum(xs) / len(xs) if xs else float('nan')
SHOTWIN = (30700, 30800, 30900)
a = load(sys.argv[1]); b = load(sys.argv[2])
both = set(a) & set(b)
print('window lines: gl13 %d, gl14 %d, both %d; last gl13 %d, gl14 %d' % (len(a), len(b), len(both), max(a) if a else 0, max(b) if b else 0))
PARTS = [('to vb 15,600', 100, 15600), ('call 1 (15,601-19,900)', 15700, 19900), ('viewfinder (19,901-26,000)', 20000, 26000),
         ('call 2 (26,001-28,800)', 26100, 28800), ('deck waiting (29,401-30,600)', 29500, 30600), ('deck walk (30,601-31,300)', 30700, 31300),
         ('deck other (28,801-29,400, 31,301-31,800)', None, None), ('inside (31,801-37,800)', 31900, 37800)]
MODEL = {'deck waiting (29,401-30,600)': 'model +1.5% (data cache placement), est. 150.52 -> ~152.8', 'deck walk (30,601-31,300)': 'model +1.5% (vb 30,810-30,890)',
         'inside (31,801-37,800)': 'model +1.1%, est. 91.90 -> ~92.9 (+-2.5)', 'call 1 (15,601-19,900)': 'model -5.8% (codec route), est. 64.57 -> ~60.8; C1 on -2.0% more',
         'call 2 (26,001-28,800)': 'model -5.8% (codec route), est. 66.44 -> ~62.6; C1 on -2.0% more'}   # psp_harness.md "Build gl14"
def wins(lo, hi, src):
    ws = list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
    return [W for W in ws if W in src]
def a13(W, i):   # gl13 with everything on at W's vblanks: its own window or the mean of the two around it
    if W in a and on13(W): return a[W][i]
    if W - 100 in a and W + 100 in a and on13(W - 100) and on13(W + 100): return (a[W - 100][i] + a[W + 100][i]) / 2
    return None
def fld(fn, ws):
    return ', '.join('%s %+.2f' % (f, mean([fn(W, i) for W in ws])) for i, f in enumerate(FIELDS) if 2 <= i <= 8 and abs(mean([fn(W, i) for W in ws])) >= 0.05)
ta = tb = 0.0
for name, lo, hi in PARTS:
    wa, wb = wins(lo, hi, a), wins(lo, hi, b)
    ma = sum(a[W][0] * a[W][1] for W in wa) / 60000; mb = sum(b[W][0] * b[W][1] for W in wb) / 60000; ta += ma; tb += mb
    print('%s: %d / %d windows, %.2f -> %.2f min (%+.1f%%)%s' % (name, len(wa), len(wb), ma, mb, 100 * (mb - ma) / ma if ma else 0,
          ('; ' + MODEL[name]) if name in MODEL else ''))
    ws = [W for W in wb if W not in SHOTWIN]
    on = [W for W in ws if s9(W) in ON and a13(W, 1) is not None]
    if on:
        ma_ = mean([a13(W, 1) for W in on]); mb_ = mean([b[W][1] for W in on])
        print('    everything on, %2d windows: gl13 %.2f, gl14 %.2f ms a step: %+.2f (%+.1f%%); %s' % (
            len(on), ma_, mb_, mb_ - ma_, 100 * (mb_ - ma_) / ma_, fld(lambda W, i: b[W][i] - a13(W, i), on)))
    for s, lab in ((4, 'T1 off'), (6, 'T2 off'), (8, 'C1 on')):
        pairs = [W for W in ws if W > 1000 and s9(W) == s and W - 100 in b and W + 100 in b and W - 100 not in SHOTWIN and W + 100 not in SHOTWIN
                 and (lo is None or (lo <= W - 100 and W + 100 <= hi))]
        if not pairs: continue
        nb = lambda W, i: (b[W - 100][i] + b[W + 100][i]) / 2
        d = mean([b[W][1] - nb(W, 1) for W in pairs]); r = mean([(b[W][1] - nb(W, 1)) / nb(W, 1) for W in pairs])
        print('    %-7s %2d windows against their on neighbours: %+.2f ms a step (%+.1f%%)%s; %s' % (
            lab, len(pairs), d, 100 * r, (', so it saves %.2f' % d) if s != 8 else '', fld(lambda W, i: b[W][i] - nb(W, i), pairs)))
    ge = [W for W in ws if W > 1000 and s9(W) == 1 and W + 100 in b and W - 100 in b and W + 100 not in SHOTWIN and W - 100 not in SHOTWIN
          and (lo is None or (lo <= W - 100 and W + 100 <= hi))]
    if ge:
        print('    GE overlap off, %2d pairs: C1 off %.2f ms a step (GE wait %.2f), C1 on %.2f (GE wait %.2f); C1 on - off %+.2f (GE wait %+.2f); '
              'C1 off against the all-on window before it %+.2f (GE wait %+.2f)' % (
              len(ge), mean([b[W][1] for W in ge]), mean([b[W][8] for W in ge]), mean([b[W + 100][1] for W in ge]), mean([b[W + 100][8] for W in ge]),
              mean([b[W + 100][1] - b[W][1] for W in ge]), mean([b[W + 100][8] - b[W][8] for W in ge]),
              mean([b[W][1] - b[W - 100][1] for W in ge]), mean([b[W][8] - b[W - 100][8] for W in ge])))
print('all windows: %.2f -> %.2f min' % (ta, tb))
for W in SHOTWIN:
    if W in a and W in b:
        print('shot window %d (gl14 setting %d): gl13 %.2f ms (max %.1f), gl14 %.2f ms (max %.1f)' % (W, s9(W), a[W][1], a[W][9], b[W][1], b[W][9]))
