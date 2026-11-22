#!/usr/bin/env python3
"""ld_any_kind.py <layout .ld> ...: rewrites, in place, each "obj(.text[.hot|.unlikely|.startup].NAME)" entry of a layout file written by an
older layout_order.py to "obj(.text.NAME .text.hot.NAME .text.unlikely.NAME .text.startup.NAME)", as layout_order.py now writes it, so a
function a rebuild's PGO profile moved between the hot and the plain section keeps its place.  Idempotent."""
import sys, re
pat = re.compile(r'^(\s*)([\w.]+\.o)\(\.text(?:\.hot|\.unlikely|\.startup)?\.([^ )]+)\)\s*$')
for fn in sys.argv[1:]:
    out, n = [], 0
    for l in open(fn).read().split('\n'):
        m = pat.match(l)
        if m: l = '%s%s(%s)' % (m.group(1), m.group(2), ' '.join(k + m.group(3) for k in ('.text.', '.text.hot.', '.text.unlikely.', '.text.startup.'))); n += 1
        out.append(l)
    open(fn, 'w').write('\n'.join(out)); print('%s: %d entries' % (fn, n))
