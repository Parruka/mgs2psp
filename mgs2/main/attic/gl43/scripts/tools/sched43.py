"""sched43.py (gl43; attic/gl43/gl43_plan.md): sched42.py with option B's kinds in the rain turns (the deck t0-t49 as gl42).
sched42.py (gl42; attic/gl42/gl42_plan.md section 3): the turns' schedule.  A turn is 100 vblanks from vb 24,000 (turn t = (vb - 24000) // 100).
C2 is on in every turn and before (the lead, 2026-11-24: DEVIATIONS.md B7), P1, P3 and M5 on (MGS2_OPT2 fffff9ff), F1 on, XP 3.
Each turn has a kind (one change against K-A in each):
  A   K-A: everything drawn, today's path (MGS2_NGE_CYCLE 0, MGS2_BOUND_CYCLE 0)
  N   K-N1: everything drawn, the ship native (step 1: MGS2_NGE_CYCLE 1)
  2   K-N2: everything drawn, the ship and the people native (step 2: MGS2_NGE_CYCLE 2; the people from vb 24,100 on)
  R   K-N2R: K-N2 with the picture's bound kind 9 (MGS2_BOUND_CYCLE 9, BD_NATIVE: chains 2-4 drawn, the ME still converting the native
      groups): the main CPU's share with the native picture
  7   K7: nothing drawn, the main CPU alone (MGS2_BOUND_CYCLE 7), the ME resting
gl43, option B (OPT2_B43 2097152 in the turn's MGS2_OPT2_CYCLE word: fffff9ff; every other word ffdff9ff):
  B   K-B: K-A + B (the floor and falling splashes' logic kept to the seed, the time and their lives; their prims and ripple's hidden)
      + bound 4 (BD_RAIN: the rain's effect calls left out but the nearest drop-sheet call, MGS2_GL_RAIN_NEAR=1)
  C   K-N2B: K-N2 + B + bound 4
  D   K7B: K7 + B (nothing drawn: the main CPU's share of B alone)
plus
  dig MGS2_DIG_CYCLE (1 = no frame digest; digests exist only in 29,490-31,400): on in t55 (K-N2, rain wait) and t73 (K-N2, the walk's end)
  xp  MGS2_XP_CYCLE: 3 everywhere, 7 (3 + the check) in the digest turns
sched42.py keys | pgokeys | table | pairs | count | py"""
import sys
# the deck t0-t49: blocks of ten, each lever turn between two K-A turns: X = A 2 A R A N A 2 A R, Y = the same with K7 last
X, Y = 'A2ARANA2AR', 'A2ARANA2A7'
DECK = X + Y + X + Y + Y
RAIN = 'ABABA2' + '2C2C2' + 'ABA' + '7D7' + 'A' + 'ABA2CC'   # gl43, t50-t73: rain begins, the wait t51-t67, the walk t68-t73: K-B between two
                                                            # K-A, K-N2B between two K-N2, K7B between two K7; t73 K-N2B with digests
DIGON = (55, 73)               # whole turns with digests: t55 K-N2 (as gl42), t73 K-N2B (gl43: B's picture against the twin's)
N = 74
K = {t: DECK[t] for t in range(50)}; K.update({50 + i: c for i, c in enumerate(RAIN)}); assert sorted(K) == list(range(N))
# the PGO training's turns past t73 (to vb 37,800): every kind, the new code in the counts
PGOX = 'A2ARAN'
def scene(t):
    if t == 0 or t == 10 or t in (20, 21): return 'deck, change of scene'
    if 1 <= t <= 9: return 'first deck scene'
    if 11 <= t <= 19: return 'deck (first play)'
    if 22 <= t <= 46: return 'steady deck'
    if 47 <= t <= 49: return 'change of scene'
    if t == 50: return 'rain begins'
    if 51 <= t <= 54: return 'rain wait, first turns'
    if 55 <= t <= 65: return 'rain wait'
    if t in (66, 67): return 'rain wait (heavier)'
    if 68 <= t <= 72: return 'rain walk'
    return 'the walk ends'
