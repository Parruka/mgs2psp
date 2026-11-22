#!/usr/bin/env python3
# ws_cost2.py <binary> <log>...: me12 (task #142) step 3: the guard cost of the lists over all given routes.  The sets (as wt_union.py:
# G0 = class-0 writers outside the boot, G1t = class-1 into texture sources, G1r = class-1 into other referenced data) are taken over
# all logs together; then for each log, the stores a vblank of every store site in the set's functions (a guard per function, the
# design) and of the sites themselves that wrote in flight on any route (a guard per site), with all stores and the scratchpad DMA's
# copies for scale.
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
rs = re.compile(r'^me12 ws: site ([0-9a-f]+) stores (\d+)')
rt = re.compile(r'^me12 ws totals: store sites (\d+) \(dropped (\d+)\), last vblank (\d+)')
rf = re.compile(r'^me12 copy fit')
sets = collections.defaultdict(set); fsets = collections.defaultdict(set); c0max = collections.defaultdict(int); c0s = collections.defaultdict(set)
logs = {}
for log in sys.argv[2:]:
    sites = {}; lastvb = None; fit = []
    for line in open(log, errors='replace'):
        m = rw.search(line)
        if m:
            w, c, k, up, vb = int(m.group(1), 16), int(m.group(2)), m.group(3), bool(m.group(4)), int(m.group(9)); f = name(w)
            if c == 0: c0s[f].add(w); c0max[f] = max(c0max[f], vb)
            elif k == 'REF': sets['G1t' if up else 'G1r'].add(w); fsets['G1t' if up else 'G1r'].add(f)
            continue
        m = rs.match(line)
        if m: sites[int(m.group(1), 16)] = int(m.group(2)); continue
        m = rt.match(line)
        if m: lastvb = int(m.group(3)); continue
        if rf.match(line): fit.append(line.strip())
    logs[log] = (sites, lastvb, fit)
for f, ws in c0s.items():
    if c0max[f] > 40: sets['G0'] |= ws; fsets['G0'].add(f)
for k in fsets: fsets[k].discard('dma208')
for log, (sites, lastvb, fit) in logs.items():
    vb = lastvb or 1; byfn = collections.defaultdict(int); nf = collections.Counter()
    for w, n in sites.items(): byfn[name(w)] += n; nf[name(w)] += 1
    print('== %s: %d store sites, %.0f stores a vblank (%d vblanks); the scratchpad DMA %.1f copies a vblank' % (log, len(sites), sum(sites.values()) / vb, vb, sites.get(0x208, 0) / vb))
    for k in ('G0', 'G1t', 'G1r'):
        fs = fsets[k]; ws = set(w for w in sets[k] if w != 0x208)
        a = sum(byfn[f] for f in fs); s = sum(sites.get(w, 0) for w in ws)
        print('  %-3s: %3d functions (%d sites): %.0f stores a vblank; their %d sites that wrote in flight: %.0f a vblank' % (k, len(fs), sum(nf[f] for f in fs), a / vb, len(ws), s / vb))
        print('       the most: %s' % '; '.join('%s %.0f' % (f, byfn[f] / vb) for f in sorted(fs, key=lambda f: -byfn[f])[:8]))
    both = fsets['G0'] | fsets['G1t']
    print('  G0 + G1t (design A): %d functions, %.0f stores a vblank; with G1r (design C): %d functions, %.0f a vblank' % (
          len(both), sum(byfn[f] for f in both) / vb, len(both | fsets['G1r']), sum(byfn[f] for f in both | fsets['G1r']) / vb))
    for l in fit: print('  ' + l)
