#!/usr/bin/env python3
"""v44_apply.py <gen_s_w00a.h (the wpl one, with the G0R hooks)>: gl44 study hook, in place.

One hook in rain.c's act: at 0x31b47c (after an entry's own vector update and its two flip words, before its vertex and fill
loops) an in-place entry (G0R_D) may jump to the entry's tail at L31b6c0, with the loops' polls added (boot/native_w00a.c
g0r_lite, MGS2_OPT2 bit 22).  The anchor is the address comment only; the script stops if it is missing, doubled or already
hooked.  line_sph needs no header change (nat_ls_prim does it)."""
import sys
p = sys.argv[1]; s = open(p).read()
A = '/*31b47c*/ '; H = 'if (G0R_LITE()) goto L31b6c0; '
if A + H in s: sys.exit('v44_apply: already hooked')
if s.count(A) != 1 or 'L31b6c0:;' not in s or 'G0R_START();' not in s: sys.exit('v44_apply: anchors not found (is this the wpl header with G0R?)')
open(p, 'w').write(s.replace(A, A + H))
print('v44_apply: hooked 0x31b47c')
