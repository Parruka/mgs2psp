#!/usr/bin/env python3
"""dgt_inputs.py <dg trace out> <boot binary> [--gap N]: the draw pipeline's input (and, from "dv" lines, what VIF1 DMA reads by reference) from a GEN_DG_TRACE run (boot.c MGS2_DG_TRACE, "ir" / "iw"
lines): reads inside the pipeline of RAM words last written outside it, per 256-byte block.  Blocks are merged into ranges (gap N bytes,
default 1024); per range: reads, the pipeline functions that read it, and the code outside that last wrote it ("not written by guest code":
loaded from disc or set up before the trace's start by the runtime).  Numbers only (addresses, counts): no game data."""
import bisect, collections, subprocess, sys
out, binary = sys.argv[1:3]
gap = int(sys.argv[sys.argv.index('--gap') + 1]) if '--gap' in sys.argv else 1024
syms = []
for l in subprocess.run(['nm', '-n', binary], capture_output=True, text=True).stdout.split('\n'):
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
addrs = [a for a, _ in syms]
nm_base = dict((n, a) for a, n in syms)['rt_dgt_rd']
def who(ra, off):
    if ra == 0: return 'not written by guest code'
    if ra < 0x1000: return 'copy (%x)' % ra
    i = bisect.bisect_right(addrs, ra - off) - 1
    return syms[i][1] if i >= 0 else hex(ra)
off = 0; rd = collections.defaultdict(collections.Counter); wr = collections.defaultdict(collections.Counter); dv = collections.defaultdict(collections.Counter)
for l in open(out):
    p = l.split()
    if p[0] == 'base': off = int(p[2], 16) - nm_base
    elif p[0] in ('ir', 'iw'):
        b, ra, n = int(p[1], 16), int(p[2], 16), int(p[3])
        (rd if p[0] == 'ir' else wr)[b][who(ra, off)] += n
    elif p[0] == 'dv':
        b, ra, n = int(p[1], 16), int(p[2], 16), int(p[3]); dv[b][who(ra, off)] += n
blocks = sorted(rd)
ranges = []
for b in blocks:
    if ranges and b - ranges[-1][1] <= gap: ranges[-1][1] = b
    else: ranges.append([b, b])
tot = sum(sum(c.values()) for c in rd.values())
print('pipeline input reads: %d in %d blocks of 256 bytes, %d ranges (gap %d)' % (tot, len(blocks), len(ranges), gap))
print('\nrange              KB      reads  readers (top 4) | last writers outside (top 3)')
rows = []
for x, y in ranges:
    r = collections.Counter(); w = collections.Counter()
    for b in blocks[bisect.bisect_left(blocks, x):bisect.bisect_right(blocks, y)]: r.update(rd[b]); w.update(wr[b])
    rows.append((sum(r.values()), x, y, r, w))
for n, x, y, r, w in sorted(rows, key=lambda t: -t[0]):
    span = 'spr ' if x < 0x4000 else ''
    print('%s%07x-%07x %5d %10d  %s | %s' % (span, x, y + 256, (y + 256 - x) // 1024, n,
          ', '.join('%s %d%%' % (k, 100 * v // n) for k, v in r.most_common(4)),
          ', '.join('%s %d%%' % (k, 100 * v // max(1, sum(w.values()))) for k, v in w.most_common(3))))

if dv:
    bl = sorted(dv); rg = []
    for b in bl:
        if rg and b - rg[-1][1] <= gap: rg[-1][1] = b
        else: rg.append([b, b])
    t = sum(sum(c.values()) for c in dv.values())
    print('\nVIF1 DMA reads of words not written inside the pipeline (data sent by reference): %d words read, %d KB in %d ranges' % (t, len(bl) // 4, len(rg)))
    rows = []
    for x, y in rg:
        w = collections.Counter()
        for b in bl[bisect.bisect_left(bl, x):bisect.bisect_right(bl, y)]: w.update(dv[b])
        rows.append((sum(w.values()), x, y, w))
    for n, x, y, w in sorted(rows, key=lambda r: -r[0])[:40]:
        print('%07x-%07x %5d KB %9d  %s' % (x, y + 256, (y + 256 - x) // 1024, n, ', '.join('%s %d%%' % (k, 100 * v // n) for k, v in w.most_common(3))))
