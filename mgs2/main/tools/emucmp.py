#!/usr/bin/env python3
"""emucmp.py <emu log.txt> <x86 twin log>: the PSP / emulator run's frame digests ("picture: vb N buffer B list L frame F", the cfg's
MGS2_GL_DIG window) and vuh lines against the x86 twin's lines for the same vblanks (the twin prints them from 0 to 37,800).
Exit status 0 when every one is equal.  Run it with python3 -I (mainline tree, 2026-11-18)."""
import re, sys
def rd(p):
    P = {}; V = {}
    for l in open(p, errors='replace'):
        m = re.match(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+)', l)
        if m: P.setdefault(int(m.group(1)), []).append((m.group(2), m.group(3)))
        m = re.match(r'vuh: vb (\d+) (\w+)', l)
        if m: V[int(m.group(1))] = m.group(2)
    return P, V
a, va = rd(sys.argv[1]); b, vb = rd(sys.argv[2])
dp = [k for k in sorted(a) if b.get(k) != a[k]]; dv = [k for k in sorted(va) if vb.get(k) != va[k]]
print('frames: %d vblanks with digests, %d equal to the twin%s; %d distinct' % (len(a), len(a) - len(dp), (', first differing %d' % dp[0]) if dp else '',
      len(set(x for k in a for x in a[k]))))
print('vuh: %d lines, %d equal to the twin%s' % (len(va), len(va) - len(dv), (', first differing %d' % dv[0]) if dv else ''))
sys.exit(0 if a and not dp and not dv else 1)
