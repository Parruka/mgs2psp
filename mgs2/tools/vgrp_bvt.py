#!/usr/bin/env python3
"""vgrp_bvt.py <in.h> <out.h>: rewrites the check of every checked VFPU group (vu_vgrp, recomp/vuemit.py vfpu_group) in a generated
header from `vcmp ES; nop; mfvc %[cc], $131` to a branch on the condition code: `bvtl 4, 1f; ori %[cc], %[cc], 0x10` after %[cc] is
cleared, the asm in noreorder mode.  Why: v12 on the PSP fell back less often than the emulator and the x86 model and left the reference
at vb 16000 (prof/hw_v12); inferred cause: mfvc reads the condition code before an in-flight vcmp has written it (PPSSPP has no such
delay).  hwprobe.c probe 7 measures it.  vuemit.py emits the same form now; this applies it to headers generated before."""
import sys, re
src = open(sys.argv[1]).read()
CHK1 = r'\n\tnop\n\tmfvc %[cc], $131'
CHKN = r'\n\tnop\n\tmfvc %[t], $131\n\tor %[cc], %[cc], %[t]'
NEW = r'\n\tbvtl 4, 1f\n\tori %[cc], %[cc], 0x10\n\t1:'
n = {'groups': 0, 'checks': 0}
def fix(m):
    body = m.group(1)
    k = body.count(CHK1) + body.count(CHKN)
    assert k >= 1 and 'mfvc' not in body.replace(CHK1, '').replace(CHKN, ''), body[:200]
    n['groups'] += 1; n['checks'] += k
    body = body.replace(CHKN, NEW).replace(CHK1, NEW)
    return 'vu_vgrp((".set push\\n\\t.set noreorder\\n\\tmove %[cc], $0\\n\\t' + body + '\\n\\t.set pop" : [cc] "=&r"(vcc_) :'
out = re.sub(r'vu_vgrp\(\("((?:[^"\\]|\\.)*)" : \[cc\] "=&r"\(vcc_\), \[t\] "=&r"\(vct_\) :', fix, src)
assert 'mfvc %[cc]' not in out and 'vct_' not in out
open(sys.argv[2], 'w').write(out)
print('%s: %d groups, %d checks' % (sys.argv[1], n['groups'], n['checks']))
