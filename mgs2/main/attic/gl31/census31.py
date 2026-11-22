"""census31.py TWIN.log [lo hi] (gl31, the lead's update: arm X on the whole route): the 0x42 shadow decals' census from a twin log's
digest lines ('picture: vb N buffer .. list .. frame ..: ...; dz <arm> 42:n oth:n pair:n sub:n fb:a,b,c,d,e,f bias:n w:n aux:n'), per
stage (the log's 'stage code: <name>' lines, each visit apart, in order) and in all: frames, frames by arm, frames with a target, 0x42 targets (XFORM
decals with the depth test), paired exactly, subset pairs (XS), fallbacks by kind (base / matrix / vertex set / triangles / uv-colour /
room), 0x42 batches of other kinds (drawn as before, never targets), the largest copy.  lo hi: also the same counts over that vb range."""
import sys, re, collections
PIC = re.compile(r'picture: vb (\d+) buffer \d+ list \w+ frame \w+: .*?; dz (\S+) 42:(\d+) oth:(\d+) pair:(\d+)(?: sub:(\d+))? '
                 r'fb:(\d+),(\d+),(\d+),(\d+),(\d+),(\d+) bias:(\d+) w:(\d+) aux:(\d+)')
def new(): return dict(frames=0, arms=collections.Counter(), tf=0, n42=0, oth=0, pair=0, sub=0, fb=[0] * 6, aux=(0, 0), vb=[None, None], unp=[])
def add(s, vb, g):
    arm, n42, oth, pair, sub = g[1], int(g[2]), int(g[3]), int(g[4]), int(g[5] or 0); fb = [int(x) for x in g[6:12]]; aux = int(g[14])
    s['frames'] += 1; s['arms'][arm] += 1; s['tf'] += n42 > 0; s['n42'] += n42; s['oth'] += oth; s['pair'] += pair; s['sub'] += sub
    s['fb'] = [a + b for a, b in zip(s['fb'], fb)]
    if aux > s['aux'][0]: s['aux'] = (aux, vb)
    if s['vb'][0] is None: s['vb'][0] = vb
    s['vb'][1] = vb
    if arm == 'X' and pair + sub != n42 and len(s['unp']) < 6: s['unp'].append((vb, n42 - pair - sub))
lo, hi = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (None, None)
S = collections.OrderedDict(); cur = '0 boot'; nv = 0; T = new(); R = new()
for l in open(sys.argv[1], errors='replace'):
    if l.startswith('stage code: '): nv += 1; cur = '%d %s' % (nv, l.split()[2]); continue
    m = PIC.match(l)
    if not m: continue
    vb = int(m[1]); add(S.setdefault(cur, new()), vb, m.groups()); add(T, vb, m.groups())
    if lo is not None and lo <= vb <= hi: add(R, vb, m.groups())
def row(n, s):
    print('%-12s vb %6s-%-6s frames %6d (%s) with a target %5d | 0x42 targets %6d  paired %6d  subset %5d  fallbacks %s = %d | other kinds %6d | '
          'largest copy %5d B%s%s' % (n, s['vb'][0], s['vb'][1], s['frames'], ' '.join('%s %d' % kv for kv in sorted(s['arms'].items())), s['tf'],
          s['n42'], s['pair'], s['sub'], '/'.join(map(str, s['fb'])), sum(s['fb']), s['oth'], s['aux'][0], (' (vb %d)' % s['aux'][1]) if s['aux'][0] else '',
          ('; X frames with targets not paired (vb, n): %s' % s['unp']) if s['unp'] else ''))
print('%s: the 0x42 census by stage (fallbacks: base/matrix/vertex set/triangles/uv-colour/room)' % sys.argv[1])
for n, s in S.items(): row(n, s)
row('all', T)
if lo is not None: row('%d-%d' % (lo, hi), R)
