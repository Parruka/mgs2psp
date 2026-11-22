#!/usr/bin/env python3
"""ldst.py <survey header> ...: 32-bit functions (regcache's WIDE list) where a register loaded with ld / lq from the stack is later stored
with sd / sq (any base) -- the pattern whose upper word v27's 32-bit locals drop"""
import re, sys
sys.path.insert(0, '/mnt/project-files/mgs2/recomp'); import regcache as rc
LQ = re.compile(r'qa_ = \(\(uint32_t\)\(R\[29\] \+ -?\d+\)\) & ~15u; if \(\d+\) \{ R\[(\d+)\] = \(int64_t\)RD64\(qa_\)')
LD = re.compile(r'\bR\[(\d+)\] = \(int64_t\)RD64\(\(uint32_t\)\(R\[29\] \+ -?\d+\)\)')
ST = re.compile(r'\bWR64\(\(*uint32_t\)\(R\[(\d+)\] \+ (-?\d+)\)\)?(?: & ~15u)?, R\[(\d+)\]\)')
FN = re.compile(r'^static void (\w+)\(void\)$')
n = 0; kinds = {}
for fn in sys.argv[1:]:
    cur = None; body = []
    for l in open(fn):
        l = l.rstrip('\n'); m = FN.match(l)
        if m: cur = m.group(1); body = []; continue
        if cur is None: continue
        if l != '}': body.append(l); continue
        b = '\n'.join(body); cur0 = cur; cur = None
        if rc.is_wide(b): continue
        loaded = {}; hits = []
        for ln in body:
            for m2 in LQ.finditer(ln): loaded[m2.group(1)] = 'lq'
            for m2 in LD.finditer(ln): loaded[m2.group(1)] = 'ld'
            for m2 in ST.finditer(ln):
                r = m2.group(3)
                if r in loaded: hits.append('%s r%s -> %s(R[%s])' % (loaded[r], r, m2.group(2), m2.group(1)))
            for m2 in re.finditer(r'\bR\[(\d+)\] = ', ln):
                if m2.group(1) in loaded and not (LQ.search(ln) or LD.search(ln)): loaded.pop(m2.group(1))
        if hits:
            n += 1; print(fn.split('/')[-3], cur0, '; '.join(hits[:5]))
print('32-bit functions storing a register loaded with ld / lq from the stack:', n)
