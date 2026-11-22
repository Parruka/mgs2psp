# digcmp.py <x86 log> <PSP log.txt>: the frames' list and frame digests ("picture: vb N buffer B list L frame F") of an x86 check run
# and a PSP (emulator or hardware) run of the same route, frame by frame (numbers only)
import sys, re
pat = re.compile(r'picture: vb (\d+) buffer (\d) list ([0-9a-f]{16}) frame ([0-9a-f]{16})')
def rd(p):
    d = {}
    for line in open(p, errors='replace'):
        m = pat.search(line)
        if m: d[int(m.group(1))] = (m.group(2), m.group(3), m.group(4))
    return d
a, b = rd(sys.argv[1]), rd(sys.argv[2])
both = sorted(set(a) & set(b)); dl = [v for v in both if a[v][1] != b[v][1]]; df = [v for v in both if a[v][2] != b[v][2]]
db = [v for v in both if a[v][0] != b[v][0]]
print('frames: x86 %d, PSP %d, both %d; list digests differ %d, frame digests differ %d, buffers differ %d' % (len(a), len(b), len(both), len(dl), len(df), len(db)))
if dl: print('first list differences at vb', dl[:10])
if df: print('first frame differences at vb', df[:10])
only = sorted(set(a) ^ set(b))
if only: print('in one run only: %d, first %s' % (len(only), only[:10]))
