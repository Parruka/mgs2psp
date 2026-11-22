#!/usr/bin/env python3
# wt_union.py <binary> <log>...: me12 (task #142) the guard lists over routes: per set (G0 = class-0 writers outside the boot, G1t =
# class-1 writers into texture sources, G1r = class-1 writers into other referenced data), the functions on each route, the union, and
# how many routes each function is on; by stage overlay (S_<stage>_...) or the main program (P...).
import sys, subprocess, bisect, re, collections
binary = sys.argv[1]
syms = []
for line in subprocess.run(['nm', '-n', '--defined-only', binary], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [a for a, _ in syms]
def name(off):
    if off < 0x1000: return 'dma%x' % off
    i = bisect.bisect_right(addrs, off) - 1
    return syms[i][1] if i >= 0 else '?%x' % off
rw = re.compile(r'me12 wt: writer ([0-9a-f]+) class (\d) (tag|after tag|REF|normal)( upload)?( in pipeline)?: writes (\d+) bytes (\d+) steps (\d+), first vb (\d+) at')
per = {}
for log in sys.argv[2:]:
    sets = collections.defaultdict(set); c0max = collections.defaultdict(int); c0 = set()
    for line in open(log, errors='replace'):
        m = rw.search(line)
        if not m: continue
        w, c, k, up, vb = int(m.group(1), 16), int(m.group(2)), m.group(3), bool(m.group(4)), int(m.group(9)); f = name(w)
        if c == 0: c0.add(f); c0max[f] = max(c0max[f], vb)
        elif k == 'REF': sets['G1t' if up else 'G1r'].add(f)
        else: sets['G1p'].add(f)   # packets (tags, data after tags): copied, so no guard
    sets['G0'] = set(f for f in c0 if c0max[f] > 40); sets['boot'] = set(f for f in c0 if c0max[f] <= 40)
    per[log] = sets
for k in ('G0', 'G1t', 'G1r', 'G1p', 'boot'):
    cnt = collections.Counter()
    for log in per: cnt.update(per[log][k])
    stg = collections.Counter(('main' if not f.startswith('S_') else f.split('_')[1]) for f in cnt)
    print('%-4s: %s; union %d (on all routes %d), by unit: %s' % (k, ', '.join('%s %d' % (l.split('/')[-1], len(per[l][k])) for l in per), len(cnt),
          sum(1 for f in cnt if cnt[f] == len(per)), ' '.join('%s %d' % x for x in sorted(stg.items()))))
    if k in ('G0', 'G1t', 'boot'): print('      %s' % ' '.join('%s(%d)' % (f, cnt[f]) for f in sorted(cnt)))
