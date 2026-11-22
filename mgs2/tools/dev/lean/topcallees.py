#!/usr/bin/env python3
"""topcallees.py <gi counts> <steps> <header>...: the functions entered most often per step (runs of their first line / k), with
their size in guest instructions (marked lines) and the guest instructions they run per entry; logic and draw pipeline together."""
import re, sys
MARK = re.compile(r'^/\*([0-9a-f]{6})\*/', re.M)
INNER = re.compile(r'/\*[0-9a-f]{6}\*/')
XFER = re.compile(r'\bgoto\b|\breturn\b|\bJALR\(|\bJR\(|\b(?:S_\w+_)?P[0-9a-f]{5,6}\(\)')
FN = re.compile(r'^static void ((?:S_\w+_)?P[0-9a-f]{5,6})\(void\)\n\{\n(.*?)\n\}$', re.S | re.M)
gi = {}
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 3: gi[int(p[0], 16)] = int(p[1]) + int(p[2])
steps = float(sys.argv[2]); rows = []
for h in sys.argv[3:]:
    for m in FN.finditer(open(h).read()):
        lines = [l for l in m.group(2).split('\n') if MARK.match(l)]
        if not lines: continue
        first = lines[0]; pc0 = int(MARK.match(first).group(1), 16)
        k0 = (2 if XFER.search(first) else 1) + len(INNER.findall(first, MARK.match(first).end()))
        ent = gi.get(pc0, 0) / k0
        if ent == 0: continue
        gins = sum(gi.get(int(MARK.match(l).group(1), 16), 0) for l in lines)
        rows.append((ent / steps, m.group(1), len(lines), gins / max(ent, 1)))
rows.sort(reverse=True)
tot = sum(r[0] for r in rows)
print('%d functions entered, %.0f entries per step' % (len(rows), tot))
for e, n, sz, per in rows[:25]: print('%8.0f per step  %-16s %5d lines, %6.1f guest instr per entry' % (e, n, sz, per))
