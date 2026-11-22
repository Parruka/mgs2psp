#!/usr/bin/env python3
# g18cmp.py <emu log> <gl18 hashref> <gl17 emu log> <x86 log (gz ok)>: gl18's emulator run (task #179) against gl18's x86 reference
# (every hash line's ee and vfb; split lines), gl17's emulator run (vuh lines, hash lines before vb 23,300 where the skips change
# nothing yet), and gl18's x86 run (the frame digest lines of the emulator's window, by vblank); list words and memory lines.
import re, sys, gzip
def rd(p):
    op = gzip.open if p.endswith('.gz') else open
    H = {}; V = []; P = {}; X = []; SP = []
    for l in op(p, 'rt', errors='replace'):
        if l.startswith('hash: vb '):
            m = re.match(r'hash: vb (\d+) ee (\w+)', l); f = re.search(r' vfb (\d+)', l)
            if m: H[int(m.group(1))] = (m.group(2), f.group(1) if f else None)
        elif l.startswith('vuh: vb '): V.append(l.strip())
        elif l.startswith('split:'): SP.append(l.strip()[:160])
        else:
            m = re.match(r'picture: vb (\d+) buffer (\d+) list (\w+) frame (\w+): (.*)', l)
            if m: P[int(m.group(1))] = (m.group(2), m.group(3), m.group(4), m.group(5).strip())
            elif re.search(r'frames made|headroom|break|lowest|^translator: [0-9]', l): X.append(l.rstrip()[:240])
    return H, V, P, X, SP
e = rd(sys.argv[1]); g = rd(sys.argv[3]); x = rd(sys.argv[4])
ref = {}
for l in open(sys.argv[2]):
    p = l.split()
    if len(p) >= 2: ref[int(p[0])] = (p[1], p[2] if len(p) > 2 else None)
n = sum(1 for v in e[0] if v in ref); d = [v for v in sorted(e[0]) if v in ref and (ref[v][0] != e[0][v][0] or (ref[v][1] and e[0][v][1] and ref[v][1] != e[0][v][1]))]
print('hash lines %d; against gl18\'s reference: %d compared, %d differ%s' % (len(e[0]), n, len(d), (' (first %d)' % d[0]) if d else ''))
print('split lines: %d: %s' % (len(e[4]), ' | '.join(e[4][:3])))
gb = [v for v in sorted(e[0]) if v < 23300 and v in g[0]]; gd = [v for v in gb if g[0][v][0] != e[0][v][0]]
print('against gl17\'s emulator run before vb 23,300: %d compared, %d differ' % (len(gb), len(gd)))
print('vuh lines %d, gl17 %d, equal: %s' % (len(e[1]), len(g[1]), e[1] == g[1]))
ev = sorted(e[2]); print('picture lines %d (vb %s..%s)' % (len(ev), ev[0] if ev else '-', ev[-1] if ev else '-'))
same = sum(1 for v in ev if v in x[2] and x[2][v][1:3] == e[2][v][1:3]); diff = [v for v in ev if v not in x[2] or x[2][v][1:3] != e[2][v][1:3]]
rest = sum(1 for v in ev if v in x[2] and x[2][v] == e[2][v]); print('  list and frame digests vs x86; whole lines equal: %d' % rest)
print('  digests equal to the x86 run: %d of %d; differing: %s' % (same, len(ev), diff[:10]))
for v in diff[:3]: print('   vb %d emu %s / x86 %s' % (v, e[2][v], x[2].get(v)))
gs = sum(1 for v in ev if v in g[2] and g[2][v][1:3] == e[2][v][1:3]); print('  equal to gl17\'s emulator run: %d of %d' % (gs, len(ev)))
fr = set(p[2] for p in e[2].values()); print('  distinct frame digests in the window: %d' % len(fr))
for l in e[3][-8:]: print('  ' + l)
