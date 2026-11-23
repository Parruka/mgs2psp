#!/usr/bin/env python3
"""top3 prototype P3 (2026-11-23): blank the box cull's 8 per-corner `vcmp.q ES` checks in a COPY of gen_vu.h (vupv_96936c0f_0).
Usage: python3 -I p3_boxcull_checks.py SRC_gen_vu.h DST_gen_vu.h.  Lines are blanked, not deleted, so later line numbers stay.
Line numbers are those of gl39's gen/vp22/gen_vu.h (md5 feb948ce8beb6f19c1b764b20ab8da0e); the asserts stop on any other file.
Why it is exact: between each corner's check and the out_ check of the same VFPU register, the register is only multiplied
(vmul with z/w masked), so a non-finite lane stays non-finite and the out_ check still takes vfail_, which has written nothing.
For a generator rule: a check of register r is redundant when every write of r before the next check of r is a multiply (or masked)."""
import sys
src, dst = sys.argv[1:3]
L = open(src).read().split('\n')
for a in (104069, 104109, 104146, 104183, 104283, 104320, 104357, 104396):
    assert 'vcmp.q ES' in L[a-1] and 'VPC(vck_' in L[a] and 'goto vfail_' in L[a+1], a
    for k in range(3): L[a-1+k] = ''
open(dst, 'w').write('\n'.join(L))
