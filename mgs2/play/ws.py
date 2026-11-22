"""ws.py <tag>...: the working set of play sessions (run/gi_<tag>.txt, run/cov_<tag>.txt) against the full ELF and w00a units.
Functions: from the full headers (h1size/full_l: each function's guest pcs); PSP size: psp-nm of h1size/full_gen.o / full_gen_s_w00a.o
(v23 flags, -O2, no PGO).  Reduced build = the functions in vgz/W (the units v24 ships; cov_u.txt)."""
import re, subprocess, sys, collections
H = '/home/claude/h1size'
def funcs(hdr, pre):
    F = {}; cur = None
    rx = re.compile(r'^static void %sP([0-9a-f]+)\(void\)$' % pre); mk = re.compile(r'^/\*([0-9a-f]{6})\*/')
    for ln in open(hdr):
        m = rx.match(ln)
        if m: cur = int(m.group(1), 16); F[cur] = []; continue
        m = mk.match(ln)
        if m and cur is not None: F[cur].append(int(m.group(1), 16))
    return F
def sizes(obj, pre):
    S = {}
    for ln in subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-S', obj], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 4 and p[3].startswith(pre + 'P'):   # gcc clones (.part.N, .isra.N, .constprop.N) count with their function
            try: a = int(p[3][len(pre) + 1:].split('.')[0], 16)
            except ValueError: continue
            S[a] = S.get(a, 0) + int(p[1], 16)
    return S
U = {'elf': (funcs(H + '/full_l/gen_elf.h', ''), sizes(H + '/full_gen.o', '')),
     'w00a': (funcs(H + '/full_l/gen_s_w00a.h', 'S_w00a_'), sizes(H + '/full_gen_s_w00a.o', 'S_w00a_'))}
red = collections.defaultdict(set)
for ln in open('/home/claude/vgz/W/cov_u.txt'):
    u, a = ln.split(); u = 'elf' if u == 'elf' else ('w00a' if 'w00a' in u else None)
    if u: red[u].add(int(a, 16))
pc2f = {}
for u, (F, S) in U.items():
    for f, pcs in F.items():
        for pc in pcs: pc2f[(u, pc)] = f
def unit_of(pc): return 'w00a' if pc >= 0x1f1c00 else 'elf'
tags = sys.argv[1:]
tot = collections.defaultdict(lambda: [0, 0, set()])   # (unit, f) -> [count, dg count, tags]
for t in tags:
    for ln in open('/home/claude/play/run/gi_%s.txt' % t):
        pc, c, d = ln.split(); pc = int(pc, 16); u = unit_of(pc); f = pc2f.get((u, pc))
        if f is None: continue
        e = tot[(u, f)]; e[0] += int(c); e[1] += int(d); e[2].add(t)
cov = collections.defaultdict(set)
for t in tags:
    for ln in open('/home/claude/play/run/cov_%s.txt' % t):
        u, a = ln.split(); u = 'elf' if u == 'elf' else ('w00a' if 'w00a' in u else None)
        if u: cov[u].add(int(a, 16))
for u in ('elf', 'w00a'):
    F, S = U[u]; ex = {f for (uu, f) in tot if uu == u} | cov[u]
    new = ex - red[u]
    print('%s: %d functions in the unit (%.2f MB), reduced build %d (%.2f MB); run in these sessions %d (%.2f MB); not in the reduced build %d (%.1f KB)' % (
        u, len(F), sum(S.values()) / 1e6, len(red[u]), sum(S.get(f, 0) for f in red[u]) / 1e6, len(ex), sum(S.get(f, 0) for f in ex) / 1e6,
        len(new), sum(S.get(f, 0) for f in new) / 1e3))
    allc = sum(tot[(u, f)][0] + tot[(u, f)][1] for f in ex if (u, f) in tot)
    newc = sum(tot[(u, f)][0] + tot[(u, f)][1] for f in new if (u, f) in tot)
    print('   guest instructions: %d in all, %d (%.4f%%) in functions outside the reduced build' % (allc, newc, 100.0 * newc / max(1, allc)))
    for f in sorted(new, key=lambda f: -(tot[(u, f)][0] + tot[(u, f)][1]) if (u, f) in tot else 0)[:25]:
        e = tot.get((u, f), [0, 0, set()])
        print('     %06x  %7d B  instr %10d (dg %d)  sessions %s' % (f, S.get(f, 0), e[0], e[1], ','.join(sorted(e[2]))))
