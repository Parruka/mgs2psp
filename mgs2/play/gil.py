"""gil.py: shared loader for the play-probe analyses.  Functions and PSP sizes as ws.py (h1size full units, -O2, no PGO); sessions =
run/gi_<tag>.txt (fork server; steps = stop - 30500 - 1 from run/batch*.txt) or 'tut' (tut/gi_tut.txt: the timed tutorial, vb 24001-25200)."""
import re, subprocess, collections, glob
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
        if len(p) == 4 and p[3].startswith(pre + 'P'):
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
size = lambda k: U[k[0]][1].get(k[1], 0)
STEPS = {'tut': 1200}
for b in ['/home/claude/play/run/batch4.txt', '/home/claude/play/run/batch5.txt']:
    for ln in open(b):
        p = ln.split()
        if len(p) >= 2: STEPS[p[0]] = int(p[1]) - 30500 - 1
def path(t): return '/home/claude/play/tut/gi_tut.txt' if t == 'tut' else '/home/claude/play/run/gi_%s.txt' % t
def load(t, split=False):
    """Counter (unit, function) -> guest instructions (outside + inside the draw pipeline; split: (outside, inside))"""
    c = collections.Counter(); d = collections.Counter()
    for ln in open(path(t)):
        pc, a, g = ln.split(); pc = int(pc, 16); u = unit_of(pc); f = pc2f.get((u, pc))
        if f is None: continue
        if split: c[(u, f)] += int(a); d[(u, f)] += int(g)
        else: c[(u, f)] += int(a) + int(g)
    return (c, d) if split else c
