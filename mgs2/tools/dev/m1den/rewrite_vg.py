#!/usr/bin/env python3
"""rewrite_vg.py <header>...: in place, vu_vgrp( -> M1VG(0x<pc>u,  (the pc: the last /*hex*/ before it); guest.h M1VGT logs each run."""
import re, sys
pcre = re.compile(r'/\*([0-9a-f]{5,8})\*/')
for fn in sys.argv[1:]:
    s = open(fn).read(); out = []; pos = 0; n = 0
    for m in re.finditer(r'\bvu_vgrp\(', s):
        pm = None
        for pm in pcre.finditer(s, max(0, m.start() - 300), m.start()): pass
        out.append(s[pos:m.start()]); out.append('M1VG(0x%su, ' % (pm.group(1) if pm else '0')); pos = m.end(); n += 1
    out.append(s[pos:]); open(fn, 'w').write(''.join(out)); print(fn, n)
