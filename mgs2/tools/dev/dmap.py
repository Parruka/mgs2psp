#!/usr/bin/env python3
"""dmap.py [-g <bucket bytes>] <dhist or dacc file> <elf> [<elf>...]: D-misses (or accesses) by data symbol; g_mem split by guest address range."""
import sys, os, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
args = sys.argv[1:]; gran = 1024
if args[0] == '-g': gran = int(args[1]); args = args[2:]
f, elfs = args[0], args[1:]
syms = []
for e in elfs:
    for ln in subprocess.run([NM, '-S', '-n', e], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 4 and p[2] in 'bBdDgGsSrRvV':
            syms.append((int(p[0], 16), int(p[1], 16), p[3]))
syms.sort(); addrs = [a for a, _, _ in syms]
gm = [s for s in syms if s[2] == 'g_mem']
gbase = gm[0][0] if gm else None
tot = collections.Counter(); n = 0
step = gran if f.endswith('dhist') else 64
for ln in open(f):
    a, c = ln.split(); a = int(a, 16); c = int(c); n += c
    if gbase is not None and gbase <= a < gbase + gm[0][1]:
        o = a - gbase
        k = 'g_mem guest SPR 0-16K' if o < 0x4000 else 'g_mem guest RAM %07x-%07x' % (o & ~0xfffff, (o & ~0xfffff) + 0xfffff)
        tot[k] += c; continue
    i = bisect.bisect_right(addrs, a) - 1
    if i >= 0 and a < syms[i][0] + max(syms[i][1], step): tot[syms[i][2]] += c
    else: tot['? %07x' % (a & ~0xffff)] += c
print('total', n)
for k, c in tot.most_common(int(os.environ.get('TOP', '45'))): print('%8d %5.1f%%  %s' % (c, 100.0 * c / n, k))
