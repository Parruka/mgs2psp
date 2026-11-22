# picsets.py <no-copy log> <copy log> <reference log>: frames differing from the reference in each run, and their overlap
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
n, s, r = (pics(p) for p in sys.argv[1:4])
lst = lambda d, v: d[v].get('l', '').split(' frame ')[0]
dn = {v for v in r if v in n and n[v] != r[v]}; ds = {v for v in r if v in s and s[v] != r[v]}
ln = {v for v in dn if lst(n, v) != lst(r, v)}; ls_ = {v for v in ds if lst(s, v) != lst(r, v)}
print('frames: no copy differ', len(dn), 'lists', len(ln), '| copy differ', len(ds), 'lists', len(ls_))
print('both differ', len(dn & ds), '| only no copy', len(dn - ds), '(lists', len(ln - ls_), ') | only copy', len(ds - dn))
same = sum(1 for v in dn & ds if n[v] == s[v]); print('of the frames both differ at, no copy equal to copy', same, 'of', len(dn & ds))
