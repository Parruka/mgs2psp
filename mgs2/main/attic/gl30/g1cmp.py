import sys, re, collections
# g1cmp.py OLD.log NEW.log [lo hi] (gl30 G1 / G2, 2026-11-20): the twin's digest lines of gl29x (OLD, out_x1.log) against gl30's (NEW):
# per frame (vb), the list and frame digests, the content digest and the dz counts.  G1: equal digests in every frame where NEW has no
# subset pair (sub 0); in the frames with one, NEW's pairs + subset pairs = OLD's pairs + OLD's vertex-set fallbacks.  G2: NEW's
# fallbacks by reason over vb lo..hi (default 29,490-31,400), and the census paired / subset / fallbacks.
PIC = re.compile(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+): .*?(; dz (\S+) 42:(\d+) oth:(\d+) pair:(\d+)(?: sub:(\d+))? '
                 r'fb:(\d+),(\d+),(\d+),(\d+),(\d+),(\d+) bias:(\d+) w:(\d+) aux:(\d+))?$')
CON = re.compile(r'picture: vb (\d+) content (\w+)$')
def rd(p):
    D = {}; C = {}
    for l in open(p, errors='replace'):
        l = l.rstrip('\n')
        m = PIC.match(l)
        if m:
            g = m.groups()
            d = dict(list=g[1], frame=g[2], arm=g[4])
            if g[3]:
                d.update(n42=int(g[5]), oth=int(g[6]), pair=int(g[7]), sub=int(g[8] or 0), fb=[int(x) for x in g[9:15]], aux=int(g[17]))
            D[int(g[0])] = d; continue
        m = CON.match(l)
        if m: C[int(m[1])] = m[2]
    return D, C
lo, hi = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (29490, 31400)
A, CA = rd(sys.argv[1]); B, CB = rd(sys.argv[2])
print('digest lines: old %d, new %d; content lines: old %d, new %d' % (len(A), len(B), len(CA), len(CB)))
print('same frames (vb):', sorted(A) == sorted(B), '; content digests equal in all:', CA == CB)
t = collections.Counter(); bad = []; subfr = []
for vb in sorted(B):
    a, b = A.get(vb), B[vb]
    if a is None: bad.append((vb, 'not in old')); continue
    if b.get('sub', 0) == 0:
        t['frames without XS'] += 1
        if (a['list'], a['frame']) == (b['list'], b['frame']): t['  digests equal'] += 1
        else: bad.append((vb, 'digest differs without XS'))
        if 'n42' in b and (a['n42'], a['pair'], a['fb']) != (b['n42'], b['pair'], b['fb']): bad.append((vb, 'counts differ without XS'))
    else:
        t['frames with XS'] += 1; subfr.append(vb)
        if (a['list'], a['frame']) != (b['list'], b['frame']): t['  digests differ'] += 1
        if a['n42'] == b['n42'] and a['pair'] == b['pair'] and a['fb'][2] - b['sub'] == b['fb'][2] and a['fb'][:2] + a['fb'][3:] == b['fb'][:2] + b['fb'][3:]:
            t['  counts: old vertex-set fallbacks = new subset pairs + new vertex-set fallbacks'] += 1
        else: bad.append((vb, 'counts', a, b))
for k in t: print('%-80s %d' % (k, t[k]))
for x in bad[:20]: print('BAD', x)
# G2: the census over lo..hi (frames whose vb lies there and whose arm is X)
S = collections.Counter(); fbr = [0] * 6; per = collections.Counter(); mx = (0, 0)
for vb in sorted(B):
    b = B[vb]
    if not (lo <= vb <= hi) or 'n42' not in b: continue
    S['frames'] += 1; S['arm %s' % b['arm']] += 1; S['0x42 targets'] += b['n42']; S['paired (exact)'] += b['pair']; S['subset (XS)'] += b['sub']
    for k in range(6): fbr[k] += b['fb'][k]
    per[sum(b['fb'])] += 1
    if b['aux'] > mx[0]: mx = (b['aux'], vb)
print('G2 census vb %d-%d:' % (lo, hi), dict(S))
print('   fallbacks base/matrix/vertex set/triangles/uv-colour/room:', fbr, '; frames by fallback count:', dict(per))
print('   the largest copy %d B (vb %d)' % mx)
print('   old over the same range:', sum(A[v]['pair'] for v in A if lo <= v <= hi and 'n42' in A[v]), 'paired,',
      [sum(A[v]['fb'][k] for v in A if lo <= v <= hi and 'n42' in A[v]) for k in range(6)], 'fallbacks')
print('frames with XS: %d, from vb %s to %s' % (len(subfr), subfr[0] if subfr else '-', subfr[-1] if subfr else '-'))
