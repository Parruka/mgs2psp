# agg.py OUT: aggregate rd33pc's TURNS table: gl33 vs gl31 by window, turn kind and arm (ms a step; ME turn fields ms a step = /100)
import sys, re
rows = []
for s in open(sys.argv[1]):
    m = re.match(r'(\d+) (me|ma) \| (.*)', s.strip())
    if not m: continue
    a, k = int(m.group(1)), m.group(2)
    g33, g31 = m.group(3).split('||')
    def nums(t): return [None if x == '-' else float(x) for x in re.split(r'[\s|/]+', t.strip()) if x]
    x, y = nums(g33), nums(g31)
    # gl33: mean logic gew | me walks fl idle wfill | arm inpl nat copy ram ; gl31: mean logic gew | me walks fl idle wfill | arm nat copy ram
    rows.append(dict(a=a, k=k, m3=x[0], lg3=x[1], me3=x[3], w3=x[4], fl3=x[5], id3=x[6], wf3=x[7], inpl=x[9], nat3=x[10],
                     m1=y[0], lg1=y[1], me1=y[3], w1=y[4], fl1=y[5], id1=y[6], wf1=y[7], arm1=y[8], nat1=y[9]))
W = [('opening 24000-24999', 24000, 25000), ('deck 25000-29499', 25000, 29500), ('wait 29600-30499', 29600, 30500), ('walk 30500-31399', 30500, 31400)]
def av(v): v = [z for z in v if z is not None]; return sum(v) / len(v) if v else float('nan')
for name, lo, hi in W:
    print('==', name)
    for k in ('me', 'ma'):
        for inpl in (0, 1):
            R = [r for r in rows if lo <= r['a'] < hi and r['k'] == k and r['inpl'] == inpl]
            if not R: continue
            arms = sorted(set(r['arm1'] for r in R))
            line = '  %s inpl %d (gl31 arm %s) n=%d: mean %.2f vs %.2f (%+.2f); logic %.2f vs %.2f (%+.2f)' % (k, inpl, arms, len(R),
                   av([r['m3'] for r in R]), av([r['m1'] for r in R]), av([r['m3'] - r['m1'] for r in R]),
                   av([r['lg3'] for r in R]), av([r['lg1'] for r in R]), av([r['lg3'] - r['lg1'] for r in R]))
            if k == 'me':
                line += '; ME/step: walks %.2f vs %.2f (%+.2f), lists %.2f vs %.2f, idle %.2f vs %.2f, main waited fills %.2f vs %.2f, native %.2f vs %.2f' % (
                    av([r['w3'] for r in R]) / 100, av([r['w1'] for r in R]) / 100, av([r['w3'] - r['w1'] for r in R]) / 100,
                    av([r['fl3'] for r in R]) / 100, av([r['fl1'] for r in R]) / 100, av([r['id3'] for r in R]) / 100, av([r['id1'] for r in R]) / 100,
                    av([r['wf3'] for r in R]) / 100, av([r['wf1'] for r in R]) / 100, av([r['nat3'] for r in R]) / 100, av([r['nat1'] for r in R]) / 100)
            print(line)
