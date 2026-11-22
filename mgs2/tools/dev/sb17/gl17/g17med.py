# g17med.py <gl16 log> <gl17 log>: gl17's PSP run (task #156). Window line vb W = steps W-99..W in the setting of k = (W - 1001) // 100:
# gl17: k even the effects off (C4 + C4X off, MGS2_OPT2 fffff9df), k odd on (ffffffdf); both with the rain as Ffm chose (the drop
# sheets whole, no streaks, the rain's own sprites left out; the splash 1/2), C1 on, C2 off, step 3's trims on.
# gl16: k even everything on (the trims on, C1 on, C2 off; the drop sheets 1/2), k odd the trims off.
# Per part: the effects off, gl17 against gl16 at the same vblanks (k even in both: everything gl17 changed but the effects, means);
# the effects on against the mean of their two effects-off neighbours (median, each, and the median of each part of the step);
# windows holding a shot left out.
import re, sys, statistics as st
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                     r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
        if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
    return w
a = load(sys.argv[1]); b = load(sys.argv[2])
def k(W): return (W - 1001) // 100 if W > 1000 else -1
def even(W): return W > 1000 and k(W) % 2 == 0
SHOT = set(((f + 99) // 100) * 100 for f in (8760, 10516, 15761, 23263, 24244, 24784, 29500, 30500, 31112, 31663, 32501, 33613, 34911, 35027, 35589, 36941))
PARTS = [('to 15,600', 100, 15600), ('call 1', 15700, 19900), ('viewfinder', 20000, 26000), ('call 2', 26100, 28800),
         ('deck waiting', 29500, 30600), ('deck walk', 30700, 31300), ('deck other', None, None), ('inside', 31900, 37800)]
NAMES = ('logic', 'draw', 'IOP', 'GS walk', 'translator', 'picture', 'GE wait')
def wins(lo, hi):
    return list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
def mean(x): return sum(x) / len(x) if x else float('nan')
print('window lines gl16 %d, gl17 %d; minutes gl16 %.2f, gl17 %.2f' % (len(a), len(b), sum(v[0] * v[1] for v in a.values()) / 60000,
      sum(v[0] * v[1] for v in b.values()) / 60000))
for name, lo, hi in PARTS:
    ws = [W for W in wins(lo, hi) if W in b]
    inr = lambda W: lo is None or (lo <= W and W <= hi)
    out = []
    off = [W for W in ws if even(W) and W in a and W not in SHOT]
    if off:
        d = [b[W][1] - a[W][1] for W in off]
        out.append('effects off n%d: gl16 %.2f -> gl17 %.2f (%+.2f, %+.1f%%; parts %s)' % (len(off), mean([a[W][1] for W in off]),
                   mean([b[W][1] for W in off]), mean(d), 100 * mean(d) / mean([a[W][1] for W in off]),
                   ', '.join('%s %+.2f' % (NAMES[i], mean([b[W][2 + i] - a[W][2 + i] for W in off])) for i in range(len(NAMES)))))
    ds = []; dp = [[] for _ in NAMES]
    for W in ws:
        if W > 1000 and not even(W) and W - 100 in b and W + 100 in b and inr(W - 100) and inr(W + 100) and not ({W, W - 100, W + 100} & SHOT):
            ds.append(b[W][1] - (b[W - 100][1] + b[W + 100][1]) / 2)
            for i in range(len(NAMES)): dp[i].append(b[W][2 + i] - (b[W - 100][2 + i] + b[W + 100][2 + i]) / 2)
    if ds:
        out.append('effects on n%d med %+.2f [%s] (parts: %s)' % (len(ds), st.median(ds), ' '.join('%+.1f' % d for d in ds),
                   ', '.join('%s %+.2f' % (NAMES[i], st.median(dp[i])) for i in range(len(NAMES)))))
    print(name + ': ' + ('; '.join(out) if out else '-'))
