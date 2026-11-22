# g15med.py <gl14 log> <gl15 log>: gl15's PSP run (task #141). Window line vb W = steps W-99..W in the setting of k = (W - 1001) // 100:
# gl15 k % 9: 0, 3, 5, 7 everything on (C1 on, C2 off); 1 GE overlap off, C2 off; 2 GE overlap off, C2 on; 4, 6, 8 C2 on.
# gl14 k % 9: 0, 3, 5, 7 everything on (C1 off); others T1 / T2 off, C1 on, GE pairs.
# Per part: everything on, gl15 against gl14's all-on windows at the same vblanks (means); C2 on against the mean of its two all-on
# neighbours (median and each); the GE pair (C2 on - off with the overlap off: step and GE wait); windows holding a shot left out.
import re, sys, statistics as st
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                     r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
        if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
    return w
a = load(sys.argv[1]); b = load(sys.argv[2])
def s9(W): return ((W - 1001) // 100) % 9 if W > 1000 else 0
ON = (0, 3, 5, 7)
SHOT = set(((f + 99) // 100) * 100 for f in (8760, 10516, 15761, 23263, 24244, 24784, 29500, 30500, 31112, 31663, 32501, 33613, 34911, 35027, 35589, 36941))
SHOT14 = (30700, 30800, 30900)
PARTS = [('to 15,600', 100, 15600), ('call 1', 15700, 19900), ('viewfinder', 20000, 26000), ('call 2', 26100, 28800),
         ('deck waiting', 29500, 30600), ('deck walk', 30700, 31300), ('deck other', None, None), ('inside', 31900, 37800)]
def wins(lo, hi):
    return list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
def mean(x): return sum(x) / len(x) if x else float('nan')
print('window lines gl14 %d, gl15 %d; minutes gl14 %.2f, gl15 %.2f' % (len(a), len(b), sum(v[0] * v[1] for v in a.values()) / 60000, sum(v[0] * v[1] for v in b.values()) / 60000))
for name, lo, hi in PARTS:
    ws = [W for W in wins(lo, hi) if W in b]
    inr = lambda W: lo is None or (lo <= W and W <= hi)
    out = []
    on = [W for W in ws if s9(W) in ON and W in a and W not in SHOT and W not in SHOT14]
    if on:
        out.append('all on n%d: gl14 %.2f -> gl15 %.2f (%+.2f, %+.1f%%; GE wait %.2f -> %.2f)' % (len(on), mean([a[W][1] for W in on]), mean([b[W][1] for W in on]),
                   mean([b[W][1] - a[W][1] for W in on]), 100 * mean([b[W][1] - a[W][1] for W in on]) / mean([a[W][1] for W in on]),
                   mean([a[W][8] for W in on]), mean([b[W][8] for W in on])))
    ds = []; dpic = []
    for W in ws:
        if W > 1000 and s9(W) in (4, 6, 8) and W - 100 in b and W + 100 in b and inr(W - 100) and inr(W + 100) and not ({W, W - 100, W + 100} & SHOT):
            ds.append(b[W][1] - (b[W - 100][1] + b[W + 100][1]) / 2); dpic.append(b[W][7] - (b[W - 100][7] + b[W + 100][7]) / 2)
    if ds: out.append('C2 on n%d med %+.2f (picture %+.2f) [%s]' % (len(ds), st.median(ds), st.median(dpic), ' '.join('%+.1f' % d for d in ds)))
    ge = [W for W in ws if W > 1000 and s9(W) == 1 and W + 100 in b and W - 100 in b and inr(W - 100) and inr(W + 100) and not ({W, W - 100, W + 100} & SHOT)]
    if ge:
        d1 = [b[W + 100][1] - b[W][1] for W in ge]; g1 = [b[W + 100][8] - b[W][8] for W in ge]; d0 = [b[W][1] - b[W - 100][1] for W in ge]
        out.append('GE pair n%d: C2 on-off step med %+.2f [%s], GE wait med %+.2f [%s]; overlap off vs on before med %+.2f' % (len(ge), st.median(d1),
                   ' '.join('%+.1f' % d for d in d1), st.median(g1), ' '.join('%+.2f' % d for d in g1), st.median(d0)))
    print(name + ': ' + ('; '.join(out) if out else '-'))
