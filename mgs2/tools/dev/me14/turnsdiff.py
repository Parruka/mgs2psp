# turnsdiff.py <ref log> <run log>: differing frames of a run in turns (from vb 1,000, 100 vblanks each, 4 configurations) by configuration
import sys, re, collections
rl = re.compile(r'^picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+)')
def load(fn):
    p = {}
    for line in open(fn, errors='replace'):
        m = rl.match(line)
        if m: p[int(m.group(1))] = (m.group(2), m.group(3))
    return p
a, b = load(sys.argv[1]), load(sys.argv[2])
d = sorted(v for v in b if v in a and a[v] != b[v])
byc = collections.Counter(((v - 1000) // 100) % 4 if v >= 1000 else -1 for v in d)
tot = collections.Counter(((v - 1000) // 100) % 4 if v >= 1000 else -1 for v in b if v in a)
print('differing frames', len(d), 'by configuration (index: differing / frames):', {k: '%d/%d' % (byc[k], tot[k]) for k in sorted(tot)})
runs = []; 
for v in d:
    if runs and v - runs[-1][1] <= 3: runs[-1][1] = v
    else: runs.append([v, v])
print('spans:', ', '.join('%d-%d' % (x, y) if x != y else str(x) for x, y in runs[:40]), '...' if len(runs) > 40 else '')
