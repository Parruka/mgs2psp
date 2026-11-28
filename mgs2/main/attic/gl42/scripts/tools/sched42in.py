"""sched42in.py (gl42in, 2026-11-27: gl42's EBOOT, the INDOOR measurement): gl42's schedule t0-t73 unchanged (tools/sched42.py), then
t74-t127 (vb 31,400-36,800): t74-t77 K-A (the way to the door and the stage change), t78-t127 five blocks of ten as on the deck
(X = A 2 A R A N A 2 A R, Y = the same with K7 last; X Y X Y X).  The EBOOT's cycles hold 128 turns (BD_CYC_N), so t128-t137
(vb 36,800-37,800) run with the defaults past the lists: kind 'D' = everything drawn on today's path with F1 off and XP 0 (not a K-A turn).
F1 on and XP 3 in every turn of the 128; digests (and XP 7, the check) in t55, t73 as gl42 and in two indoor K-N2 turns, t89 (vb 32,900)
and t119 (vb 35,900): the cfg's MGS2_GL_DIG is 29490:36800.
sched42in.py keys | table | py"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched42 as s
X, Y = 'A2ARANA2AR', 'A2ARANA2A7'
N = 138
NC = 128   # the cycles' length in the EBOOT
K = dict(s.K); K.update({74 + i: 'A' for i in range(4)}); K.update({78 + i: c for i, c in enumerate(X + Y + X + Y + X)}); K.update({t: 'D' for t in range(NC, N)}); assert sorted(K) == list(range(N))
DIGON = (55, 73, 89, 119)
BOUND = dict(s.BOUND, D=0); NGE = dict(s.NGE, D=0); KN = dict(s.KN, D='default past the cycle (F1 off, XP 0)')
def scene(t): return s.scene(t) if t < 74 else 'to the door' if t < 78 else 'indoor' if t < NC else 'indoor, past the cycle'
def stretch(t): return s.stretch(t) if t < 74 else 'to the door' if t < 78 else 'indoor' if t < NC else 'indoor, past the cycle'
def dig(t): return 0 if t in DIGON or t >= NC else 1
def xp(t): return 0 if t >= NC else 7 if t in DIGON else 3
def f1(t): return 0 if t >= NC else 1
def keys():
    return ['MGS2_OPT_CYCLE=24000:100:' + ','.join(['ffffffff'] * 74),
            'MGS2_OPT2_CYCLE=' + ','.join([s.OPT2] * 74),
            'MGS2_HALF_CYCLE=' + ','.join(['b'] * 74),
            'MGS2_BOUND_CYCLE=' + ''.join(str(BOUND[K[t]]) for t in range(NC)),
            'MGS2_NGE_CYCLE=' + ''.join(str(NGE[K[t]]) for t in range(NC)),
            'MGS2_NAT_F1_CYCLE=' + '1' * NC,
            'MGS2_DIG_CYCLE=' + ''.join(str(dig(t)) for t in range(NC)),
            'MGS2_XP_CYCLE=' + ''.join(str(xp(t)) for t in range(NC))]
if __name__ == '__main__':
    m = sys.argv[1] if len(sys.argv) > 1 else 'keys'
    if m == 'keys': print('\n'.join(keys()))
    elif m == 'table':
        for t in range(74, N): print('t%-3d | %d-%d | %s | NGE %d | bound %d | XP %d | digests %s | %s' % (t, 24000 + 100 * t, 24099 + 100 * t, KN[K[t]], NGE[K[t]], BOUND[K[t]], xp(t), 'on' if t in DIGON else 'off', scene(t)))
    elif m == 'py': print(repr([(t, 24000 + 100 * t, K[t], NGE[K[t]], BOUND[K[t]], dig(t), xp(t), scene(t), stretch(t), f1(t)) for t in range(N)]))
