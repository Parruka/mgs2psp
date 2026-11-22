#!/usr/bin/env python3
# wt_names.py <log> <binary> [top]: me12 (task #142): names the writers of the "me12 wt:" lines (each a store's host address as an offset in
# the executable) with nm: the function that holds it (P<guest address> for the game's translated functions; dma208 = the scratchpad DMA to
# RAM), then sums by function, class (0: the transfer's own step, 1: the step after), kind and whether the PS2 draw pipeline was running.
import sys, subprocess, bisect, re, collections
log, binary = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 60
syms = []
for line in subprocess.run(['nm', '-n', '--defined-only', binary], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW':
        syms.append((int(p[0], 16), p[2]))
syms.sort()
addrs = [a for a, _ in syms]
def name(off):
    if off < 0x1000:
        return 'dma%x' % off
    i = bisect.bisect_right(addrs, off) - 1
    return syms[i][1] if i >= 0 else '?%x' % off
rx = re.compile(r'me12 wt: writer ([0-9a-f]+) class (\d) (tag|after tag|REF|normal)( upload)?( in pipeline)?: writes (\d+) bytes (\d+) steps (\d+), first vb (\d+) at ([0-9a-f]+)')
agg = collections.defaultdict(lambda: [0, 0, 0, None, 0])
bycls = collections.Counter(); byk = collections.Counter()
for line in open(log, errors='replace'):
    if line.startswith('me12 wt totals') or line.startswith('me12 untracked'):
        print(line.rstrip()); continue
    m = rx.search(line)
    if not m:
        continue
    w, c, k, up, pl, n, b, st, vb, a = m.groups()
    f = name(int(w, 16)); kk = k + (' upload' if up else '')
    e = agg[(f, int(c), kk, bool(pl))]
    e[0] += int(n); e[1] += int(b); e[2] = max(e[2], int(st)); e[4] += 1
    if e[3] is None or int(vb) < e[3][0]:
        e[3] = (int(vb), a)
    bycls[(int(c), bool(pl))] += int(b); byk[(int(c), kk)] += int(b)
print('bytes written into bytes in flight, by class (0 = in the transfer\'s step, 1 = the step after) and draw pipeline:')
for (c, pl), b in sorted(bycls.items()):
    print('  class %d %s: %d' % (c, 'in pipeline' if pl else 'outside it', b))
print('by class and kind:')
for (c, k), b in sorted(byk.items()):
    print('  class %d %-16s %d' % (c, k, b))
print('writers (function, class, kind, pipeline): writes, bytes, most steps of one site, sites, first vb at address')
for (f, c, k, pl), e in sorted(agg.items(), key=lambda x: -x[1][1])[:top]:
    print('  %-24s %d %-16s %-3s %10d %11d %7d %4d  vb %d at %s' % (f, c, k, 'in' if pl else 'out', e[0], e[1], e[2], e[4], e[3][0], e[3][1]))