def stretch(t): return 'deck' if t < 50 else 'rain wait' if t < 68 else 'walk'
BOUND = {'A': 0, 'N': 0, '2': 0, 'R': 9, '7': 7, 'B': 4, 'C': 4, 'D': 7}
NGE = {'A': 0, 'N': 1, '2': 2, 'R': 2, '7': 0, 'B': 0, 'C': 2, 'D': 0}
OPT2, OPT2B = 'ffdff9ff', 'fffff9ff'   # gl43: B (bit 21) only in the B kinds' turns
def opt2(k): return OPT2B if k in 'BCD' else OPT2
KN = {'A': 'K-A all drawn, today', 'N': 'K-N1 ship native', '2': 'K-N2 ship+people native', 'R': 'K-N2R native, bound 9', '7': 'K7 nothing drawn',
      'B': 'K-B today + B, bound 4', 'C': 'K-N2B native + B, bound 4', 'D': 'K7B nothing drawn + B'}
BASE = {'B': 'A', 'C': '2', 'D': '7'}   # gl43: the kind a B kind is read against
def dig(t): return 0 if t in DIGON else 1
def xp(t): return 7 if t in DIGON else 3
def digtxt(t):
    lo = 24000 + 100 * t
    if lo + 99 < 29490: return 'none (before 29,490)'
    return 'on' if t in DIGON else 'off'
SHOTS = (8762, 10518, 15763, 23265, 24246, 24786, 29502, 30502, 30548, 30646, 30724, 30826, 30926, 31048, 31114, 31264)
# the pairs to read (one change against K-A): (lever turn, kind, its K-A neighbours), digest turns left out
def pairs():
    out = []
    for t in range(N):
        k = K[t]
        if k in 'N2R7BCD' and t not in DIGON:
            nb = [u for u in (t - 1, t + 1) if 0 <= u < N and K[u] == BASE.get(k, 'A') and u not in DIGON]
            out.append((t, k, nb))
    return out
def keys(n=N):
    ks = [K[t] if t < N else PGOX[(t - N) % len(PGOX)] for t in range(n)]
    return ['MGS2_OPT_CYCLE=24000:100:' + ','.join(['ffffffff'] * N),
            'MGS2_OPT2_CYCLE=' + ','.join(opt2(k) for k in ks),
            'MGS2_HALF_CYCLE=' + ','.join(['b'] * N),
            'MGS2_BOUND_CYCLE=' + ''.join(str(BOUND[k]) for k in ks),
            'MGS2_NGE_CYCLE=' + ''.join(str(NGE[k]) for k in ks),
            'MGS2_NAT_F1_CYCLE=' + '1' * N,
            'MGS2_DIG_CYCLE=' + ''.join(str(dig(t)) for t in range(N)),
            'MGS2_XP_CYCLE=' + ''.join(str(xp(t)) for t in range(N))]
if __name__ == '__main__':
    m = sys.argv[1] if len(sys.argv) > 1 else 'keys'
    if m == 'keys': print('\n'.join(keys()))
    elif m == 'pgokeys': print('\n'.join(keys(128)))
    elif m == 'table':
        print('turn | vblanks | kind | B | MGS2_NGE_CYCLE | MGS2_BOUND_CYCLE | XP | digests | shots | scene')
        for t in range(N):
            lo = 24000 + 100 * t; sh = ' '.join(str(s) for s in SHOTS if lo <= s < lo + 100) or '-'
            print('t%-2d | %d-%d | %s | %s | %d | %d | %d | %s | %s | %s' % (t, lo, lo + 99, KN[K[t]], 'on' if K[t] in 'BCD' else 'off', NGE[K[t]], BOUND[K[t]], xp(t), digtxt(t), sh, scene(t)))
    elif m == 'pairs':
        for t, k, nb in pairs(): print('t%-2d %-26s %-24s next to %s %s' % (t, KN[k], scene(t), KN[BASE.get(k, 'A')].split(' ')[0], ' '.join('t%d' % u for u in nb) or '-'))
    elif m == 'count':
        for part in ('deck', 'rain wait', 'walk'):
            c = {}; cp = {}
            for t in range(N):
                if stretch(t) != part: continue
                c[K[t]] = c.get(K[t], 0) + 1
            for t, k, nb in pairs():
                if stretch(t) == part and nb: cp[k] = cp.get(k, 0) + 1
            print('%-9s turns: %s; pairs with a neighbour of their base kind (digest turns out): %s' % (part, ', '.join('%s %d' % (k, c.get(k, 0)) for k in 'AN2R7BCD'),
                  ', '.join('%s %d' % (k, cp.get(k, 0)) for k in 'N2R7BCD')))
    elif m == 'py': print(repr([(t, 24000 + 100 * t, K[t], BOUND[K[t]], NGE[K[t]], opt2(K[t]), dig(t), xp(t), scene(t), stretch(t)) for t in range(N)]))
