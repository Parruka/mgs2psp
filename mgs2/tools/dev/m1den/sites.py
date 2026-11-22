#!/usr/bin/env python3
"""sites.py <run log> [top]: the denormal sites of an m1den run, each with the guest function holding its pc and what the loaded register
is used for next (by the generated text: int = converted by cvt.s.w, store, mfc1, dead = overwritten unread, float = a float operand)."""
import re, sys, glob, bisect, collections
import os
W = os.environ.get('M1W', '/home/claude/m1den/W')   # v30: M1W = the rewritten header folder
log, top = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 12
sites = {}
for f in glob.glob(W + '/*.sites'):
    for l in open(f):
        p = l.rstrip('\n').split(' ', 4)
        if len(p) == 5: sites[int(p[0])] = (p[1], p[2], p[3])
hdr, starts = {}, {}
def unit_text(unit):
    fn = W + ('/gen_elf.h' if unit == 'elf' else '/gen_s_%s.h' % unit)
    if fn not in hdr:
        s = hdr[fn] = open(fn).read()
        st = sorted(set(int(m.group(1), 16) for m in re.finditer(r'\nstatic void (?:S_[a-z0-9_]+?_)?P([0-9a-f]+)\(void\)\n\{', s)))
        starts[fn] = st
    return fn, hdr[fn]
def guest_fn(unit, pc):
    fn, s = unit_text(unit); st = starts[fn]; i = bisect.bisect_right(st, pc) - 1
    return ('P%x' % st[i]) if i >= 0 else '?'
CALL = {'lwc1': 'M1RDF(%du, ', 'lwc1b': 'M1RDFB(%du, ', 'lwc1s': 'M1RDFS(%du, ', 'mtc1': 'M1I2F(%du, '}
def use_of(unit, idx, kind):
    fn, s = unit_text(unit); k = s.find(CALL[kind] % idx)
    if k < 0: return '?', ''
    m = re.search(r'(f\d+) = $', s[k - 12:k])
    if not m: return 'nodst', ''
    r = m.group(1)
    # the end of the statement: the ';' after the call's closing parenthesis
    d, j = 0, k
    while j < len(s):
        c = s[j]
        if c == '(': d += 1
        elif c == ')':
            d -= 1
            if d == 0: break
        j += 1
    rest = s[j + 1:j + 4000]
    u = re.search(r'\b%s\b' % r, rest)
    if not u: return 'none', ''
    a = rest.rfind(';', 0, u.start()); b = rest.find(';', u.end())
    stmt = rest[a + 1:b].strip()
    stmt = re.sub(r'^(/\*[0-9a-f]+\*/\s*|L[0-9a-f]+:;?\s*|\{\s*)+', '', stmt)
    if re.match(r'%s = \(float\)\(int32_t\)F2I\(%s\)' % (r, r), stmt): return 'int', stmt
    if re.match(r'%s = ' % r, stmt) and not re.search(r'\b%s\b' % r, stmt[len(r) + 3:]): return 'dead', stmt
    if re.match(r'WRFS?\(', stmt): return 'store', stmt
    if re.search(r'F2I\(%s\)' % r, stmt) and not re.search(r'\(float\)', stmt): return 'mfc1', stmt
    return 'float', stmt
recs = []
for l in open(log):
    m = re.match(r'm1: site (\d+) pc (\w+) (\w+) n (\d+) vb (-?\d+)-(-?\d+) first a (\w+) u (\w+) last a (\w+) u (\w+)', l)
    if not m: continue
    idx = int(m.group(1)); unit, pc, kind = sites.get(idx, ('?', '0', '?'))
    cls, stmt = use_of(unit, idx, kind) if unit != '?' else ('?', '')
    recs.append(dict(idx=idx, unit=unit, pc=int(m.group(2), 16), kind=m.group(3), n=int(m.group(4)), vb0=int(m.group(5)), vb1=int(m.group(6)),
                     a=m.group(7), u=m.group(8), a1=m.group(9), u1=m.group(10), cls=cls, stmt=stmt, fn=guest_fn(unit, int(m.group(2), 16))))
agg = collections.OrderedDict()
for r in recs:
    k = (r['cls'], r['kind']); a = agg.setdefault(k, [0, 0]); a[0] += 1; a[1] += r['n']
print('sites %d, uses %d' % (len(recs), sum(r['n'] for r in recs)))
for k in sorted(agg, key=lambda k: -agg[k][1]): print('  %-6s %-5s sites %5d  uses %11d' % (k[0], k[1], agg[k][0], agg[k][1]))
fl = sorted((r for r in recs if r['cls'] == 'float'), key=lambda r: -r['n'])
print('float-operand sites: %d, uses %d; functions %d' % (len(fl), sum(r['n'] for r in fl), len(set((r['unit'], r['fn']) for r in fl))))
fa = collections.Counter()
for r in fl: fa[(r['unit'], r['fn'])] += r['n']
print('  by function: ' + ', '.join('%s %s %d' % (u, f, n) for (u, f), n in fa.most_common(15)))
for r in fl[:top]:
    print('  %s %s pc %x %s: n %d vb %d-%d a %s u %s (last %s %s) | %s' % (r['unit'], r['fn'], r['pc'], r['kind'], r['n'], r['vb0'], r['vb1'], r['a'], r['u'],
          r['a1'], r['u1'], r['stmt'][:90]))
