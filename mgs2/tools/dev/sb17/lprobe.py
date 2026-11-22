# lprobe.py <x86 log with MGS2_LPROBE=1 lines>: the rain's streak calls (task #158 probe): per call its entry, strips, segments and the
# box of its on-screen vertices (clip x/w, y/w within -1..1); per frame (the lprobe lines before a "picture: vb N" line) the calls in order.
# Prints: how much of the screen each call's box spans (median, quartiles), and per frame whether the even and the odd calls (by order)
# and the 1-in-3 calls span the same box as all of them.
import re, sys, statistics as st
fr = []; cur = []; bad = [0]
for l in open(sys.argv[1], errors='replace'):
    F = r'(-?\d+\.\d\d)'
    m = re.search(r'lprobe: src (\w+) lo (\w+) entry (\d+) n (\d+) strips (\d+) segs (\d+) in (\d+) box ' + ' '.join([F] * 4) + ' mean ' + F + ' ' + F + r'$', l.rstrip('\n'))
    if 'lprobe:' in l and not m: bad[0] += 1
    if m:
        g = m.groups(); cur.append((int(g[2]), int(g[3]), int(g[4]), int(g[5]), int(g[6])) + tuple(float(x) for x in g[7:])); continue
    m = re.match(r'picture: vb (\d+) buffer', l)
    if m and cur: fr.append((int(m.group(1)), cur)); cur = []
calls = [c for _, cs in fr for c in cs]
print('probe lines cut by other output: %d' % bad[0])
print('frames with rain calls %d, calls %d, calls a frame: median %d, min %d, max %d' % (len(fr), len(calls), st.median([len(cs) for _, cs in fr]),
      min(len(cs) for _, cs in fr), max(len(cs) for _, cs in fr)))
print('n per call: %s; strips: %s; segs: %s' % (sorted(set(c[1] for c in calls))[:8], sorted(set(c[2] for c in calls))[:8], sorted(set(c[3] for c in calls))[:8]))
vis = [c for c in calls if c[4] > 0]
wx = [c[6] - c[5] for c in vis]; wy = [c[8] - c[7] for c in vis]
q = lambda a: '%.2f / %.2f / %.2f' % tuple(st.quantiles(a, n=4)) if len(a) > 3 else '-'
print('calls with vertices on screen %d of %d; their box width (of 2.0) q1/med/q3 %s, height %s; on-screen vertices a call %s' % (len(vis), len(calls), q(wx), q(wy), q([c[4] for c in vis])))
# per frame: the box of all calls' on-screen vertices against the even calls' and the 1-in-3 calls'
def box(cs):
    v = [c for c in cs if c[4] > 0]
    if not v: return None
    return (min(c[5] for c in v), max(c[6] for c in v), min(c[7] for c in v), max(c[8] for c in v))
def area(b): return (b[1] - b[0]) * (b[3] - b[2]) if b else 0.0
r2 = []; r3 = []
for vb, cs in fr:
    a = area(box(cs))
    if a <= 0: continue
    r2.append(area(box(cs[0::2])) / a); r3.append(area(box(cs[0::3])) / a)
print('frames %d: the even calls\' box / all calls\' box (area) q1/med/q3 %s; 1 in 3 %s' % (len(r2), q(r2), q(r3)))
for vb, cs in fr[len(fr) // 2: len(fr) // 2 + 2]:
    print('vb %d: %d calls' % (vb, len(cs)))
    for c in cs: print('  entry %2d strips %2d segs %2d in %3d box x %5.2f..%5.2f y %5.2f..%5.2f mean %5.2f %5.2f' % ((c[0],) + c[2:]))
