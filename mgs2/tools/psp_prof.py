#!/usr/bin/env python3
"""psp_prof.py <prof> <steps> <elf> [<elf> ...] [--top N]: per-function summary of a PspProf dump from the container's headless PPSSPP
(tools/ppsspp_mgs2.diff on top of the TT patch; harness "prof A B" in mgs2.cfg; psp_harness.md "Emulator profile").
<steps> = the vblanks profiled.  ELFs: MGS2T.elf plus the overlay ELF of the stage that was loaded (ovl_<stage>.elf, linked at the arena).
Cost model as tt_port/src/model/psp_prof.py: cycles = instructions + MISS * (I-cache + D-cache misses of a simulated 16 KB 2-way 64-byte-line
cache each), MISS = 125 measured on a PSP-3000 with hardware counters in the TT port (v10).  An estimate: no load-use, FPU or branch stalls,
no bus contention with the GE.  Numbers only, no game data."""
import bisect, collections, re, subprocess, sys
MISS = 125
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
args = [a for a in sys.argv[1:] if not a.startswith('--')]
top = int(sys.argv[sys.argv.index('--top') + 1]) if '--top' in sys.argv else 40
if '--top' in sys.argv: args.remove(str(top))
prof, steps, elfs = args[0], float(args[1]), args[2:]
syms = {}
for e in elfs:
    for ln in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 3 and p[1] in 'tTwW' and p[2] != '_ftext': syms.setdefault(int(p[0], 16), p[2])   # _ftext: the linker's name for the start of .text, which is also the first function's address
syms = sorted(syms.items()); addrs = [a for a, _ in syms]
def name(pc):
    i = bisect.bisect_right(addrs, pc) - 1
    return syms[i][1] if i >= 0 else '?'
def group(n):
    if re.match(r'(S_\w+_)?P[0-9a-f]{5,6}(_\w+)?$', n): return 'game code (stage)' if n.startswith('S_') else 'game code (ELF)'
    if n.startswith(('vu_', 'vu0_', 'vif0', 'vif_')): return 'VU0 / VIF0 model'
    if n.startswith(('k_', 'rt_poll', 'sys_', 'th_', 'ksys')): return 'kernel / poll / syscalls'
    if n.startswith(('dma_', 'gs_', 'gif_', 'vif1')): return 'DMA / GS walk'
    if n.startswith(('iop', 'IOP', 'sif', 'spu', 'sd_', 'cd', 'jsif')): return 'IOP / SIF / sound'
    if n in ('memcpy', 'memset', 'memmove', 'memcmp') or n.startswith(('__', '_')): return 'libc / libgcc'
    return 'other runtime'
hdr = open(prof).readline().strip()
agg = collections.defaultdict(lambda: [0, 0, 0]); tot = [0, 0, 0]
for ln in open(prof):
    if ln[0] == '#': continue
    pc, c, im, dm = ln.split(); pc = int(pc, 16); v = (int(c), int(im), int(dm))
    a = agg[name(pc)]
    for i in range(3): a[i] += v[i]; tot[i] += v[i]
cost = lambda a: a[0] + MISS * (a[1] + a[2])
ms = lambda a: cost(a) / steps / 333e3
T = cost(tot)
print(hdr)
print('per step (%d steps): %.2f M instructions, %.1f k I-miss, %.1f k D-miss; estimate %.1f ms at 333 MHz (instructions alone %.1f ms)' % (
    steps, tot[0] / steps / 1e6, tot[1] / steps / 1e3, tot[2] / steps / 1e3, ms(tot), tot[0] / steps / 333e3))
g = collections.defaultdict(lambda: [0, 0, 0])
for n, a in agg.items():
    for i in range(3): g[group(n)][i] += a[i]
print('\nby group:')
for n, a in sorted(g.items(), key=lambda kv: -cost(kv[1])):
    print('  %-26s %7.2f M instr %6.1f k imiss %6.1f k dmiss %7.2f ms %5.1f%%' % (n, a[0] / steps / 1e6, a[1] / steps / 1e3, a[2] / steps / 1e3, ms(a), 100 * cost(a) / T))
print('\n%-36s %8s %7s %7s %7s %6s' % ('function', 'kinstr', 'kimiss', 'kdmiss', 'ms', '%'))
for n, a in sorted(agg.items(), key=lambda kv: -cost(kv[1]))[:top]:
    print('%-36s %8.1f %7.2f %7.2f %7.2f %5.1f%%' % (n[:36], a[0] / steps / 1e3, a[1] / steps / 1e3, a[2] / steps / 1e3, ms(a), 100 * cost(a) / T))
try:
    calls = collections.Counter()
    for ln in open(prof + '.calls'):
        s, t, c = ln.split(); calls[name(int(t, 16))] += int(c)
    print('\nmost called (calls per step):')
    for n, c in calls.most_common(25): print('  %-36s %9.0f' % (n[:36], c / steps))
except FileNotFoundError:
    pass
