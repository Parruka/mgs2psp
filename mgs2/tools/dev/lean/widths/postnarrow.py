#!/usr/bin/env python3
"""postnarrow.py <survey header> <out>: regcache.run's passes up to the width decision (RC_SPECIAL rename, inline_pass, the scratchpad
base, narrow.py with the sound ABI sets), written out (the input regcache.process sees, before its own interprocedural sets)"""
import os, re, sys
sys.path.insert(0, '/mnt/project-files/mgs2/recomp')
os.environ.setdefault('RC_SOUND', '1'); os.environ.setdefault('RC_SPECIAL', '1')
import regcache as rc, narrow
src = open(sys.argv[1]).read()
for pc in (0x175d98, 0x175dd4):
    old = '/*%x*/ { int64_t t_ = R[6]; R[31] = 0x%x; R[4] = R[29] + R[0]; JALR((uint32_t)t_); }' % (pc, pc + 8)
    if src.count(old) == 1: src = src.replace(old, old.replace('JALR(', 'JSIF('))
src = rc.inline_pass(src)
src = src.replace('/*19e21c*/ R[2] = 1879048192;', '/*19e21c*/ R[2] = 0;   /* lui 0x7000: the scratchpad, at guest 0 (regcache.py) */')
bodies = {m.group(1): m.group(2) for m in rc.FN.finditer(src)}
ipo = narrow.ipo_abi(bodies, lambda b, look: rc.ipo_function_sound(b, look)[:4]); narrow.MUST.clear(); narrow.MUST.update(rc.must_abi_sound(bodies))
tot = [0, 0]
def nf(m):
    b, k = narrow.narrow(m.group(2), ipo); tot[0] += k; tot[1] += k > 0
    return 'static void %s(void)\n{\n%s\n}' % (m.group(1), b)
w0 = sum(1 for m in rc.FN.finditer(src) if rc.is_wide(m.group(2)))
src = rc.FN.sub(nf, src)
print('narrow: %d dsll32 rewritten in %d functions; 64-bit functions %d -> %d' % (tot[0], tot[1], w0, sum(1 for m in rc.FN.finditer(src) if rc.is_wide(m.group(2)))))
open(sys.argv[2], 'w').write(src)
