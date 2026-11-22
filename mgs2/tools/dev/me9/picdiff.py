# picdiff.py <log a> <log b>: frames ("picture: vb" list/frame and content lines) of two x86 runs: counts, and the differing frames as
# ranges (consecutive shown frames, step 2), with the first differing line of each range
import re, sys
def pics(p):
    d = {}
    for l in open(p, errors='replace'):
        if l.startswith('picture: vb '):
            m = re.match(r'picture: vb (\d+) (buffer \d+ list (\S+) frame (\S+).*|content (\S+))', l)
            if not m: continue
            vb = int(m.group(1))
            if m.group(5): d.setdefault(vb, {})['c'] = m.group(5)
            else: d.setdefault(vb, {})['l'] = l.split(': ', 1)[1].strip()
    return d
a, b = pics(sys.argv[1]), pics(sys.argv[2])
common = sorted(set(a) & set(b)); diff = [v for v in common if a[v] != b[v]]
ld = [v for v in diff if a[v].get('l', '').split(' frame ')[0] != b[v].get('l', '').split(' frame ')[0]]
print('frames a', len(a), 'b', len(b), 'common', len(common), 'differ', len(diff), '(list digest too:', len(ld), ')')
rs = []
for v in diff:
    if rs and v - rs[-1][1] <= 4: rs[-1][1] = v; rs[-1][2] += 1
    else: rs.append([v, v, 1])
print('ranges', len(rs))
for r in rs[:60]: print('  %d-%d: %d frames' % (r[0], r[1], r[2]))
