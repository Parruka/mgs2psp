#!/usr/bin/env python3
"""rl_skip.py <boot binary> [name ...]: the MGS2_READ_SKIP value (boot.c dgt_rh_add) that leaves out the loads made by the named host
functions (and their .part / .isra / .constprop pieces), as hex offsets from rt_dgt_rd.  Default names: the renderer's frame code that the
game runs outside the draw-pipeline call (re/mesh_packets.md section 1): 0x123e80 packet buffer swap, 0x124380 chain list, 0x1238e8 DMA
kick, 0x124450 / 0x124460 packet write pointer get / set."""
import subprocess, sys
names = sys.argv[2:] or ['P123e80', 'P124380', 'P1238e8', 'P124450', 'P124460']
syms = []
for l in subprocess.run(['nm', '-n', sys.argv[1]], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
base = [a for a, n in syms if n == 'rt_dgt_rd'][0]
out = []
for i, (a, n) in enumerate(syms):
    if any(n == x or n.startswith(x + '.') for x in names):
        e = syms[i + 1][0] if i + 1 < len(syms) else a + 1
        out.append('%x:%x' % ((a - base) & (1 << 64) - 1, (e - base) & (1 << 64) - 1))
        print('%s %x..%x' % (n, a, e), file=sys.stderr)
print(','.join(out))
