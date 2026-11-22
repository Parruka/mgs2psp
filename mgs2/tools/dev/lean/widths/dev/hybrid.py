#!/usr/bin/env python3
"""hybrid.py <proof json> <out json>: widths = the proof's in the functions v27 makes 64-bit (a 64-bit construct), v27's (all 32-bit) in
the others: the proof narrows the 64-bit functions; the 32-bit ones keep v27's route-checked assumption (GEN_NCHK).  Lists the 32-bit
functions where the proof would widen a register (the places the assumption is not proved) in <out json>'s 'unproved'."""
import sys, json
J = json.load(open(sys.argv[1])); V = json.load(open('/home/claude/lean/w3/wpw_v27x.json'))['wr']
out = {}; unproved = {}
for f, w in J['wr'].items():
    if V.get(f): out[f] = w
    else:
        out[f] = []
        if w: unproved[f] = w
json.dump({'wr': out, 'unproved': unproved}, open(sys.argv[2], 'w'))
print('functions %d; 64-bit in v27 %d; 32-bit in v27 where the proof widens: %d (%d registers)' %
      (len(out), sum(1 for f in out if V.get(f)), len(unproved), sum(len(v) for v in unproved.values())))
