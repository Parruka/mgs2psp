# g17cmp.py <emu log> <gl16 emu log> <x86 log>: gl17's emulator run against gl16's (state hashes, vuh) and against gl17's x86 run (the
# frame digest lines of the emulator's window, by vblank), plus its list words and memory lines
import re, sys
def rd(p):
    H = []; V = []; P = {}; X = []
    for l in open(p, errors='replace'):
        if l.startswith('hash: vb '): H.append(' '.join(l.split()[:7]))
        elif l.startswith('vuh: vb '): V.append(l.strip())
        else:
            m = re.match(r'picture: vb (\d+) buffer (\d+) list (\w+) frame (\w+): (.*)', l)
            if m: P[int(m.group(1))] = (m.group(2), m.group(3), m.group(4), m.group(5).strip())
            elif re.search(r'list words|headroom|break|end: |lowest', l): X.append(l.rstrip()[:200])
    return H, V, P, X
e = rd(sys.argv[1]); g = rd(sys.argv[2]); x = rd(sys.argv[3])
print('hash lines %d, gl16 %d, equal: %s' % (len(e[0]), len(g[0]), e[0] == g[0]))
if e[0] != g[0]:
    d = [i for i in range(min(len(e[0]), len(g[0]))) if e[0][i] != g[0][i]]; print('  first differing:', e[0][d[0]] if d else '-', '/', g[0][d[0]] if d else '-')
print('vuh lines %d, gl16 %d, equal: %s' % (len(e[1]), len(g[1]), e[1] == g[1]))
ev = sorted(e[2]); print('picture lines %d (vb %s..%s)' % (len(ev), ev[0] if ev else '-', ev[-1] if ev else '-'))
same = sum(1 for v in ev if v in x[2] and x[2][v] == e[2][v]); diff = [v for v in ev if v not in x[2] or x[2][v] != e[2][v]]
print('  equal to the x86 run, line for line: %d of %d; differing: %s' % (same, len(ev), diff[:10]))
for v in diff[:3]: print('   vb %d emu %s / x86 %s' % (v, e[2][v], x[2].get(v)))
fr = set(p[2] for p in e[2].values()); print('  distinct frame digests in the window: %d' % len(fr))
for l in e[3][-8:]: print('  ' + l)
