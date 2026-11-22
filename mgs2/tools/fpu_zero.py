#!/usr/bin/env python3
"""fpu_zero.py <gen header> [...]: routes the FPU adds and subtracts of generated code (in place) through guest.h FADD / FSUB:
`x = a + b;` / `x = a - b;` with x, a, b float registers (regcache.py locals fN, F[N], ACC), and the add / subtract of the
multiply-accumulate forms (`ACC + a * b`, `ACC - a * b`).  Why: the PSP FPU with FCR31 FS = 1 returns the other operand unchanged when
exactly one operand is zero, a denormal included (TT lessons A9); the x86 reference flushed it, and v15 on the PSP left the reference at
vb 15794 over such an add (0 + 0x0031685c, prof/hw_v15).  On the PSP FADD / FSUB are the plain operators (the same code); on x86 they
model the rule.  Run after the other passes (recomp/README.md).  Idempotent; prints the number of sites rewritten."""
import sys, re
V = r'(?:f\d+|F\[\d+\]|ACC)'
BIN = re.compile(r'\b(%s) = (%s) ([-+]) (%s);' % (V, V, V))
MAC = re.compile(r'\b(%s) = ACC ([-+]) (%s \* %s);' % (V, V, V))
for fn in sys.argv[1:]:
    s = open(fn).read()
    t, a = BIN.subn(lambda m: '%s = %s(%s, %s);' % (m.group(1), 'FADD' if m.group(3) == '+' else 'FSUB', m.group(2), m.group(4)), s)
    t, b = MAC.subn(lambda m: '%s = %s(ACC, %s);' % (m.group(1), 'FADD' if m.group(2) == '+' else 'FSUB', m.group(3)), t)
    if t != s: open(fn, 'w').write(t)
    print('%s: %d add / subtract, %d multiply-accumulate sites rewritten' % (fn, a, b))
