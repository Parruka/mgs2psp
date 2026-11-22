#!/usr/bin/env python3
"""vgrp_fcl.py <gen header> [...]: in the fallback argument (F, the sixth) of every vu_vgrp(...) of a generated header, the result clamps
VU_MCL1..4 become VU_FCL1..4 (in place).  Why: F runs with the VFPU's rounding (round to nearest, vu0.h VU_FB_ON), where an overflow gives
+-Inf, so the PSP's VU_MCL_RZ identity clamp (exact only under round toward zero) left +-Inf where the VU gives +-Fmax (v16 emulator,
vb 15739).  VU_FCL clamps on every build (vu0.h).  Idempotent; prints the number of groups and clamps changed."""
import sys, re
CL = re.compile(r'\bVU_MCL([1-4])\(')

def args_end(s, i):
    """s[i] is just after 'vu_vgrp('; returns (list of (start, end) of the arguments, index after the closing ')')"""
    depth, start, out, n = 0, i, [], len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            q = c; i += 1
            while s[i] != q:
                i += 2 if s[i] == '\\' else 1
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                out.append((start, i)); return out, i + 1
            depth -= 1
        elif c == ',' and depth == 0:
            out.append((start, i)); start = i + 1
        i += 1
    raise ValueError('unbalanced vu_vgrp at %d' % i)

for fn in sys.argv[1:]:
    s = open(fn).read()
    parts, pos, groups, clamps = [], 0, 0, 0
    for m in re.finditer(r'\bvu_vgrp\(', s):
        if m.start() < pos: continue
        a, end = args_end(s, m.end())
        if len(a) != 6: raise ValueError('%s: vu_vgrp with %d arguments at %d' % (fn, len(a), m.start()))
        f0, f1 = a[5]
        t, k = CL.subn(r'VU_FCL\1(', s[f0:f1])
        parts.append(s[pos:f0]); parts.append(t); pos = f1
        groups += 1; clamps += k
    parts.append(s[pos:])
    t = ''.join(parts)
    if t != s: open(fn, 'w').write(t)
    print('%s: %d groups, %d fallback clamps now VU_FCL' % (fn, groups, clamps))
