#!/usr/bin/env python3
"""prune_cov.py <gen_*.h> <cov.txt> <unit> <out>: the timing harness's reduced unit.  Every translated function that the coverage run
(gen_pre.h GEN_COVER, boot.c rt_cov: lines "<unit> <hex address>") did not enter keeps its entry but its body becomes rt_missing(address)
(the runtime stops there).  Exact only for runs that enter the same functions as the coverage run (the scripted boot-to-deck run)."""
import re, sys
src, cov, unit, out = sys.argv[1:5]
seen = {int(a, 16) for u, a in (l.split() for l in open(cov)) if u == unit or u.endswith('/' + unit)}
lines = open(src).read().split('\n'); o = []; i = 0; kept = cut = 0
DEF = re.compile(r'^static void \w+\(void\)$')
while i < len(lines):
    l = lines[i]
    if DEF.match(l) and i + 2 < len(lines) and lines[i + 1] == '{' and lines[i + 2].startswith('FN_ENTER(0x'):
        a = int(re.match(r'FN_ENTER\(0x([0-9a-f]+)\)', lines[i + 2]).group(1), 16)
        j = i + 1
        while lines[j] != '}': j += 1
        if a in seen: o.extend(lines[i:j + 1]); kept += 1
        else: o += [l, '{', lines[i + 2], '  rt_missing(0x%x);' % a, '}']; cut += 1
        i = j + 1; continue
    o.append(l); i += 1
open(out, 'w').write('\n'.join(o))
print('%s: %d functions kept, %d cut' % (unit, kept, cut))
