#!/usr/bin/env python3
"""mkpc36.py EMU_LOG OUT  (gl36; python3 -I): rd36pc.py from rd36pc.tpl.py with the fingerprints of an emulator run of the delivered
package (the hash lines, the digest lines with and without the dz text, the digests by block of 100 vblanks), computed as rd36pc.py
computes them from a PSP log."""
import sys, re, hashlib, os
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
H, D = [], []
for s in open(sys.argv[1], encoding='latin-1').read().splitlines():
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append((int(m.group(1)), '%s %s %s' % m.groups()[:3], m.group(4).strip()))
bl = {}
for vb, a, z in D: bl.setdefault(vb // 100, []).append(a + ' ' + z)
b0, b1 = min(bl), max(bl) + 1
t = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rd36pc.tpl.py')).read()
for k, v in (('@@EH@@', md(H)), ('@@NH@@', str(len(H))), ('@@ED@@', md([a for _, a, _ in D])), ('@@EDZ@@', md([a + ' ' + z for _, a, z in D])),
             ('@@ND@@', str(len(D))), ('@@B0@@', str(b0)), ('@@B1@@', str(b1)), ('@@EB@@', ' '.join(md(bl.get(k, [])) for k in range(b0, b1)))):
    t = t.replace(k, v)
assert '@@' not in t
open(sys.argv[2], 'w').write(t)
print('hash lines %d %s; digest lines %d %s / %s; blocks %d00..%d00' % (len(H), md(H), len(D), md([a for _, a, _ in D]), md([a + ' ' + z for _, a, z in D]), b0, b1 - 1))
