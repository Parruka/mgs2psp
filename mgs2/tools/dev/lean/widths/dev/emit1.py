#!/usr/bin/env python3
"""emit1.py <widths json> <unit> <function> [lines]: regcache's output for one function under the json's widths (rcdrv), numbered"""
import sys, os, json
sys.path.insert(0, '/home/claude/lean/w2/tools')
import rcdrv as D
J = json.load(open(sys.argv[1])); u = sys.argv[2]; f = sys.argv[3]
U = D.load(u); D.ctx(U)
wr = set(J['wr'].get(f, []))
out = D.emit(U, f, wr)
lines = out.split('\n')
sel = None
if len(sys.argv) > 4:
    sel = set()
    for part in sys.argv[4].split(','):
        a, _, b = part.partition('-'); a = int(a); b = int(b) if b else a
        sel |= set(range(a, b + 1))
print('== %s wide %s' % (f, sorted(wr)))
for i, l in enumerate(lines):
    if sel is None or i in sel: print('%4d %s' % (i, l[:220]))
