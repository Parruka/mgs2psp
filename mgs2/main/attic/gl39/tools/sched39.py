"""sched39.py (gl39): the turns' schedule.  A turn is 100 vblanks from vb 24,000 (turn t = (vb - 24000) // 100).  Each turn has
  kind   MGS2_BOUND_CYCLE: 0 all drawn (ME), 5 stage program off, 7 nothing drawn, the main CPU's turn (ME resting),
         8 nothing drawn, the ME's turn (empty pictures through the hand-over; gl39)
  F1     MGS2_NAT_F1_CYCLE (1 on: gl39's default; 0 in the F1-off turns)
  dig    MGS2_DIG_CYCLE (1 = no frame digest in the turn; digests exist only in 29,490-31,400 anyway)
  load   MGS2_LOAD_CYCLE (kind 8 only: 0 none, 1 arithmetic, 2 cached reads, 3 uncached writes)
  m5     MGS2_OPT2_CYCLE's value for the turn (16 values, turn t takes value t % 16): M5 on (fffff9df) or off (fffbf9df)
The re-use is off everywhere (MGS2_NAT_REUSE_CYCLE=0,0,0,0).  sched39.py keys | table | py"""
import sys
S = {}   # turn -> [kind, f1, digoff, load, scene]
def put(t, k, sc, f=1, d=0, l=0): assert t not in S; S[t] = [k, f, d, l, sc]
# the scenes as gl38's schedule found them (build_gl38/tools/sched38.py)
for t, k in enumerate([0, 0, 7, 8, 0, 0, 7, 8, 0, 7, 0]): put(t, k, 'first deck scene' if 1 <= t <= 9 else 'deck, change of scene')
for t, (k, l) in zip(range(11, 20), [(0,0),(0,0),(7,0),(8,0),(0,0),(7,0),(8,3),(0,0),(0,0)]): put(t, k, 'nothing drawn', l=l)
for t in (20, 21): put(t, 0, 'deck, change of scene')
# steady deck (26,200-28,700): the codec screen; no ship groups there (F1 changes nothing); no digests
for t, (k, l) in zip(range(22, 47), [(0,0),(0,0),(7,0),(8,0),(8,1),(8,2),(8,3),(0,0),(5,0),(7,0),(0,0),(8,0),(8,1),(8,2),(8,3),(7,0),(0,0),(5,0),
                                     (0,0),(7,0),(8,0),(8,3),(0,0),(7,0),(0,0)]):
    put(t, k, 'steady deck', l=l)
for t in (47, 48, 49): put(t, 0, 'change of scene')
put(50, 0, 'rain begins')
# rain wait (29,100-30,800): t51-t54 still settle and have no digests (MGS2_GL_DIG begins at 29,490); t55-t65 flat; t66-t67 heavier
for t, k in zip(range(51, 55), [0, 0, 7, 0]): put(t, k, 'rain wait, first turns')
#                     kind f1 dig load
for t, v in zip(range(55, 68), [(0, 1, 0, 0),    # t55 K0 digests on, F1 on: digests against the twin
                                (0, 1, 1, 0),    # t56 K0 digest off
                                (7, 1, 1, 0),    # t57 K7 digest off
                                (8, 1, 1, 0),    # t58 K8, no load
                                (8, 1, 1, 1),    # t59 K8, arithmetic
                                (8, 1, 1, 2),    # t60 K8, cached reads
                                (8, 1, 1, 3),    # t61 K8, uncached writes
                                (0, 1, 1, 0),    # t62 K0 digest off (second copy)
                                (0, 0, 1, 0),    # t63 K0 digest off, F1 off
                                (7, 1, 1, 0),    # t64 K7 digest off, M5 off (by t % 16 == 0): M5 alone against t57
                                (0, 0, 0, 0),    # t65 K0 digests on, all off (M5 off by t % 16 == 1, F1 off): gl38's default
                                (0, 1, 1, 0),    # t66 K0 digest off (heavier scene)
                                (7, 1, 0, 0)]):  # t67 K7 digests on (heavier scene): digests against the twin
    put(t, v[0], 'rain wait', f=v[1], d=v[2], l=v[3])
# rain walk (30,800-31,300)
for t, v in zip(range(68, 73), [(0, 1, 0, 0), (0, 1, 1, 0), (7, 1, 1, 0), (8, 1, 0, 3), (0, 1, 1, 0)]):
    put(t, v[0], 'rain walk', f=v[1], d=v[2], l=v[3])
put(73, 0, 'the walk ends')
N = 74; assert sorted(S) == list(range(N))
M5OFF = (0, 1)   # t % 16
bound = ''.join(str(S[t][0]) for t in range(N)); f1 = ''.join(str(S[t][1]) for t in range(N))
dig = ''.join(str(S[t][2]) for t in range(N)); load = ''.join(str(S[t][3]) for t in range(N))
opt2 = ','.join('fffbf9df' if i in M5OFF else 'fffff9df' for i in range(16)); opt = ','.join(['ffffffff'] * 16); half = ','.join(['b'] * 16)
KN = {0: 'K0 all drawn', 5: 'K5 stage program off', 7: 'K7 nothing drawn, main CPU, ME resting', 8: 'K8 nothing drawn, ME turn'}
LN = ['-', 'arithmetic', 'cached reads', 'uncached writes']
def digtxt(t):
    lo = 24000 + 100 * t
    return 'none (before 29,490)' if lo + 99 < 29490 else 'off' if S[t][2] else 'on'
if __name__ == '__main__':
    m = sys.argv[1] if len(sys.argv) > 1 else 'keys'
    if m == 'keys':
        print('MGS2_OPT_CYCLE=24000:100:' + opt); print('MGS2_OPT2_CYCLE=' + opt2); print('MGS2_HALF_CYCLE=' + half)
        print('MGS2_BOUND_CYCLE=' + bound); print('MGS2_NAT_F1_CYCLE=' + f1); print('MGS2_DIG_CYCLE=' + dig); print('MGS2_LOAD_CYCLE=' + load)
    elif m == 'table':
        print('turn | vblanks | kind | F1 | digests | ME load | M5 | scene')
        for t in range(N):
            k, f, d, l, sc = S[t]
            print('t%-2d | %d-%d | %s | %s | %s | %s | %s | %s' % (t, 24000 + 100 * t, 24100 + 100 * t, KN[k], 'on' if f else 'OFF', digtxt(t),
                  LN[l] if k == 8 else '-', 'OFF' if t % 16 in M5OFF else 'on', sc))
    elif m == 'py': print(repr([(t, 24000 + 100 * t, S[t][0], S[t][1], S[t][2], S[t][3], 0 if t % 16 in M5OFF else 1, S[t][4]) for t in range(N)]))
