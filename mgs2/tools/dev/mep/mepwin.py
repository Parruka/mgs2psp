#!/usr/bin/env python3
# mepwin.py <gl14mp log.txt> [<gl14 log.txt> | -] [<gl14mp battery.txt>]: the ME probe's run (gl14mp, task #132; mgs2/me_probe_notes.md), g14win.py's way.
# The boot probe's "mep" lines are printed as they are.  The turns, MGS2_MEP_CYCLE=1000:100:0,1,0,2,0,3: main_psp.c's window line W
# holds steps W-99..W, all in the mode of k = (W - 1001) // 100, cyc[k % 6]: 0 the ME at rest, 1 P (the paced stream), 2 B (the burst
# copy), 3 D (the DMA copy, the ME at rest).  Each P, B and D window against the mean of its two rest neighbours (what the main CPU's
# step loses to that traffic), per part of the route; the probe's own "mep vb W" line (counts of steps W-100..W-1: 99 of window W's
# steps and the last step of the window before) gives what the ME and the DMA moved.  The traffic runs between the steps too (the state
# hashes, the waits), so it is taken per second of the window: the ME's MB over its busy time (Count; it streams the whole window), the
# DMA's over its periods (made + dropped, 2,757 us each).  Scaled to the picture's need [I]: the main CPU's loss times the rate of 3.4 MB
# a step at the rest neighbours' step time (me_picture_study.md section 3) over the rate measured, on the assumption that the loss grows
# with the rate.  With gl14's
# log: the rest windows against gl14's everything-on windows at the same vblanks (the cost of the ME started and resting, with
# gl14mp's own code placement).  The windows of the three deck shots (30,700, 30,800, 30,900) are left out, as in g14win.py.
import re, sys
CYC = (0, 1, 0, 2, 0, 3); NAME = {0: 'rest', 1: 'P paced', 2: 'B burst', 3: 'D DMA'}
FIELDS = ('steps', 'mean', 'logic', 'dp', 'iop', 'gs', 'xl', 'pic', 'gw', 'max')
WRE = re.compile(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                 r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms')
MRE = re.compile(r'mep vb (\d+): mode (\d); ME (\d+) blocks, read ([\d.]+) MB, wrote ([\d.]+) MB, busy ([\d.]+) ms, rests (\d+) .*?kicks (\d+); '
                 r'DMA (\d+) calls, ([\d.]+) MB, ([\d.]+) ms in the calls, ([\d.]+) ms writing back, (\d+) errors(?: \(last [0-9a-f]+\), (\d+) periods dropped)?')
def load(p):
    w, m, boot, other = {}, {}, [], []
    for l in open(p, errors='replace'):
        a = WRE.search(l)
        if a: w[int(a.group(1))] = (int(a.group(2)),) + tuple(float(x) for x in a.groups()[2:]); continue
        b = MRE.search(l)
        if b: m[int(b.group(1))] = tuple(float(x) if x is not None else 0.0 for x in b.groups()[1:]); continue
        if l.startswith('mep'): (boot if not m else other).append(l.rstrip())
    return w, m, boot, other
def mode(W): return CYC[((W - 1001) // 100) % 6] if W > 1000 else 0
def s9(W): return ((W - 1001) // 100) % 9 if W > 1000 else 0
def mean(xs): return sum(xs) / len(xs) if xs else float('nan')
SHOTWIN = (30700, 30800, 30900)
PARTS = [('to vb 15,600', 100, 15600), ('call 1 (15,601-19,900)', 15700, 19900), ('viewfinder (19,901-26,000)', 20000, 26000),
         ('call 2 (26,001-28,800)', 26100, 28800), ('deck waiting (29,401-30,600)', 29500, 30600), ('deck walk (30,601-31,300)', 30700, 31300),
         ('deck other (28,801-29,400, 31,301-31,800)', None, None), ('inside (31,801-37,800)', 31900, 37800)]
def wins(lo, hi, src):
    ws = list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
    return [W for W in ws if W in src]
w, mp, boot, other = load(sys.argv[1])
g = load(sys.argv[2])[0] if len(sys.argv) > 2 and sys.argv[2] != '-' else None
# Count is 32 bits at 331.343 MHz and wraps every 12.962 s, so the busy time of a longer window (the deck's 100 steps of ~150 ms, the
# dense hashes' windows) reads short by whole wraps: with the run's battery.txt (third argument; its lines carry "t <s> s, vb <V>:"
# every 5 s) the window's wall time is interpolated and the wraps that bring the busy time nearest it are added (the ME streams the
# whole window in P and B); without it, the window's own step time (a lower bound) is used.
BRE = re.compile(r' t (\d+) s, vb (\d+):'); bt = []
if len(sys.argv) > 3:
    for l in open(sys.argv[3], errors='replace'):
        a = BRE.search(l)
        if a and (not bt or int(a.group(2)) > bt[-1][0]): bt.append((int(a.group(2)), int(a.group(1))))
def wall(V):
    for i in range(1, len(bt)):
        if bt[i][0] >= V: return bt[i - 1][1] + (bt[i][1] - bt[i - 1][1]) * (V - bt[i - 1][0]) / (bt[i][0] - bt[i - 1][0])
    return None
def busy(W, x):
    a, b = (wall(W - 100), wall(W)) if bt else (None, None)
    span = (b - a) * 1000 if a is not None and b is not None else w[W][0] * w[W][1]
    return x[4] + 12962.0 * max(0, round((span - x[4]) / 12962.0))
print('the boot probe (%d lines):' % len(boot))
for l in boot: print('  ' + l)
if other:
    print('other "mep" lines in the game (%d):' % len(other))
    for l in other[:20]: print('  ' + l)
print('window lines %d, "mep vb" lines %d; last window %d' % (len(w), len(mp), max(w) if w else 0))
bad = [W for W in mp if int(mp[W][0]) != mode(W - 1)]
if bad: print('MODE MISMATCH in the "mep vb" lines at', bad[:10])
tot = 0.0
for name, lo, hi in PARTS:
    ws = wins(lo, hi, w); mins = sum(w[W][0] * w[W][1] for W in ws) / 60000; tot += mins
    print('%s: %d windows, %.2f min' % (name, len(ws), mins))
    ws = [W for W in ws if W not in SHOTWIN]
    for m in (1, 2, 3):
        pairs = [W for W in ws if W > 1000 and mode(W) == m and W - 100 in w and W + 100 in w and W - 100 not in SHOTWIN
                 and W + 100 not in SHOTWIN and (lo is None or (lo <= W - 100 and W + 100 <= hi))]
        if not pairs: continue
        nb = lambda W, i: (w[W - 100][i] + w[W + 100][i]) / 2
        d = mean([w[W][1] - nb(W, 1) for W in pairs]); r = mean([(w[W][1] - nb(W, 1)) / nb(W, 1) for W in pairs])
        parts = ', '.join('%s %+.2f' % (f, mean([w[W][i] - nb(W, i) for W in pairs])) for i, f in enumerate(FIELDS)
                          if 2 <= i <= 8 and abs(mean([w[W][i] - nb(W, i) for W in pairs])) >= 0.05)
        # what moved: the probe's line at the same vblank covers 99 of the window's steps and the step before them
        mv = [(W, mp[W]) for W in pairs if W in mp]
        if m in (1, 2):
            mb = sum(x[2] for _, x in mv); sec = sum(busy(W, x) for W, x in mv) / 1000
            if sec <= 0: sec = sum(w[W][0] * w[W][1] for W, _ in mv) / 1000   # (no Count: the steps' own time, a lower bound)
            what = 'the ME read %.2f MB a window, %.1f MB/s' % (mb / max(len(mv), 1), mb / sec if sec > 0 else 0)
        else:
            mb = sum(x[8] for _, x in mv); sec = sum((x[7] + x[12]) * 2757e-6 for _, x in mv)
            what = 'the DMA copied %.2f MB a window, %.1f MB/s, %.1f ms a window in its calls, %d errors, %d periods dropped' % (
                mb / max(len(mv), 1), mb / sec if sec > 0 else 0, sum(x[9] for _, x in mv) / max(len(mv), 1), sum(x[11] for _, x in mv), sum(x[12] for _, x in mv))
        rate = mb / sec if sec > 0 else 0; need = 3.4 / (mean([nb(W, 1) for W in pairs]) / 1000)
        sc = (' -> at 3.4 MB a step (%.1f MB/s) %+.2f ms [I]' % (need, d * need / rate)) if rate > 0 else ''
        print('    %-8s %2d windows against their rest neighbours: %+.2f ms a step (%+.1f%%); %s; %s%s' % (NAME[m], len(pairs), d, 100 * r, parts, what, sc))
    if g:
        on = [W for W in ws if mode(W) == 0 and W in g and s9(W) in (0, 3, 5, 7)]
        if on:
            d = mean([w[W][1] - g[W][1] for W in on])
            print('    rest     %2d windows against gl14 everything on at the same vblanks: %+.2f ms a step (gl14 %.2f)' % (len(on), d, mean([g[W][1] for W in on])))
print('all windows: %.2f min' % tot)
