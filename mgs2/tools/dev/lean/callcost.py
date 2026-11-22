#!/usr/bin/env python3
"""callcost.py <gi counts> <steps> <header A> <header B> [<header A2> <header B2> ...]: the register traffic of the generated code's calls,
returns and polls, header A against header B, per step, weighted by how often each line runs (recomp/lean_plan.md step 1).

<gi counts>: an x86 run's guest instruction counts (tools/dev/gi_count.py, the play probe: lines "<pc> <count> <draw-pipeline count>";
GI_(pc, k) adds k each time a line runs, k = 2 on a line with a control transfer plus one per inlined instruction marker), over <steps>
steps.  Lines are matched by their /*pc*/ marker, so A and B must come from the same translation (only the register traffic differs).
A line's runs weigh every store and reload on it (a store at a conditional return counts as if the return were taken every time).
PSP instructions: an int32_t local stored to the 64-bit R[] 3 (sra + 2 sw), an int64_t local 2; a reload 1 / 2; F[] 1.  Stores inside
IRQ_POLLF(...) run only when the poll is due (static count only)."""
import re, sys
from collections import defaultdict
MARK = re.compile(r'^/\*([0-9a-f]{6})\*/', re.M)
INNER = re.compile(r'/\*[0-9a-f]{6}\*/')
XFER = re.compile(r'\bgoto\b|\breturn\b|\bJALR\(|\bJR\(|\b(?:S_\w+_)?P[0-9a-f]{5,6}\(\)')
FN = re.compile(r'^static void ([A-Za-z_]\w*)\(void\)\n\{\n(.*?)\n\}$', re.S | re.M)
ST = re.compile(r'\bR\[(\d+)\] = r(\d+)\b')
LD = re.compile(r'\br(\d+) = (?:\(int32_t\))?R\[(\d+)\]')
FST = re.compile(r'\bF\[(\d+)\] = f(\d+)\b')
FLD = re.compile(r'\bf(\d+) = F\[(\d+)\]')
RET = re.compile(r'\{ ([^{}]*?); (?:\{ ([^{}]*?); )?return; \}(?: \})?')

def spans(s, name):
    out = []; i = 0
    while True:
        j = s.find(name + '(', i)
        if j < 0: return out
        d = 0; k = j + len(name)
        while True:
            c = s[k]; d += (c == '(') - (c == ')'); k += 1
            if d == 0: break
        out.append((j, k)); i = k

def scan(path):
    """per pc: (runs weight k, dict of counts by kind), and the static totals"""
    src = open(path).read(); res = {}; stat = defaultdict(int)
    for m in FN.finditer(src):
        body = m.group(2)
        d = re.search(r'^  (?:int64_t [^;]*; )?(?:int64_t|int32_t) [^;]*;', body, re.M)
        w64 = set()
        if d:
            for part in d.group(0).split(';'):
                part = part.strip()
                if part.startswith('int64_t'): w64 |= {int(x) for x in re.findall(r'\br(\d+)\b', part)}
        for ln in body.split('\n'):
            mm = MARK.match(ln)
            if not mm: continue
            pc = int(mm.group(1), 16); k = (2 if XFER.search(ln) else 1) + len(INNER.findall(ln, mm.end()))
            c = defaultdict(int)
            cut = spans(ln, 'IRQ_POLLF')
            for a, b in cut:
                seg = ln[a:b]
                c['poll st'] += len(ST.findall(seg)) + len(FST.findall(seg))
            rest = ''.join(ln[p:q] for p, q in zip([0] + [b for a, b in cut], [a for a, b in cut] + [len(ln)]))
            for r in RET.finditer(rest):
                seg = r.group(0)
                for x in ST.finditer(seg): c['ret st'] += 1; c['ret instr'] += 2 if int(x.group(2)) in w64 else 3
                for x in FST.finditer(seg): c['ret F st'] += 1; c['ret instr'] += 1
            rest = RET.sub('', rest)
            for x in ST.finditer(rest): c['call st'] += 1; c['call instr'] += 2 if int(x.group(2)) in w64 else 3
            for x in LD.finditer(rest):
                if x.group(1) == x.group(2): c['call ld'] += 1; c['call instr'] += 2 if int(x.group(1)) in w64 else 1
            for x in FST.finditer(rest): c['call F st'] += 1; c['call instr'] += 1
            for x in FLD.finditer(rest):
                if x.group(1) == x.group(2): c['call F ld'] += 1; c['call instr'] += 1
            for kk, v in c.items(): stat[kk] += v
            res[pc] = (k, c)
    return res, stat

gi = {}
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 3: gi[int(p[0], 16)] = (int(p[1]), int(p[2]))
steps = float(sys.argv[2])
pairs = sys.argv[3:]
tot = [[defaultdict(float), defaultdict(float)] for _ in range(2)]; stot = [defaultdict(int), defaultdict(int)]   # [header][logic, draw]
for i in range(0, len(pairs), 2):
    for j, path in enumerate(pairs[i:i + 2]):
        res, stat = scan(path)
        for kk, v in stat.items(): stot[j][kk] += v
        for pc, (k, c) in res.items():
            g = gi.get(pc, (0, 0))
            for kk, v in c.items():
                for x in (0, 1): tot[j][x][kk] += v * g[x] / k / steps
keys = ['call st', 'call ld', 'call F st', 'call F ld', 'call instr', 'ret st', 'ret F st', 'ret instr', 'poll st']
for x, nm in ((0, 'game logic (outside the draw pipeline)'), (1, 'draw pipeline (P11da90 and its callees)')):
    print('== %s, per step' % nm)
    print('%-12s %12s %12s %9s   %10s %10s' % ('', 'A', 'B', 'B-A', 'static A', 'static B'))
    for kk in keys:
        a, b = tot[0][x][kk], tot[1][x][kk]
        print('%-12s %12.0f %12.0f %+8.1f%%   %10d %10d' % (kk, a, b, 100 * (b - a) / a if a else 0, stot[0][kk], stot[1][kk]))
    ia = tot[0][x]['call instr'] + tot[0][x]['ret instr']; ib = tot[1][x]['call instr'] + tot[1][x]['ret instr']
    print('PSP instructions of the call and return traffic: A %.0f, B %.0f (%+.0f, %+.3f ms at 1.4 cycles per instruction, 333 MHz)' % (ia, ib, ib - ia, (ib - ia) * 1.4 / 333000))
