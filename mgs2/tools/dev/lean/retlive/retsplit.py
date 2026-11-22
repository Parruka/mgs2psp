#!/usr/bin/env python3
"""retsplit.py <gi counts> <steps> <open dir> <header> [header ...]: the dynamic return stores of the headers (callcost.py's weighting,
logic + draw), split by open / closed function and by register (dev, lean 1c)."""
import re, sys, os, importlib.util
from collections import defaultdict
spec = importlib.util.spec_from_file_location('cc', '/mnt/project-files/mgs2/tools/dev/lean/callcost.py')
MARK = re.compile(r'^/\*([0-9a-f]{6})\*/'); INNER = re.compile(r'/\*[0-9a-f]{6}\*/')
XFER = re.compile(r'\bgoto\b|\breturn\b|\bJALR\(|\bJR\(|\b(?:S_\w+_)?P[0-9a-f]{5,6}\(\)')
FN = re.compile(r'^static void ([A-Za-z_]\w*)\(void\)\n\{\n(.*?)\n\}$', re.S | re.M)
ST = re.compile(r'\bR\[(\d+)\] = r(\d+)\b'); FST = re.compile(r'\bF\[(\d+)\] = f(\d+)\b')
RET = re.compile(r'\{ ([^{}]*?); (?:\{ ([^{}]*?); )?return; \}(?: \})?')
gi = {}
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 3: gi[int(p[0], 16)] = int(p[1]) + int(p[2])
steps = float(sys.argv[2]); od = sys.argv[3]
tot = defaultdict(float); byreg = defaultdict(float); byfn = defaultdict(float)
for path in sys.argv[4:]:
    u = 'elf' if path.endswith('gen_elf.h') else re.search(r'gen_s_(\w+)\.h$', path).group(1)
    opn = set(open(os.path.join(od, 'open_%s.txt' % u)).read().split())
    src = open(path).read()
    for m in FN.finditer(src):
        name = m.group(1); cls = 'open' if name in opn else 'closed'
        for ln in m.group(2).split('\n'):
            mm = MARK.match(ln)
            if not mm: continue
            pc = int(mm.group(1), 16); w = gi.get(pc, 0)
            if not w: continue
            k = (2 if XFER.search(ln) else 1) + len(INNER.findall(ln, mm.end()))
            for r in RET.finditer(ln):
                seg = r.group(0)
                for x in ST.finditer(seg): v = w / k / steps; tot[(cls, 'R')] += v; byreg[(cls, 'r%s' % x.group(1))] += v; byfn[(name, 'R')] += v
                for x in FST.finditer(seg): v = w / k / steps; tot[(cls, 'F')] += v; byreg[(cls, 'f%s' % x.group(1))] += v; byfn[(name, 'F')] += v
for k_, v in sorted(tot.items()): print('%-8s %s %10.0f' % (k_[0], k_[1], v))
for cls in ('open', 'closed'):
    print(cls, ' '.join('%s:%.0f' % (r, v) for (c, r), v in sorted(byreg.items(), key=lambda x: -x[1]) if c == cls and v >= 300))
print('top functions:', ' '.join('%s/%s:%.0f' % (n, t, v) for (n, t), v in sorted(byfn.items(), key=lambda x: -x[1])[:25]))
