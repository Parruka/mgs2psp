#!/usr/bin/env python3
"""hashcmp.py <log.txt> <reference>: compares a run's "hash: vb N ee X iop Y vfb Z" lines with a reference ("vb ee iop vfb" lines,
prof/hashes_x86_*.txt; or "vb ee vfb [vuh]" hashref files).  Prints equal / compared for ee, iop and vfb, the first difference of each,
and any "split:" line of the log."""
import re, sys
log, ref = sys.argv[1], sys.argv[2]
R = {}
for ln in open(ref):
    p = ln.split()
    if len(p) >= 4 and len(p[2]) == 16: R[int(p[0])] = {'ee': p[1], 'iop': p[2], 'vfb': p[3]}
    elif len(p) >= 3: R[int(p[0])] = {'ee': p[1], 'vfb': p[2]}
n = {}; eq = {}; first = {}; splits = []
for ln in open(log, errors='replace'):
    if ln.startswith('split'): splits.append(ln.rstrip())
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)(?: vfb (\d+))?', ln)
    if not m: continue
    vb = int(m.group(1)); got = {'ee': m.group(2), 'iop': m.group(3), 'vfb': m.group(4)}
    if vb not in R: continue
    for k, v in R[vb].items():
        if got.get(k) is None: continue
        n[k] = n.get(k, 0) + 1
        if got[k] == v: eq[k] = eq.get(k, 0) + 1
        elif k not in first: first[k] = (vb, got[k], v)
print(' '.join('%s %d/%d' % (k, eq.get(k, 0), n[k]) for k in ('ee', 'iop', 'vfb') if k in n) or 'no hash line compared')
for k, (vb, g, w) in sorted(first.items()): print('first %s difference at vb %d: %s (reference %s)' % (k, vb, g, w))
for s in splits[:3]: print(s)
