# census5.py <log>...: lane 4's share of the P07 calls (renderer_notes 11.32) in sb5 x86 logs: the census's P07 calls (entry 1a31e0), the
# slots lane 4 took (one call each), the lane's P07 calls read from the game's streams and those that needed the VU memory (vc_p07_rec -8),
# the STCYCL 7 / 1 tags it did not take by the first failed test, and MGS2_SB_CHK's checked / differing counts (lanes 1, 2 and 4)
import re, sys
R4 = re.compile(r'slots (\d+); (\d+) P07 calls from the game.s streams, (\d+) from the VU memory, checked (\d+), differing (\d+); STCYCL 7 / 1 tags '
                r'not taken: room (\d+), tag 0 (\d+), tags 1-5 (\d+), tags 6-19 (\d+), data (\d+), declined (\d+)')
R12 = re.compile(r'groups (\d+) \(lane 1\) (\d+) \(lane 2\); (\d+) df0 calls from the game.s streams, checked (\d+), differing (\d+); (\d+) P06 calls '
                 r'from the game.s streams, (\d+) from the VU memory, checked (\d+), differing (\d+)')
for p in sys.argv[1:]:
    cen = l4 = l12 = None
    for l in open(p, errors='replace'):
        if l.startswith('xlive census:') and 'entry 1a31e0' in l: cen = int(re.search(r': (\d+) calls', l).group(1))
        elif l.startswith('sb lane 4:'): l4 = [int(x) for x in R4.search(l).groups()]
        elif l.startswith('sb lane:'): l12 = [int(x) for x in R12.search(l).groups()]
    print(p)
    print('  P07 calls in the census: %s' % cen)
    if l4:
        s, st, vm, ck, df, room, t0, t15, t619, data, decl = l4
        print('  lane 4: slots %d (%.4f%% of the census), from the streams %d, from the VU memory %d (%.4f%% of those converted); checked %d, differ %d'
              % (s, 100.0 * s / cen if cen else 0, st, vm, 100.0 * vm / (st + vm) if st + vm else 0, ck, df))
        print('  not taken: census - slots = %d; STCYCL 7 / 1 tags not taken: room %d, tag 0 %d, tags 1-5 %d, tags 6-19 %d, data %d, declined %d'
              % ((cen or 0) - s, room, t0, t15, t619, data, decl))
    if l12: print('  lanes 1 2: groups %d / %d; df0 from streams %d checked %d differ %d; P06 from streams %d, VU %d, checked %d, differ %d' % tuple(l12))
