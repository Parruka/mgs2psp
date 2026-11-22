# picstretch.py <no-copy log> <copy log> <reference log> <edges...>: the frames that differ only without the copy, by stretch of vb
import re, sys, bisect
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
n, s, r = (pics(p) for p in sys.argv[1:4]); E = [int(x) for x in sys.argv[4:]]
dn = {v for v in r if v in n and n[v] != r[v]}; ds = {v for v in r if v in s and s[v] != r[v]}
cnt = {}; tot = {}
for v in r:
    k = bisect.bisect_right(E, v); tot[k] = tot.get(k, 0) + 1
    if v in dn - ds: cnt[k] = cnt.get(k, 0) + 1
lo = [0] + E
for k in sorted(tot): print('vb %d-%s: %d of %d frames differ only without the copy' % (lo[k], (E[k] - 1) if k < len(E) else 'end', cnt.get(k, 0), tot[k]))
