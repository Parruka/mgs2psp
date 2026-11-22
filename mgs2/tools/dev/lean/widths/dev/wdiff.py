#!/usr/bin/env python3
"""wdiff.py <widths json A> <widths json B> <function> ...: per function, the 64-bit registers of A and of B, those B narrows and
those B widens"""
import sys, json
A = json.load(open(sys.argv[1]))['wr']; B = json.load(open(sys.argv[2]))['wr']
for f in sys.argv[3:]:
    a = set(A.get(f, [])); b = set(B.get(f, []))
    print('%-22s A %2d  B %2d  narrowed %s  widened %s' % (f, len(a), len(b), sorted(a - b), sorted(b - a)))
