"""per.py <file>:<steps>[:<label>] ...: guest instructions per step for each gi file: outside the draw pipeline (ELF, stage) and inside it."""
import sys
for a in sys.argv[1:]:
    p = a.split(':'); fn, n = p[0], int(p[1]); lab = p[2] if len(p) > 2 else fn
    e = s = d = 0
    for ln in open(fn):
        pc, c, g = ln.split(); pc = int(pc, 16); c = int(c); g = int(g)
        if pc >= 0x1f1c00: s += c
        else: e += c
        d += g
    print('%-9s steps %5d: outside the draw pipeline %8.0f (ELF %7.0f, stage %7.0f), draw pipeline %7.0f, all %8.0f' % (lab, n, (e + s) / n, e / n, s / n, d / n, (e + s + d) / n))
