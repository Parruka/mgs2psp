#!/usr/bin/env python3
"""analyze.py <dir> <tag> <route d|w>: the m1: report of a run (sites with their function and context, the zero rule's places) and its
hashes against the v21 references."""
import re, sys, glob, bisect, subprocess
D, T, R = sys.argv[1:4]
W = '/home/claude/m1den/W'
sites = {}
for f in glob.glob(W + '/*.sites'):
    for l in open(f):
        p = l.rstrip('\n').split(' ', 4)
        if len(p) == 5: sites[int(p[0])] = (p[1], p[2], p[3], p[4])
hdr = {}
def enclosing(unit, idx, kind):
    fn = W + ('/gen_elf.h' if unit == 'elf' else '/gen_s_%s.h' % unit)
    if fn not in hdr: hdr[fn] = open(fn).read()
    s = hdr[fn]; k = s.find(('RDFS(%du,' if kind == 'lwc1' else 'I2FS(%du,') % idx)
    if k < 0: return '?'
    m = None
    for m in re.finditer(r'\n(?:static )?(?:void|int) ((?:S_[a-z0-9_]+_)?P[0-9a-f]+)\(void\)\s*\{', s[max(0, k - 400000):k]): pass
    return m.group(1) if m else '?'
log = open('%s/run_%s.log' % (D, T)).read().splitlines()
for l in log:
    if l.startswith('m1: mode') or l.startswith('m1: base'): print(l)
recs = [l for l in log if l.startswith('m1: site')]
print('sites with denormal operands: %d' % len(recs))
for l in recs:
    m = re.match(r'm1: site (\d+) pc (\w+) (\w+) n (\d+) vb (-?\d+)-(-?\d+) first a (\w+) u (\w+) last a (\w+) u (\w+)', l)
    idx = int(m.group(1)); unit, pc, kind, ctx = sites.get(idx, ('?', '?', '?', ''))
    print('  %s %s pc %s %s: n %s, vb %s-%s, a %s u %s (last a %s u %s)' % (unit, enclosing(unit, idx, kind), m.group(2), m.group(3), m.group(4), m.group(5), m.group(6), m.group(7), m.group(8), m.group(9), m.group(10)))
    print('      ' + ctx[:230])
rules = [l for l in log if l.startswith('m1: rule')]
base = [l for l in log if l.startswith('m1: base')]
if rules and base:
    rt = int(base[0].split()[2], 16)
    nm = subprocess.run(['nm', '-n', D + '/boot'], capture_output=True, text=True).stdout.split('\n')
    syms = [(int(x.split()[0], 16), x.split()[2]) for x in nm if len(x.split()) == 3 and x.split()[1] in 'tTdDbB']
    gh = [a for a, n in syms if n == 'g_hash_vb'][0]; off = rt - gh
    funcs = sorted((a, n) for a, n in syms if True)
    addrs = [a for a, n in funcs]
    print('zero-rule places: %d' % len(rules))
    for l in rules:
        m = re.match(r'm1: rule (\w+) at (\w+) n (\d+) vb (-?\d+)-(-?\d+) first (\w+) (\w+)', l)
        ra = int(m.group(2), 16) - off; i = bisect.bisect_right(addrs, ra) - 1
        print('  %s in %s+0x%x: n %s, vb %s-%s, operands %s %s' % (m.group(1), funcs[i][1], ra - funcs[i][0], m.group(3), m.group(4), m.group(5), m.group(6), m.group(7)))
def load(fn, cols):
    d = {}
    for l in open(fn):
        p = l.split()
        if len(p) >= 1 + cols: d[int(p[0])] = p[1:1 + cols]
    return d
ref = '/mnt/project-files/mgs2/prof/hashes_x86_%s_v21.txt' % ('deck' if R == 'd' else 'w01a')
den = '/mnt/project-files/mgs2/prof/hashref_x86_%s_v21.txt' % ('deck' if R == 'd' else 'w01a')
a = load(ref, 3); b = load('%s/%s.ref' % (D, T), 3)
print('every-100 hashes against %s: %d points in the run, %d in the reference' % (ref.split('/')[-1], len(b), len(a)))
for i, n in enumerate(('ee', 'iop', 'vfb')):
    df = [v for v in sorted(set(a) & set(b)) if a[v][i] != b[v][i]]
    print('  %s: %d of %d differ%s' % (n, len(df), len(set(a) & set(b)), (', first vb %d' % df[0]) if df else ''))
a = load(den, 2); b = load('%s/%s.dense' % (D, T), 2)
for i, n in enumerate(('ee', 'vfb')):
    df = [v for v in sorted(set(a) & set(b)) if a[v][i] != b[v][i]]
    print('  dense %s: %d of %d differ%s' % (n, len(df), len(set(a) & set(b)), (', first vb %d' % df[0]) if df else ''))
au = {}; bu = {}
for fn, dd in ((den, au), ('%s/%s.dense' % (D, T), bu)):
    for l in open(fn):
        p = l.split()
        if len(p) >= 4: dd[int(p[0])] = p[3]
df = [v for v in sorted(set(au) & set(bu)) if au[v] != bu[v]]
print('  vuh: %d of %d differ%s' % (len(df), len(set(au) & set(bu)), (', first vb %d' % df[0]) if df else ''))
