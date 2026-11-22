# fndelta.py <dumpA> <dumpB> <build dir> <ovl elf> <steps> [n]: per-function change B - A (same build), sorted by |ms|, fitted weights
import bisect, subprocess, sys
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def syms(elfs):
    s = {}
    for e in elfs:
        for ln in subprocess.run([NM, '-n', '-S', e], capture_output=True, text=True).stdout.split('\n'):
            p = ln.split()
            if len(p) == 4 and p[2] in 'tTwW': s.setdefault(int(p[0], 16), (p[3], int(p[1], 16)))
    S = sorted(s.items()); return [a for a, _ in S], S
da, db, bd, ovl, st = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5]); top = int(sys.argv[6]) if len(sys.argv) > 6 else 30
A, S = syms([bd + '/MGS2T.elf', ovl])
def load(dump):
    f = {}
    for ln in open(dump):
        if ln[0] == '#': continue
        pc, c, im, dm = ln.split(); pc = int(pc, 16); i = bisect.bisect_right(A, pc) - 1
        n = S[i][1][0] if i >= 0 and pc < S[i][0] + max(S[i][1][1], 4) else '?'
        v = f.setdefault(n, [0, 0, 0]); v[0] += int(c); v[1] += int(im); v[2] += int(dm)
    return f
a, b = load(da), load(db)
ms = lambda v: (1.647 * v[0] + 68.2 * v[1] + 85.9 * v[2]) / st / 333e3
rows = []
for n in set(a) | set(b):
    x = a.get(n, [0, 0, 0]); y = b.get(n, [0, 0, 0])
    rows.append((ms(y) - ms(x), n, [(y[k] - x[k]) / st / 1e3 for k in range(3)], ms(x), ms(y)))
rows.sort(key=lambda r: -abs(r[0]))
tot = sum(r[0] for r in rows); ti = sum(r[2][0] for r in rows); tim = sum(r[2][1] for r in rows); tdm = sum(r[2][2] for r in rows)
print('total %+.2f ms: instr %+.1f k, I-miss %+.2f k, D-miss %+.2f k a step' % (tot, ti, tim, tdm))
for d, n, dv, x, y in rows[:top]:
    print('%-28s %+6.2f ms  (%6.2f -> %6.2f)  instr %+8.1f k  I %+6.2f k  D %+6.2f k' % (n, d, x, y, dv[0], dv[1], dv[2]))
