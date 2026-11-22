# pxent.py <gsref stdout>...: per vblank, the pixels each VU1 entry wrote (gsref's "entry %06x px" counts), shown frames (with df0 calls) marked
import sys, re, collections
NAME = {'000000': 'path 2/3', '1991d8': 'df0', '19a9f0': '+25f0 fans', '19ac78': '+2870 fans', '19af68': '+2b60 lines', '19bb50': '+3740 sprites',
        '19be50': '+3a38 quads', '198b60': '+780 lit', '198e58': '+a70 skinned', '199478': '+1088', '194478': 'P01', '1944c0': 'P01b',
        '19fe08': 'P06+0', '1a0160': 'P06+358', '1a0540': 'P06+738', '1a08c0': 'P06+ab8', '1a31e0': 'P07', '1a8148': 'P09 sea',
        '201f78': 'stage 201f78', '202008': 'stage 202008', '202170': 'stage 202170', '202390': 'stage post 202390'}
DRAWN = {'000000', '1991d8', '19a9f0', '19ac78', '19fe08', '1a0160', '1a0540', '1a08c0', '1a31e0'}
for fn in sys.argv[1:]:
    px = collections.defaultdict(lambda: collections.Counter()); tot = {}; calls = {}
    for l in open(fn, errors='replace'):
        m = re.match(r'vb (\d+)\s+entry ([0-9a-f]{6}) px\s+(\d+)', l)
        if m: px[int(m.group(1))][m.group(2)] += int(m.group(3)); continue
        m = re.match(r'vb (\d+)\s+pixels written\s+(\d+)', l)
        if m: tot[int(m.group(1))] = int(m.group(2)); continue
        m = re.match(r'vb (\d+)\s+vu1 calls total\s+(\d+)', l)
        if m: calls[int(m.group(1))] = int(m.group(2))
    print('==', fn)
    for vb in sorted(tot):
        p = px[vb]; shown = p.get('1991d8', 0) > 0 or any(k == '1991d8' for k in p)
        t = tot[vb]; und = sum(v for k, v in p.items() if k not in DRAWN)
        print('vb %d %s: %d px, %d calls; not drawn on the PSP %d px (%.1f%%)' % (vb, 'FULL' if shown else 'reduced', t, calls.get(vb, 0), und, 100.0 * und / max(t, 1)))
        for k, v in sorted(p.items(), key=lambda kv: -kv[1]):
            if v: print('   %-20s %s %9d %5.1f%%' % (NAME.get(k, k), ' ' if k in DRAWN else 'X', v, 100.0 * v / max(t, 1)))
