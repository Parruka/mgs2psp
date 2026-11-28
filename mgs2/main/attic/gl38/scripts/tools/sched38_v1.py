"""sched38.py (gl38): the bound turns' schedule.  A turn is 100 vblanks from vb 24,000 (turn t = (vb - 24000) // 100); each turn has a kind
K0..K7 (MGS2_BOUND_CYCLE, a digit a turn) and F1's switch (MGS2_NAT_F1_CYCLE, a digit a turn).  The re-use is gl37's cycle 1,1,0,0
(on in turns with t % 4 < 2).  sched38.py keys | table | py"""
import sys
S = {}   # turn -> (kind, f1, scene)
def put(t, k, f, sc): assert t not in S; S[t] = (k, f, sc)
# Scenes by the twin's frames (batches a frame, gl37's keys): t0 the deck scene begins; t1-t9 the first deck scene (d01t; 670-910 batches,
# changing all the time: indicative only); t10 its end; t11-t19 nothing drawn (logic only); t20-t21 the steady deck begins; t22-t46 the
# steady deck (961 batches in every frame; NO native ship groups and nothing at chains 2 and 6 there, so K1, K2, K3, K4, K6, the re-use
# and F1 change nothing: K0, K5 and K7 only); t47-t50 change of scene, the rain begins; t51-t67 the rain wait; t68-t72 the rain walk;
# t73 the walk's end (11 frames)
for t, (k, f) in enumerate([(0,0),(0,0),(3,0),(4,0),(0,1),(1,0),(2,0),(0,0),(6,0),(7,0),(0,0)]):
    put(t, k, f, 'first deck scene' if 1 <= t <= 9 else 'deck, change of scene')
for t, k in zip(range(11, 22), [0,0,7,0,0,7,0,0,0,0,0]):
    put(t, k, 0, 'nothing drawn' if t < 20 else 'deck, change of scene')
# steady deck (26,200-28,700)
for t, k in zip(range(22, 47), [0,0,0,5,7,0,5,7,0,0,5,7,0,5,7,0,0,5,7,0,5,7,0,5,7]):
    put(t, k, 0, 'steady deck')
for t in (47, 48, 49): put(t, 0, 0, 'change of scene')
put(50, 0, 0, 'rain begins')
# rain wait (29,100-30,800): the kinds that draw the ship first, each with the re-use on and off; the ship-off kinds last
for t, (k, f) in zip(range(51, 68), [(0,0),(0,0),(0,1),(0,1),(2,0),(2,0),(3,0),(3,0),(4,0),(4,0),(5,0),(1,0),(6,0),(1,0),(6,0),(7,0),(7,0)]):
    put(t, k, f, 'rain wait')
# rain walk (30,800-31,300)
for t, (k, f) in zip(range(68, 73), [(0,0),(7,0),(4,1),(2,0),(6,0)]):
    put(t, k, f, 'rain walk')
put(73, 0, 0, 'the walk ends')
N = 74; assert sorted(S) == list(range(N))
bound = ''.join(str(S[t][0]) for t in range(N)); f1 = ''.join(str(S[t][1]) for t in range(N))
KN = ['K0 all drawn', 'K1 ship off', 'K2 people off', 'K3 sorted prims and meshes off (rain kept)', 'K4 rain off but the near sheet',
      'K5 stage program off', 'K6 ship, people, sorted and rain off', 'K7 nothing drawn, ME resting']
if __name__ == '__main__':
    m = sys.argv[1] if len(sys.argv) > 1 else 'keys'
    if m == 'keys': print('MGS2_BOUND_CYCLE=' + bound); print('MGS2_NAT_F1_CYCLE=' + f1)
    elif m == 'table':
        print('turn | vblanks | kind | re-use | F1 | scene')
        for t in range(N):
            k, f, sc = S[t]; print('t%-2d | %d-%d | %s | %d | %d | %s' % (t, 24000 + 100 * t, 24100 + 100 * t, KN[k], 1 if t % 4 < 2 else 0, f, sc))
    elif m == 'py': print(repr([(t, 24000 + 100 * t, S[t][0], 1 if t % 4 < 2 else 0, S[t][1], S[t][2]) for t in range(N)]))
