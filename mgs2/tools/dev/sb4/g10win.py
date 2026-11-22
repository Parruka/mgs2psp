#!/usr/bin/env python3
# g10win.py <gl9 log> <gl10 log>: gl10's PSP run against gl9's run 1 (task #116).
# Window line vb W holds steps W-99..W, all but the first in the setting of ((W - 1001) // 100):
#   gl9  MGS2_OPT_CYCLE=1000:100:ffffffff,fff0ffff                    k % 2: 0 everything on, 1 the A-D codec speed-ups off
#   gl10 MGS2_OPT_CYCLE=1000:100:ffffffff,f7ffffff,ffffffff,f87fffff  k % 4: 0 and 2 everything on, 1 step c off, 3 the four trims off
# So gl10's all-on windows are exactly gl9's all-on windows (k even) and compare one for one.
# gl10 takes three more shots, at vb 30,601, 30,701 and 30,801 (the steps after them start with cold caches); the windows holding them
# (30,700, 30,800, 30,900) are left out of every comparison and printed on their own. The other shots are the same in both runs.
# Each part: minutes in both runs; everything on, gl10 - gl9 by field; each off setting against the mean of its two all-on neighbours
# in gl10's own run (what step c and the trims save on the PSP); the model's figures for comparison.
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
def s10(W): return k(W) % 4 if W > 1000 else 0
def mean(xs): return sum(xs) / len(xs) if xs else float('nan')
SHOTWIN = (30700, 30800, 30900)
a = load(sys.argv[1]); b = load(sys.argv[2])
both = set(a) & set(b)
print('window lines: gl9 %d, gl10 %d, both %d; last gl9 %d, gl10 %d' % (len(a), len(b), len(both), max(a), max(b) if b else 0))
PARTS = [('to vb 15,600', 100, 15600), ('call 1 (15,601-19,900)', 15700, 19900), ('viewfinder (19,901-26,000)', 20000, 26000),
         ('call 2 (26,001-28,800)', 26100, 28800), ('deck waiting (29,401-30,600)', 29500, 30600), ('deck walk (30,601-31,300)', 30700, 31300),
         ('deck other (28,801-29,400, 31,301-31,800)', None, None), ('inside (31,801-37,800)', 31900, 37800)]
MODEL = {'deck waiting (29,401-30,600)': 'model -6.5%, est. 169.26 -> ~158', 'deck walk (30,601-31,300)': 'model -6.5% (vb 30,810-30,890)',
         'inside (31,801-37,800)': 'model -5.0%, est. 103.98 -> ~98.8'}
def wins(lo, hi):
    ws = list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
    return [W for W in ws if W in both]
ta = tb = 0.0
for name, lo, hi in PARTS:
    ws = wins(lo, hi)
    ma = sum(a[W][0] * a[W][1] for W in ws) / 60000; mb = sum(b[W][0] * b[W][1] for W in ws) / 60000; ta += ma; tb += mb
    print('%s: %d windows, %.2f -> %.2f min (%+.1f%%)%s' % (name, len(ws), ma, mb, 100 * (mb - ma) / ma if ma else 0,
          ('; ' + MODEL[name]) if name in MODEL else ''))
    ws = [W for W in ws if W not in SHOTWIN]
    on = [W for W in ws if W <= 1000 or k(W) % 2 == 0]
    if on:
        ma_ = mean([a[W][1] for W in on]); mb_ = mean([b[W][1] for W in on])
        parts = ', '.join('%s %+.2f' % (f, mean([b[W][i] - a[W][i] for W in on])) for i, f in enumerate(FIELDS)
                          if 2 <= i <= 8 and abs(mean([b[W][i] - a[W][i] for W in on])) >= 0.05)
        print('    everything on, %2d windows: gl9 %.2f, gl10 %.2f ms a step: %+.2f (%+.1f%%); %s' % (
            len(on), ma_, mb_, mb_ - ma_, 100 * (mb_ - ma_) / ma_, parts))
    for s, lab in ((1, 'step c off'), (3, 'trims off')):
        off = [W for W in ws if W > 1000 and s10(W) == s]
        pairs = [W for W in off if W - 100 in b and W + 100 in b and W - 100 not in SHOTWIN and W + 100 not in SHOTWIN
                 and (lo is None or (lo <= W - 100 and W + 100 <= hi))]
        if not pairs: continue
        nb = lambda W, i: (b[W - 100][i] + b[W + 100][i]) / 2
        d = mean([b[W][1] - nb(W, 1) for W in pairs]); r = mean([(b[W][1] - nb(W, 1)) / nb(W, 1) for W in pairs])
        parts = ', '.join('%s %+.2f' % (f, mean([b[W][i] - nb(W, i) for W in pairs])) for i, f in enumerate(FIELDS)
                          if 2 <= i <= 8 and abs(mean([b[W][i] - nb(W, i) for W in pairs])) >= 0.05)
        print('    %-10s %2d windows against their on neighbours: %+.2f ms a step (%+.1f%%), so it saves %.2f; %s' % (
            lab, len(pairs), d, 100 * r, d, parts))
print('all windows: %.2f -> %.2f min' % (ta, tb))
for W in SHOTWIN:
    if W in a and W in b:
        print('shot window %d (gl10 setting %d): gl9 %.2f ms (max %.1f), gl10 %.2f ms (max %.1f)' % (W, s10(W), a[W][1], a[W][9], b[W][1], b[W][9]))
