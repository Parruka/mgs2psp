"""sched40.py (gl40; attic/gl40/gl40_plan.md section 6): the turns' schedule.  A turn is 100 vblanks from vb 24,000 (turn t = (vb - 24000) // 100).
Each turn has
  kind   MGS2_BOUND_CYCLE: 0 all drawn (ME), 5 stage program off, 7 nothing drawn, the main CPU's turn (ME resting),
         8 nothing drawn, the ME's turn (empty pictures through the hand-over)
  F1     MGS2_NAT_F1_CYCLE: 1 in every turn (gl40: F1 on everywhere)
  dig    MGS2_DIG_CYCLE (1 = no frame digest in the turn; digests exist only in 29,490-31,400): on only in t55, t65, t68 (and t54's last
         ten vblanks 29,490-29,499, as in gl39)
  load   MGS2_LOAD_CYCLE (kind 8 only): gl39's in t0-t50; none from t51
  xp     MGS2_XP_CYCLE (gl40): 0 gl39's copy plan, 1 the merge sort's ping-pong alone, 3 ping-pong + look-back join, 7 3 + the check
  m5     MGS2_OPT2_CYCLE's value for the turn (16 values, turn t takes value t % 16): M5 on (fffff9df) or off (fffbf9df) at t % 16 in {0, 1}
The re-use is off everywhere.  sched40.py keys | table | py"""
import sys
S = {}   # turn -> [kind, f1, digoff, load, xp, scene]
def put(t, k, sc, d=0, l=0, x=0): assert t not in S; S[t] = [k, 1, d, l, x, sc]
# t0-t50: gl39's kinds and loads (build_gl39/tools/sched39.py); XP 3 in five steady-deck K0 turns (ME-limited: for the probe)
for t, k in enumerate([0, 0, 7, 8, 0, 0, 7, 8, 0, 7, 0]): put(t, k, 'first deck scene' if 1 <= t <= 9 else 'deck, change of scene')
for t, (k, l) in zip(range(11, 20), [(0,0),(0,0),(7,0),(8,0),(0,0),(7,0),(8,3),(0,0),(0,0)]): put(t, k, 'nothing drawn', l=l)
for t in (20, 21): put(t, 0, 'deck, change of scene')
XPD = (22, 29, 32, 38, 44)
for t, (k, l) in zip(range(22, 47), [(0,0),(0,0),(7,0),(8,0),(8,1),(8,2),(8,3),(0,0),(5,0),(7,0),(0,0),(8,0),(8,1),(8,2),(8,3),(7,0),(0,0),(5,0),
                                     (0,0),(7,0),(8,0),(8,3),(0,0),(7,0),(0,0)]):
    put(t, k, 'steady deck', l=l, x=3 if t in XPD else 0)
for t in (47, 48, 49): put(t, 0, 'change of scene')
put(50, 0, 'rain begins')
#                        kind dig xp
for t, v in zip(range(51, 55), [(0, 0, 0), (0, 0, 3), (7, 0, 0), (0, 0, 3)]): put(t, v[0], 'rain wait, first turns', d=v[1], x=v[2])
for t, v in zip(range(55, 68), [(0, 0, 7),    # t55 K0 XP7, digests on: the check and the digests with XP on
                                (0, 1, 0),    # t56 K0 XP0
                                (0, 1, 3),    # t57 K0 XP3                      pair t57 - t56
                                (7, 1, 0),    # t58 K7, M5 on                   M5 pair t58 - t64
                                (0, 1, 1),    # t59 K0 XP1 (ping-pong alone)    t59 - t56
                                (0, 1, 3),    # t60 K0 XP3                      pair t60 - t61
                                (0, 1, 0),    # t61 K0 XP0
                                (8, 1, 0),    # t62 K8, no load
                                (0, 1, 3),    # t63 K0 XP3                      pair t63 - t61
                                (7, 1, 0),    # t64 K7, M5 off (t % 16 = 0)
                                (0, 0, 0),    # t65 K0 XP0, digests on, M5 off (t % 16 = 1): digests against the twin
                                (0, 1, 3),    # t66 K0 XP3 (heavier scene)      pair t66 - t67
                                (0, 1, 0)]):  # t67 K0 XP0 (heavier scene)
    put(t, v[0], 'rain wait', d=v[1], x=v[2])
for t, v in zip(range(68, 73), [(0, 0, 7), (0, 1, 0), (0, 1, 3), (7, 1, 0), (0, 1, 3)]):   # walk pair t70 - t69
    put(t, v[0], 'rain walk', d=v[1], x=v[2])
put(73, 0, 'the walk ends', d=1, x=3)
N = 74; assert sorted(S) == list(range(N))
M5OFF = (0, 1)   # t % 16
bound = ''.join(str(S[t][0]) for t in range(N)); f1 = ''.join(str(S[t][1]) for t in range(N))
dig = ''.join(str(S[t][2]) for t in range(N)); load = ''.join(str(S[t][3]) for t in range(N)); xp = ''.join(str(S[t][4]) for t in range(N))
opt2 = ','.join('fffbf9df' if i in M5OFF else 'fffff9df' for i in range(16)); opt = ','.join(['ffffffff'] * 16); half = ','.join(['b'] * 16)
KN = {0: 'K0 all drawn', 5: 'K5 stage program off', 7: 'K7 nothing drawn, main CPU, ME resting', 8: 'K8 nothing drawn, ME turn'}
LN = ['-', 'arithmetic', 'cached reads', 'uncached writes']
XN = {0: 'XP0 (gl39 plan)', 1: 'XP1 ping-pong', 3: 'XP3 both', 7: 'XP7 both + check'}
def digtxt(t):
    lo = 24000 + 100 * t
    if lo + 99 < 29490: return 'none (before 29,490)'
    if lo < 29490: return 'on from 29,490' if not S[t][2] else 'off'
    return 'off' if S[t][2] else 'on'
if __name__ == '__main__':
    m = sys.argv[1] if len(sys.argv) > 1 else 'keys'
    if m == 'keys':
        print('MGS2_OPT_CYCLE=24000:100:' + opt); print('MGS2_OPT2_CYCLE=' + opt2); print('MGS2_HALF_CYCLE=' + half)
        print('MGS2_BOUND_CYCLE=' + bound); print('MGS2_NAT_F1_CYCLE=' + f1); print('MGS2_DIG_CYCLE=' + dig); print('MGS2_LOAD_CYCLE=' + load)
        print('MGS2_XP_CYCLE=' + xp)
    elif m == 'table':
        print('turn | vblanks | kind | XP | digests | ME load | M5 | scene')
        for t in range(N):
            k, f, d, l, x, sc = S[t]
            print('t%-2d | %d-%d | %s | %s | %s | %s | %s | %s' % (t, 24000 + 100 * t, 24100 + 100 * t, KN[k], XN[x] if k in (0, 5) else '-', digtxt(t),
                  LN[l] if k == 8 else '-', 'OFF' if t % 16 in M5OFF else 'on', sc))
    elif m == 'py': print(repr([(t, 24000 + 100 * t, S[t][0], S[t][1], S[t][2], S[t][3], S[t][4], 0 if t % 16 in M5OFF else 1, S[t][5]) for t in range(N)]))
