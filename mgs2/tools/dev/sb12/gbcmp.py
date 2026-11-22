# gbcmp.py <new log> <reference log>: the per-frame "xlive gb: vb N:" lines of two x86 runs with the data KB taken out: every other field
# (batches, vertices, primitives, textures, calls, the depth range "depth: meshes lo..hi (calls, vertices)" and "map lo..hi") must be equal
import sys, re
KB = re.compile(r', \d+ KB data,')
def rd(p):
    d = {}
    for l in open(p, errors='replace'):
        if l.startswith('xlive gb: vb '):
            v = int(l.split()[3].rstrip(':')); d[v] = KB.sub(', KB data,', l.rstrip('\n'))
    return d
a, r = rd(sys.argv[1]), rd(sys.argv[2])
ks = sorted(set(a) | set(r)); diff = [k for k in ks if a.get(k) != r.get(k)]
dep = re.compile(r'depth: .*$')
dd = [k for k in ks if (dep.search(a.get(k, '')) or [None]) and (dep.search(a.get(k, '')).group(0) if dep.search(a.get(k, '')) else None) != (dep.search(r.get(k, '')).group(0) if dep.search(r.get(k, '')) else None)]
print('frames: new %d, reference %d; lines differing without the data KB: %d %s; depth parts differing: %d %s' % (len(a), len(r), len(diff), diff[:5], len(dd), dd[:5]))
for k in diff[:3]:
    print('  new', a.get(k, '')[:600]); print('  ref', r.get(k, '')[:600])
