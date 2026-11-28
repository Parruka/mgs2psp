"""sched41.py (gl41; attic/gl41/gl41_plan.md section 3): the turns' schedule.  A turn is 100 vblanks from vb 24,000 (turn t = (vb - 24000) // 100).
Each turn has a kind (one change against K-A in each):
  A   K-A: everything drawn, today's path (MGS2_BOUND_CYCLE 0, MGS2_NGE_CYCLE 0), P1 and P3 on
  N   K-N1: everything drawn, the ship native (step 1: MGS2_NGE_CYCLE 1), P1 and P3 on
  1   K-A with P1 off (MGS2_OPT2 bit 19 clear: fff7f9df)
  3   K-A with P3 off (MGS2_OPT2 bit 20 clear: ffeff9df)
  7   K7: nothing drawn, the main CPU alone (MGS2_BOUND_CYCLE 7), the ME resting
plus
  dig MGS2_DIG_CYCLE (1 = no frame digest; digests exist only in 29,490-31,400): on in t55 (A), t56 (N), t73 (N), and t54's last 10 vblanks
  xp  MGS2_XP_CYCLE: 3 everywhere (gl40's plan, ping-pong + look-back join), 7 (3 + the check) in the digest turns
M5 (bit 18) on everywhere; F1 on everywhere; the re-use off; no ME load turns (gl40's K8 / MGS2_LOAD_CYCLE dropped).
sched41.py keys | table | py"""
import sys
# turn kinds: the deck t0-t49 in blocks of ten (A N A 1 A N A 3 A 7: N, 1 and 3 each between two A turns), the rain from t50
DECK = 'ANA1ANA3A7' * 5
RAIN = {50: 'A', 51: 'N', 52: 'A', 53: '1', 54: 'A', 55: 'A', 56: 'N', 57: '3', 58: 'A', 59: 'N', 60: 'A', 61: '1', 62: 'A', 63: '3',
        64: '7', 65: 'N', 66: 'A', 67: 'N', 68: 'N', 69: 'A', 70: '1', 71: 'A', 72: '3', 73: 'N'}
DIGON = (55, 56, 73)           # whole turns with digests (and t54's last 10 vblanks, 29,490-29,499: MGS2_GL_DIG's start)
N = 74
K = {t: DECK[t] for t in range(50)}; K.update(RAIN); assert sorted(K) == list(range(N))
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
BOUND = {'A': 0, 'N': 0, '1': 0, '3': 0, '7': 7}
NGE = {'A': 0, 'N': 1, '1': 0, '3': 0, '7': 0}
OPT2 = {'A': 'fffff9df', 'N': 'fffff9df', '1': 'fff7f9df', '3': 'ffeff9df', '7': 'fffff9df'}
KN = {'A': 'K-A all drawn, today', 'N': 'K-N1 ship native', '1': 'K-A, P1 off', '3': 'K-A, P3 off', '7': 'K7 nothing drawn'}
def dig(t): return 0 if t in DIGON or t == 54 else 1
def xp(t): return 7 if t in DIGON else 3
def digtxt(t):
    lo = 24000 + 100 * t
    if lo + 99 < 29490: return 'none (before 29,490)'
    if lo < 29490: return 'on from 29,490'
    return 'on' if t in DIGON else 'off'
# the pairs to read (same scene, one change): (lever turn, its K-A neighbours)
def pairs():
    out = []
    for t in range(N):
        k = K[t]
        if k in 'N13' and t not in DIGON:
            nb = [u for u in (t - 1, t + 1) if 0 <= u < N and K[u] == 'A' and u not in DIGON]
            out.append((t, k, nb))
    return out
if __name__ == '__main__':
    m = sys.argv[1] if len(sys.argv) > 1 else 'keys'
    if m == 'keys':
        print('MGS2_OPT_CYCLE=24000:100:' + ','.join(['ffffffff'] * N))
        print('MGS2_OPT2_CYCLE=' + ','.join(OPT2[K[t]] for t in range(N)))
        print('MGS2_HALF_CYCLE=' + ','.join(['b'] * N))
        print('MGS2_BOUND_CYCLE=' + ''.join(str(BOUND[K[t]]) for t in range(N)))
        print('MGS2_NGE_CYCLE=' + ''.join(str(NGE[K[t]]) for t in range(N)))
        print('MGS2_NAT_F1_CYCLE=' + '1' * N)
        print('MGS2_DIG_CYCLE=' + ''.join(str(dig(t)) for t in range(N)))
        print('MGS2_XP_CYCLE=' + ''.join(str(xp(t)) for t in range(N)))
    elif m == 'table':
        print('turn | vblanks | kind | MGS2_OPT2 | XP | digests | scene')
        for t in range(N):
            print('t%-2d | %d-%d | %s | %s | %d | %s | %s' % (t, 24000 + 100 * t, 24099 + 100 * t, KN[K[t]], OPT2[K[t]], xp(t), digtxt(t), scene(t)))
    elif m == 'pairs':
        for t, k, nb in pairs(): print('t%-2d %-16s %-22s next to K-A %s' % (t, KN[k], scene(t), ' '.join('t%d' % u for u in nb) or '-'))
    elif m == 'count':
        for part, rng in (('deck t0-t49', range(50)), ('rain t50-t73', range(50, 74))):
            c = {}
            for t in rng:
                if t in DIGON: continue
                c[K[t]] = c.get(K[t], 0) + 1
            print(part, ', '.join('%s %d' % (KN[k], c.get(k, 0)) for k in 'AN137'), '(digest turns not counted)')
    elif m == 'py': print(repr([(t, 24000 + 100 * t, K[t], BOUND[K[t]], NGE[K[t]], OPT2[K[t]], dig(t), xp(t), scene(t)) for t in range(N)]))
