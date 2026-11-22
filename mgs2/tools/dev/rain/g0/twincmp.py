# twincmp.py a.log b.log: the twin's two modes compared: hash fields per vb, time lines, picture list/frame/content digests, xlive digests
import re, sys
def load(p):
    h, t, pic, xl = {}, {}, {}, {}
    for ln in open(p, errors='replace'):
        if ln.startswith('hash: vb '):
            f = ln.split(); vb = int(f[2]); d = dict(zip(f[3::2], f[4::2])); h[vb] = d
        elif ln.startswith('time: vb '):
            f = ln.split(); t[int(f[2])] = ln.strip()
        elif ln.startswith('picture: vb '):
            m = re.match(r'picture: vb (\d+) buffer (\d+) list (\S+) frame (\S+):', ln)
            if m: pic[(int(m.group(1)), 'lf', m.group(2))] = (m.group(3), m.group(4))
            m = re.match(r'picture: vb (\d+) content (\S+)', ln)
            if m: pic[(int(m.group(1)), 'c')] = m.group(2)
        elif ln.startswith('xlive: vb '):
            f = ln.split()
            if f[2].isdigit() and len(f) == 4: xl[int(f[2])] = f[3]
    return h, t, pic, xl
A, B = load(sys.argv[1]), load(sys.argv[2])
ha, hb = A[0], B[0]; common = sorted(set(ha) & set(hb))
print('hash points', len(ha), len(hb), 'common', len(common))
for k in ('ee', 'iop', 'gs', 'gsd', 'vfb'):
    diff = [vb for vb in common if ha[vb].get(k) != hb[vb].get(k)]
    print(' %-4s differ at %d%s' % (k, len(diff), (': ' + ', '.join(map(str, diff[:12])) + (' ...' if len(diff) > 12 else '') + (' last %d' % diff[-1] if diff else '')) if diff else ''))
ta, tb = A[1], B[1]; tc = set(ta) & set(tb)
print('time lines', len(ta), len(tb), 'differ', sum(ta[v] != tb[v] for v in tc))
pa, pb = A[2], B[2]; pc = set(pa) & set(pb)
nlf = sum(1 for k in pc if k[1] == 'lf'); nc = sum(1 for k in pc if k[1] == 'c')
dl = sorted(k for k in pc if pa[k] != pb[k])
print('picture: list/frame lines %d, content lines %d (a %d b %d keys); differ %d' % (nlf, nc, len(pa), len(pb), len(dl)), dl[:8])
xa, xb = A[3], B[3]; xc = set(xa) & set(xb)
dx = sorted(v for v in xc if xa[v] != xb[v])
print('xlive digests', len(xa), len(xb), 'differ', len(dx), dx[:8])
