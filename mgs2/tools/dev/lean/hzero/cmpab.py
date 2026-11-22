#!/usr/bin/env python3
"""cmpab.py <runlog A> <runlog B>: compare the hash lines of two route runs field by field (ee, iop, gs, vfb, vuh)."""
import sys
def load(p):
    h, u = {}, {}
    for l in open(p, errors='replace'):
        f = l.split()
        if l.startswith('hash: vb'):
            d = dict(zip(f[3::2], f[4::2])); h[int(f[2])] = d
        elif l.startswith('vuh: vb'):
            u[int(f[2])] = f[3]
    return h, u
(ha, ua), (hb, ub) = load(sys.argv[1]), load(sys.argv[2])
vbs = sorted(set(ha) & set(hb))
print('hash lines: A %d, B %d, both %d' % (len(ha), len(hb), len(vbs)))
for k in ('ee', 'iop', 'gs', 'vfb'):
    eq = [v for v in vbs if ha[v].get(k) == hb[v].get(k)]
    ne = [v for v in vbs if ha[v].get(k) != hb[v].get(k)]
    print('%-4s equal %d, differ %d%s' % (k, len(eq), len(ne), (' (first %s)' % ne[:6]) if ne else ''))
vu = sorted(set(ua) & set(ub)); ne = [v for v in vu if ua[v] != ub[v]]
print('vuh  equal %d, differ %d%s' % (len(vu) - len(ne), len(ne), (' (first %s)' % ne[:6]) if ne else ''))
