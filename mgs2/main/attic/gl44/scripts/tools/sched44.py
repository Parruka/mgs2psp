"""sched44.py (gl44; attic/gl44/state.md B1, the coordinator's table of 2026-11-27 23:35): the deck t0-t49 as gl42/gl43 (V and B off: their
words ff9ff9ff); from t50 option B (OPT2 bit 21) on in every turn but the two anchors, and V44 (OPT2 bit 22, NAT_V44: line_sph's stub and
rain.c's skipped loops) in alternate turns.  Kinds (each against its base kind):
  A   K-A anchor (everything drawn, today's path; B off, V off)         2   K-N2 anchor (ship + people native; B off, V off)
  B   K-B: K-A + B + bound 4                                             b   K-BV: K-B + V44
  C   K-N2B: K-N2 + B + bound 4 (gl43's best kind, the new baseline)     c   K-N2BV: K-N2B + V44
  D   K7B: K7 + B (nothing drawn)                                        d   K7BV: K7B + V44
  deck kinds as gl42: A, N (K-N1), 2, R (K-N2R), 7 (K7)
Words: B and V off ff9ff9ff; B ffbff9ff; B + V fffff9ff (an arm without V has bit 22 CLEAR, without B bit 21 clear).
Order (drift): the rain wait t50 A (= gl43's t50 K-A, the anchor), t51-t54 K-B / K-BV as B b b B (mirrored, so a linear drift cancels),
t55 K-N2BV with digests (gl43's digest turn), t56 K-N2 (= gl43's t56 K-N2, the anchor), t57-t63 C c C c C c C (every V turn between two
non-V), t64-t67 D d d D (mirrored; also cancels the step into the heavier t66-t67); the walk t68-t72 C c C c C, t73 K-N2BV with digests.
sched44.py keys | pgokeys | table | pairs | count | py"""
import sys
# the deck t0-t49: blocks of ten, each lever turn between two K-A turns: X = A 2 A R A N A 2 A R, Y = the same with K7 last
X, Y = 'A2ARANA2AR', 'A2ARANA2A7'
DECK = X + Y + X + Y + Y
RAIN = 'A' + 'BbbB' + 'c' + '2' + 'CcCcCcC' + 'DddD' + 'CcCcC' + 'c'   # gl44, t50-t73 (see the top)
DIGON = (55, 73)               # whole turns with digests: t55 and t73, both K-N2BV (gl44: V's picture against the twin's)
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
BOUND = {'A': 0, 'N': 0, '2': 0, 'R': 9, '7': 7, 'B': 4, 'C': 4, 'D': 7, 'b': 4, 'c': 4, 'd': 7}
NGE = {'A': 0, 'N': 1, '2': 2, 'R': 2, '7': 0, 'B': 0, 'C': 2, 'D': 0, 'b': 0, 'c': 2, 'd': 0}
OPT2, OPT2B, OPT2BV = 'ff9ff9ff', 'ffbff9ff', 'fffff9ff'   # gl44: bit 21 B, bit 22 V44, each set only where the kind has it
def opt2(k): return OPT2BV if k in 'bcd' else OPT2B if k in 'BCD' else OPT2
KN = {'A': 'K-A all drawn, today', 'N': 'K-N1 ship native', '2': 'K-N2 ship+people native', 'R': 'K-N2R native, bound 9', '7': 'K7 nothing drawn',
      'B': 'K-B today + B, bound 4', 'C': 'K-N2B native + B, bound 4', 'D': 'K7B nothing drawn + B',
      'b': 'K-BV K-B + V44', 'c': 'K-N2BV K-N2B + V44', 'd': 'K7BV K7B + V44'}
BASE = {'B': 'A', 'C': '2', 'D': '7', 'b': 'B', 'c': 'C', 'd': 'D'}   # the kind each is read against (gl44: a V kind against its B kind)
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
        if k in 'N2R7BCDbcd' and t not in DIGON:
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
        print('turn | vblanks | kind | MGS2_OPT2_CYCLE word | B | V44 | MGS2_NGE_CYCLE | MGS2_BOUND_CYCLE | XP | digests | shots | scene')
        for t in range(N):
            lo = 24000 + 100 * t; sh = ' '.join(str(s) for s in SHOTS if lo <= s < lo + 100) or '-'
            print('t%-2d | %d-%d | %s | %s | %s | %s | %d | %d | %d | %s | %s | %s' % (t, lo, lo + 99, KN[K[t]], opt2(K[t]), 'on' if K[t] in 'BCDbcd' else 'off', 'on' if K[t] in 'bcd' else 'off', NGE[K[t]], BOUND[K[t]], xp(t), digtxt(t), sh, scene(t)))
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
            print('%-9s turns: %s; pairs with a neighbour of their base kind (digest turns out): %s' % (part, ', '.join('%s %d' % (k, c.get(k, 0)) for k in 'AN2R7BCDbcd'),
                  ', '.join('%s %d' % (k, cp.get(k, 0)) for k in 'N2R7BCDbcd')))
    elif m == 'py': print(repr([(t, 24000 + 100 * t, K[t], BOUND[K[t]], NGE[K[t]], opt2(K[t]), dig(t), xp(t), scene(t), stretch(t)) for t in range(N)]))
