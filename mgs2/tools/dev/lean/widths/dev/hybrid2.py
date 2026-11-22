#!/usr/bin/env python3
"""hybrid2.py <proof json> <narrow-in json> <out json>: widths = the proof's in the functions v27 makes 64-bit, the narrow-in run's (wpd.py
WPD_V32: what a 32-bit function receives taken as S, v27's NARROW assumption; W only from its own frame loads) in the others.  So a
32-bit function keeps the full values its own ld / lq give (the quadword copies v27 truncates) and nothing else.  'unproved' lists
the 32-bit functions where the full proof widens more than the narrow-in run (places that rest on the assumption)."""
import sys, json
J = json.load(open(sys.argv[1])); N = json.load(open(sys.argv[2])); V = json.load(open('/home/claude/lean/w3/wpw_v27x.json'))['wr']
out = {}; unproved = {}; nin = {}
for f, w in J['wr'].items():
    if V.get(f): out[f] = w
    else:
        out[f] = N['wr'].get(f, [])
        if out[f]: nin[f] = out[f]
        extra = sorted(set(w) - set(out[f]))
        if extra: unproved[f] = extra
json.dump({'wr': out, 'unproved': unproved, 'narrow_in': nin}, open(sys.argv[3], 'w'))
print('functions %d; 64-bit in v27 %d; 32-bit in v27 with narrow-in widths: %d (%d registers): %s; resting on the assumption: %d (%d registers)' %
      (len(out), sum(1 for f in out if V.get(f)), len(nin), sum(len(v) for v in nin.values()), ' '.join(sorted(nin)[:40]),
       len(unproved), sum(len(v) for v in unproved.values())))
