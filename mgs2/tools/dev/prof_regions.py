#!/usr/bin/env python3
"""regions.py <map> <arena> <ovl size> <ovl X end or 0> <dump> [<dump> ...] [--ex-hash <ref dump>]: PspProf per-PC counts summed
by code region (the object that holds the PC, from the link map's input sections; the overlay arena split into the stage's reduced
part and its extra part)."""
import re, sys, bisect, collections
args = sys.argv[1:]; exh = None
if '--ex-hash' in args: i = args.index('--ex-hash'); exh = args[i + 1]; del args[i:i + 2]
mp, arena, osz, xend = args[0], int(args[1], 16), int(args[2], 16), int(args[3], 16)
dumps = args[4:]
iv = []; sec = None
for l in open(mp):
    m = re.match(r'^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$', l)
    if m: sec = None; s, a, n, o = m.groups()
    else:
        m = re.match(r'^ (\.\S+)\s*$', l)
        if m: sec = m.group(1); continue
        m = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$', l)
        if not (m and sec): sec = None; continue
        s = sec; a, n, o = m.groups(); sec = None
    if not s.startswith('.text') and not s.startswith('.sceStub') and not s.startswith('.MIPS') and s not in ('.init', '.fini'): continue
    a = int(a, 16); n = int(n, 16)
    if n == 0 or a == 0: continue
    iv.append((a, a + n, o.strip().split('/')[-1]))
iv.sort(); A = [x[0] for x in iv]
def obj(pc):
    if arena <= pc < arena + osz: return 'stage reduced part'
    if xend and arena + osz <= pc < xend: return 'stage extra part'
    i = bisect.bisect_right(A, pc) - 1
    if i >= 0 and pc < iv[i][1]: return iv[i][2]
    return 'other %07x' % (pc >> 12)
def grp(o):
    if o.startswith('stage') or o.startswith('other'): return o
    if o == 'gen.o': return 'game ELF code (gen.o)'
    if o == 'gen_x.o': return 'ELF extra (gen_x.o)'
    if o in ('gen_dispatch.o',): return 'dispatch (gen_dispatch.o)'
    if o in ('boot_rt.o',): return 'runtime (boot_rt.o)'
    if o in ('iop_rt.o', 'iop_rt_libs.o', 'iop_rt_ucontext.o', 'iop_main.o', 'spu2_lite.o', 'cdbios.o', 'jsifman.o'): return 'IOP + sound'
    if o in ('xhook.o', 'pad.o'): return 'hook (xhook.o)'
    if o in ('main_psp.o', 'hwprobe.o', 'psp_ctx.o'): return 'harness'
    return 'libraries'
def load(f):
    d = {}
    for l in open(f):
        if l[0] == '#': continue
        a, c, i, m = l.split(); d[int(a, 16)] = (int(c), int(i), int(m))
    return d
ex = set()
for f in dumps:
    C = load(f)
    if exh and f.endswith('.1'):
        import subprocess
        W = load(exh)
        S = []
        for l in subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-n', mp.replace('.map', '.elf')], capture_output=True, text=True).stdout.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tTwW': S.append((int(p[0], 16), p[2]))
        SA = [s[0] for s in S]
        fn = lambda a: S[bisect.bisect_right(SA, a) - 1][1]
        ex = {a for a in C if fn(a) == 'k_vblank_hook' and (a not in W or C[a][0] > 100000)}
    else: ex = set()
    G = collections.defaultdict(lambda: [0, 0, 0])
    for a, v in C.items():
        if a in ex: continue
        g = G[grp(obj(a))]
        for k in range(3): g[k] += v[k]
    T = [sum(g[k] for g in G.values()) for k in range(3)]
    print('%s (%s): %.3f M instr, %.1f k I-miss, %.1f k D-miss per step = %.2f ms' % (f, 'state hash taken off' if ex else 'as dumped',
          T[0] / 100e6, T[1] / 100e3, T[2] / 100e3, (1.4 * T[0] + 70 * (T[1] + T[2])) / 100 / 333000))
    for k, g in sorted(G.items(), key=lambda x: -(1.4 * x[1][0] + 70 * (x[1][1] + x[1][2]))):
        ms = (1.4 * g[0] + 70 * (g[1] + g[2])) / 100 / 333000
        if ms < 0.05: continue
        print('  %-28s %7.3f M instr %6.1f k I-miss %6.1f k D-miss  %6.2f ms' % (k, g[0] / 100e6, g[1] / 100e3, g[2] / 100e3, ms))
