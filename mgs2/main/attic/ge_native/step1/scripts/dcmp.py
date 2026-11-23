# dcmp.py A.log.gz B.log.gz: the picture digests of two twin logs, kind by kind (list, frame: from 'picture: vb N buffer b list L frame F';
# content, drawn, drawn2: 'picture: vb N <kind> H'), vblank by vblank: equal, differing (the first 5 shown), only in one log.
import sys, gzip, re
def rd(p):
    d = {}
    op = gzip.open if p.endswith('.gz') else open
    with op(p, 'rt', errors='replace') as f:
        for l in f:
            if not l.startswith('picture: vb '): continue
            m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+)', l)
            if m:
                v = int(m.group(1)); d.setdefault('list', {})[v] = m.group(2); d.setdefault('frame', {})[v] = m.group(3); continue
            m = re.match(r'picture: vb (\d+) (content|drawn|drawn2) ([0-9a-f]+)\s*$', l)
            if m: d.setdefault(m.group(2), {})[int(m.group(1))] = m.group(3)
    return d
a, b = rd(sys.argv[1]), rd(sys.argv[2])
lo = int(sys.argv[3]) if len(sys.argv) > 3 else 0
for k in ('list', 'frame', 'content', 'drawn', 'drawn2'):
    x, y = a.get(k, {}), b.get(k, {})
    x = {v: h for v, h in x.items() if v >= lo}; y = {v: h for v, h in y.items() if v >= lo}
    both = sorted(set(x) & set(y)); diff = [v for v in both if x[v] != y[v]]
    print('%-7s both %6d equal %6d DIFFER %6d only A %d only B %d%s' % (k, len(both), len(both) - len(diff), len(diff), len(set(x) - set(y)),
          len(set(y) - set(x)), ('; first ' + ' '.join(map(str, diff[:5]))) if diff else ''))
