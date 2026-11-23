import sys, re, collections
prof, *dis = sys.argv[1:]
M = {}; F = {}; cur = None
for d in dis:
    for l in open(d):
        m = re.match(r'^([0-9a-f]{8}) <([^>]+)>:', l)
        if m: cur = m.group(2); continue
        m = re.match(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)', l)
        if m: pc = int(m.group(1), 16); M[pc] = (m.group(2), m.group(3)); F[pc] = cur
gen = re.compile(r'^(S_\w+?_)?P[0-9a-f]{6}|^nat_')
R = []
for l in open(prof):
    if l.startswith('#'): continue
    f = l.split()
    if len(f) < 4: continue
    pc = int(f[0], 16); dm = int(f[3])
    if dm and pc in F and F[pc] and gen.match(F[pc]): R.append((dm / 80.0, int(f[1]) / 80.0, pc, F[pc], M[pc]))
R.sort(reverse=True); tot = sum(r[0] for r in R)
print('D-misses a step in generated code + nat_: %.0f; top 40 PCs = %.0f' % (tot, sum(r[0] for r in R[:40])))
fn = collections.Counter()
for r in R: fn[r[3]] += r[0]
for r in R[:40]: print('%7.0f  exec %8.0f  %08x %-24s %s %s' % (r[0], r[1], r[2], r[3][:24], r[4][0], r[4][1]))
