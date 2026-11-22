#!/usr/bin/env python3
"""sqmiss.py <survey header> ...: functions regcache's WIDE list makes 32-bit although they hold an sq (WR64 of an aligned quadword address)
to a base other than $sp; with the register stored and how the function gets it"""
import re, sys
sys.path.insert(0, '/mnt/project-files/mgs2/recomp'); import regcache as rc
SQ = re.compile(r'\bWR64\(\(\(uint32_t\)\(R\[(\d+)\] \+ (-?\d+)\)\) & ~15u, R\[(\d+)\]\)')
FN = re.compile(r'^static void (\w+)\(void\)$')
tot = 0; nar = 0
for fn in sys.argv[1:]:
    cur = None; body = []
    for l in open(fn):
        l = l.rstrip('\n'); m = FN.match(l)
        if m: cur = m.group(1); body = []; continue
        if cur is None: continue
        if l == '}':
            b = '\n'.join(body); sq = [x for x in SQ.findall(b) if x[0] != '29']
            if sq:
                tot += 1
                if not rc.is_wide(b): nar += 1; print(fn.split('/')[-3], cur, 'sq', ' '.join('R[%s]->%s(R[%s])' % (x[2], x[1], x[0]) for x in sq[:6]))
            cur = None; continue
        body.append(l)
print('functions with a non-stack sq: %d; made 32-bit by the WIDE list: %d' % (tot, nar))
