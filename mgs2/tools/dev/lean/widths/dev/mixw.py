#!/usr/bin/env python3
"""mixw.py <proof json> <out json> <names file | -> [lo hi] (dev): widths = the proof's for the functions in the names file (or, with
lo hi, the lo..hi-1 entries of the sorted list of functions whose proof widths differ from v27's), v27's for the rest"""
import sys, json
J = json.load(open(sys.argv[1]))['wr']; V = json.load(open('/home/claude/lean/w3/wpw_v27x.json'))['wr']
diff = sorted(f for f in J if set(J[f]) != set(V.get(f, [])))
if sys.argv[3] != '-': sel = set(open(sys.argv[3]).read().split())
else: sel = set(diff[int(sys.argv[4]):int(sys.argv[5])])
out = {f: (J[f] if f in sel else V.get(f, [])) for f in J}
json.dump({'wr': out}, open(sys.argv[2], 'w'))
print('%d functions differ from v27; %d take the proof widths' % (len(diff), len(sel & set(diff))))
