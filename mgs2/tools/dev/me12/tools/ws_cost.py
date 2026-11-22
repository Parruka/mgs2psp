#!/usr/bin/env python3
# ws_cost.py <binary> <log>...: me12 (task #142) step 3: what the design's guards would test, per route, from a wruns3 log ("me12 wt:"
# writer lines and "me12 ws:" store-site counts).  Guard sets: G0 = the class-0 writers (in the transfer's own step) outside the boot
# (a function whose class-0 writes all began by vb 40 is the boot's); G1t = the class-1 writers into texture sources (REF upload);
# G1r = the class-1 writers into referenced data that is not a texture source (REF), the hybrid's overflow guards.  For each set: the
# stores of its own sites (a guard per site), and the stores of every site in its functions (a guard per function), per vblank of the
# run; the scratchpad DMA (dma208) is the runtime's and counted apart.
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
for log in sys.argv[2:]:
    sites = {}; lastvb = None; nsites = drop = None
    c0first = collections.defaultdict(lambda: 10 ** 9)   # function -> earliest first vb of its class-0 lines
    c0max = collections.defaultdict(int)                  # function -> latest first vb of its class-0 lines
    sets = {'G0': set(), 'G1t': set(), 'G1r': set()}; c0sites = collections.defaultdict(set)
    for line in open(log, errors='replace'):
        m = rw.search(line)
        if m:
            w, c, k, up = int(m.group(1), 16), int(m.group(2)), m.group(3), bool(m.group(4)); vb = int(m.group(9))
            if c == 0:
                f = name(w); c0sites[f].add(w); c0max[f] = max(c0max[f], vb)
            elif c == 1 and k == 'REF': sets['G1t' if up else 'G1r'].add(w)
            continue
        m = rs.match(line)
        if m: sites[int(m.group(1), 16)] = int(m.group(2)); continue
        m = rt.match(line)
        if m: nsites, drop, lastvb = int(m.group(1)), int(m.group(2)), int(m.group(3))
    boot = sorted(f for f in c0sites if c0max[f] <= 40)
    for f, ws in c0sites.items():
        if f not in boot: sets['G0'] |= ws
    allst = sum(sites.values()); vb = lastvb or 1
    byfn = collections.defaultdict(int); nfs = collections.Counter()
    for w, n in sites.items(): f = name(w); byfn[f] += n; nfs[f] += 1
    print('== %s: %d store sites (dropped %s), %d stores, last vblank %s: %.0f stores a vblank' % (log, len(sites), drop, allst, lastvb, allst / vb))
    print('  boot-only class-0 writers (no guard if the ME takes over after the boot): %s' % ' '.join(boot))
    for k in ('G0', 'G1t', 'G1r'):
        ws = set(w for w in sets[k] if w != 0x208); fns = sorted(set(name(w) for w in ws))
        ns = sum(sites.get(w, 0) for w in ws); nf = sum(byfn[f] for f in fns); nsf = sum(nfs[f] for f in fns)
        print('  %-3s: %d functions, %d of their sites wrote in flight: those sites %.0f stores a vblank; all %d sites of the functions %.0f a vblank%s'
              % (k, len(fns), len(ws), ns / vb, nsf, nf / vb, ('; and the scratchpad DMA: %.0f copies a vblank' % (sites.get(0x208, 0) / vb)) if 0x208 in sets[k] else ''))
        top = sorted(fns, key=lambda f: -byfn[f])[:12]
        print('       most stores (function: all its sites a vblank / in-flight sites a vblank): %s' % '; '.join(
              '%s %.0f / %.0f' % (f, byfn[f] / vb, sum(sites.get(w, 0) for w in ws if name(w) == f) / vb) for f in top))
