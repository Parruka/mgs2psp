#!/usr/bin/env python3
# g7win.py <gl7 log> [<gl6 log>]: gl7's window lines by setting (MGS2_OPT_CYCLE=1000:100:ffffffff,fff0ffff: window line vb W holds steps
# W-99..W, all but the first in setting ((W - 1001) // 100) % 2: 0 all on, 1 all off) in the route's parts, each all-off window against
# the mean of its two all-on neighbours; with gl6's log (six settings, ((W - 1001) // 100) % 6: 0 all on, 5 all off), the windows where
# both ran all on or both all off, gl7 - gl6 (the background fix's own cost in the story calls).
import re, sys
FIELDS = ('mean', 'logic', 'dp', 'iop', 'gs', 'xl', 'pic', 'gw')
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                     r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\)', l)
        if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
    return w
PARTS = [('call 1 (vb 15,641-19,899)', 15800, 19800), ('call 2 (vb 26,068-28,760)', 26200, 28700),
         ('aft deck (vb 29,101-31,500)', 29200, 31500), ('inside (vb 31,801-37,800)', 31900, 37800)]
def s7(W): return ((W - 1001) // 100) % 2
def s6(W): return ((W - 1001) // 100) % 6
def mean(xs): return sum(xs) / len(xs) if xs else float('nan')
g7 = load(sys.argv[1]); g6 = load(sys.argv[2]) if len(sys.argv) > 2 else None
print('gl7 window lines', len(g7), (' gl6 %d' % len(g6)) if g6 else '')
for name, a, b in PARTS:
    on = [W for W in range(a, b + 1, 100) if W in g7 and s7(W) == 0]; off = [W for W in range(a, b + 1, 100) if W in g7 and s7(W) == 1]
    pairs = [(W, (g7[W - 100][1] + g7[W + 100][1]) / 2) for W in off if W - 100 in g7 and W + 100 in g7 and a <= W - 100 and W + 100 <= b]
    print('%s: all on %d windows mean %.2f ms; all off %d windows mean %.2f ms; off against its on neighbours (%d): %+.2f ms (%+.1f%%)' % (
        name, len(on), mean([g7[W][1] for W in on]), len(off), mean([g7[W][1] for W in off]), len(pairs),
        mean([g7[W][1] - n for W, n in pairs]), 100 * mean([(g7[W][1] - n) / n for W, n in pairs])))
    for i, f in enumerate(FIELDS[1:], 2):
        d = mean([g7[W][i] - (g7[W - 100][i] + g7[W + 100][i]) / 2 for W, n in pairs])
        if abs(d) >= 0.05: print('    %-6s off - on %+.2f ms' % (f, d))
    if g6:
        for lab, k6, k7 in (('all on', 0, 0), ('all off', 5, 1)):
            ws = [W for W in range(a, b + 1, 100) if W in g7 and W in g6 and s6(W) == k6 and s7(W) == k7]
            if not ws: continue
            d = [g7[W][1] - g6[W][1] for W in ws]
            print('    gl7 - gl6, both %s, %d windows (%s): %+.2f ms a step (gl6 %.2f, gl7 %.2f); picture %+.2f, translator %+.2f, GS walk %+.2f' % (
                lab, len(ws), ','.join(str(W) for W in ws), mean(d), mean([g6[W][1] for W in ws]), mean([g7[W][1] for W in ws]),
                mean([g7[W][7] - g6[W][7] for W in ws]), mean([g7[W][6] - g6[W][6] for W in ws]), mean([g7[W][5] - g6[W][5] for W in ws])))
