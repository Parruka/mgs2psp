# census6.py <log>...: lane 3's share of the effect calls (renderer_notes 11.33) in sb6 x86 logs: per effect entry (the sort pass's types
# 1-5 by the MSCAL table 0x17d020) the census's calls and how many each lane made ("sb census" lines; the generic walk's the rest), lane 3's
# prims, its 25f0 / 2870 calls read from the game's streams and those that needed the VU memory, the prim tags it did not take by the first
# failed test, the prim tags the generic walk took by why, and MGS2_SB_CHK's checked / differing counts (lanes 1, 2, 3 and 4)
import re, sys, gzip
TYPES = {0x19af68: '1 +2b60', 0x19a9f0: '2 +25f0', 0x19ac78: '3 +2870', 0x19bb50: '4 +3740', 0x19be50: '5 +3a38'}
R3 = re.compile(r'prims (\d+); (\d+) 25f0 / 2870 calls from the game.s streams, (\d+) from the VU memory, checked (\d+), differing (\d+); prim tags '
                r'not taken: room (\d+), tag 0 (\d+), tag 1 (\d+), tags 2-5 (\d+), data (\d+), declined (\d+); prim tags left to the generic walk: '
                r'lane 3 off (\d+), not taken (\d+), data left (\d+), no TTE (\d+), not the one walk (\d+)')
R4 = re.compile(r'slots (\d+); (\d+) P07 calls from the game.s streams, (\d+) from the VU memory, checked (\d+), differing (\d+)')
R12 = re.compile(r'groups (\d+) \(lane 1\) (\d+) \(lane 2\); (\d+) df0 calls from the game.s streams, checked (\d+), differing (\d+); (\d+) P06 calls '
                 r'from the game.s streams, (\d+) from the VU memory, checked (\d+), differing (\d+)')
RC = re.compile(r'^sb census: to vb (\d+): entry ([0-9a-f]+): (\d+) calls: the generic walk (\d+), lane 1 (\d+), lane 2 (\d+), lane 3 (\d+), lane 4 (\d+)')
for p in sys.argv[1:]:
    cen = {}; lanes = {}; l3 = l4 = l12 = None
    for l in (gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')):
        if l.startswith('xlive census:'):
            m = re.search(r'entry ([0-9a-f]+) .*: (\d+) calls', l); e = int(m.group(1), 16)
            if e in TYPES: cen[e] = int(m.group(2))
        elif l.startswith('sb census:'):
            m = RC.match(l); e = int(m.group(2), 16)
            if e in TYPES: lanes[e] = [int(x) for x in m.groups()[2:]]
        elif l.startswith('sb lane 3:'): l3 = [int(x) for x in R3.search(l).groups()]
        elif l.startswith('sb lane 4:'): l4 = [int(x) for x in R4.search(l).groups()]
        elif l.startswith('sb lane:'): l12 = [int(x) for x in R12.search(l).groups()]
    print(p)
    tot = [0] * 6
    for e in sorted(TYPES, key=lambda e: TYPES[e]):
        c = cen.get(e, 0); ln = lanes.get(e, [c, c, 0, 0, 0, 0])
        print('  type %s (entry %06x): census %d calls; generic %d, lane 1 %d, lane 2 %d, lane 3 %d (%.4f%%), lane 4 %d' % (TYPES[e], e, c, ln[1], ln[2], ln[3],
              ln[4], 100.0 * ln[4] / c if c else 0, ln[5]))
        for k in range(6): tot[k] += ([c] + ln[1:])[k]
    print('  all five: census %d; generic %d, lane 1 %d, lane 3 %d (%.4f%%)' % (tot[0], tot[1], tot[2], tot[4], 100.0 * tot[4] / tot[0] if tot[0] else 0))
    if l3:
        pr, st, vm, ck, df = l3[:5]; miss = l3[5:11]; gen = l3[11:16]
        print('  lane 3: prims %d (= its calls: %s), 25f0 / 2870 calls from the streams %d, from the VU memory %d; checked %d, differ %d'
              % (pr, 'yes' if pr == tot[4] else 'NO, census lane 3 %d' % tot[4], st, vm, ck, df))
        print('  prim tags not taken: room %d, tag 0 %d, tag 1 %d, tags 2-5 %d, data %d, declined %d' % tuple(miss))
        print('  prim tags left to the generic walk: lane 3 off %d, not taken %d, data left %d, no TTE %d, not the one walk %d (no call: the '
              'translator does not see those transfers); in the one walk %d against the generic effect calls %d' % (tuple(gen) + (sum(gen[:4]), tot[1])))
    if l12: print('  lanes 1 2: groups %d / %d; df0 from streams %d checked %d differ %d; P06 from streams %d, VU %d, checked %d, differ %d' % tuple(l12))
    if l4: print('  lane 4: slots %d; P07 from streams %d, VU %d, checked %d, differ %d' % tuple(l4))
