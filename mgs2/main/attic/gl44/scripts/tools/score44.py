#!/usr/bin/env python3
"""score44.py [MODEL-DIR] (gl44's scoring [C model], after model44.sh; python3 -I; build_gl43/tools/score43.py's method and sides, with
V44's pairs: K-BV - K-B, K-N2BV - K-N2B, K7BV - K7B in the rain wait and the walk (each V turn against its non-V neighbours: the groups
are B b b B, C c C c C c C, D d d D, so the means cancel a linear drift); gl44 - gl43 on the same turn and kind (the anchors t50 K-A,
t56 K-N2, and K-B t51, K-N2B t57 / t59 / t72, the guard windows).  gl43's notes follow.
  ms a PSP step = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 / 80 (a window = vblanks 10-90 of a turn = 80 steps).
Sides by the build's own symbols (score40.py's): main = game + runtime + the copy plan (ge_xlme) + other; ME = the picture code (ge_* but
ge_fxthin and the plan) with the digest.  From vb 24,000 every turn but K7 is the ME's (the bound cycle), so the PSP's step in a drawn
turn is set by the slower CPU: P = max(main, 0.8 x ME) [I: the ME's code at 0.8 of its emulator-thread cost, gl39's rule], and a lever
on one side shows in the step only while that side is the slower (lead's reading of gl40's PSP log: the rain is main-CPU-bound,
main ~73.4 against ME busy ~69.7).  K7: the ME rests, P = main.  Single differences of whole windows have been wrong by 2x; turn-to-turn
content moves by about 1 ms.  PSPD: the gl42 build whose symbols name the model's addresses (default build_gl42/psp).  gl42: the 'hash' column is
the reuse content hash (n6_csum, n6_same) alone, inside main."""
import sys, os, re, glob, bisect, subprocess
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
R = '/home/claude/main/build_gl44'; d = sys.argv[1] if len(sys.argv) > 1 else R + '/model'; G41 = '/home/claude/main/build_gl43'
PD = os.environ.get('PSPD', R + '/psp')
BUILD = {'44': (PD, os.environ.get('U44', R + '/u')), '43': (os.environ.get('PSPD43', G41 + '/psp'), G41 + '/u')}
_sym = {}
def syms(b):
    if b in _sym: return _sym[b]
    P, O = BUILD[b]; S = []
    for elf in (P + '/MGS2T.elf', P + '/ovl_w00a.elf'):
        for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
            f = l.split()
            if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
    S.sort(); OBJ = {}
    for o in glob.glob(O + '/*.o'):
        for l in subprocess.run([NM, '--defined-only', o], capture_output=True, text=True).stdout.split('\n'):
            f = l.split()
            if len(f) == 3 and f[1] in 'tTwW': OBJ.setdefault(f[2], os.path.basename(o)[:-2])
    _sym[b] = (S, [s[0] for s in S], OBJ); return _sym[b]
GAME = re.compile(r'^(?:S_\w+?_)?P[0-9a-f]{6}(\..*)?$')
def side(b, n):
    if n is None: return 'other'
    if n in ('hw', 'gl_digest', 'gl_digest_c'): return 'dig'
    if n.split('.')[0] in ('n6_csum', 'n6_same'): return 'hash'
    if GAME.match(n): return 'game'
    OBJ = syms(b)[2]; o = OBJ.get(n, OBJ.get(n.split('.')[0], '?'))
    if o == 'ge_xlme': return 'main-pic'
    return 'ME' if o.startswith('ge_') and o != 'ge_fxthin' else 'main-rt'
SIDES = ('game', 'main-rt', 'main-pic', 'hash', 'other', 'ME', 'dig')
_win = {}
def win(b, p):
    if (b, p) in _win: return _win[(b, p)]
    if not os.path.isfile(p): _win[(b, p)] = None; return None
    S, st, _ = syms(b); F = {s: [0.0, 0.0, 0.0] for s in SIDES}; cache = {}
    for ln in open(p):
        if ln[0] == '#': continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else None
        s = cache.get(n)
        if s is None: s = cache[n] = side(b, n)
        x = F[s]; x[0] += int(f[1]) / 80.0; x[1] += int(f[2]) / 80.0; x[2] += int(f[3]) / 80.0
    _win[(b, p)] = F; return F
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def parts(F):
    m = [sum(F[s][j] for s in ('game', 'main-rt', 'main-pic', 'hash', 'other')) for j in range(3)]; e = [F['ME'][j] + F['dig'][j] for j in range(3)]
    return ms(m), ms(e), ms(F['main-pic']), [m[j] + e[j] for j in range(3)]
def head(p):
    if not os.path.isfile(p): return None
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 80.0, int(k['imiss']) / 80.0, int(k['dmiss']) / 80.0]
suf = lambda i: '' if i == 0 else '.%d' % i
W = {
 'ka44': ('44', ['29,010 t50 K-A', '29,110 t51 K-B', '29,210 t52 K-BV', '29,310 t53 K-BV', '29,410 t54 K-B', '29,610 t56 K-N2',
                 '29,710 t57 K-N2B', '29,810 t58 K-N2BV']),
 'kb44': ('44', ['29,910 t59 K-N2B', '30,010 t60 K-N2BV', '30,110 t61 K-N2B', '30,210 t62 K-N2BV', '30,310 t63 K-N2B', '30,410 t64 K7B',
                 '30,510 t65 K7BV', '30,610 t66 K7BV (heavier)']),
 'kw44': ('44', ['30,710 t67 K7B (heavier)', '30,810 t68 K-N2B walk', '30,910 t69 K-N2BV walk', '31,010 t70 K-N2B walk', '31,110 t71 K-N2BV walk',
                 '31,210 t72 K-N2B walk']),
 'gd44': ('44', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
 'ka43': ('43', ['29,010 t50 K-A', '29,110 t51 K-B', '29,210 t52 K-A', '29,310 t53 K-B', '29,410 t54 K-A', '29,610 t56 K-N2',
                 '29,710 t57 K-N2B', '29,810 t58 K-N2']),
 'kb43': ('43', ['29,910 t59 K-N2B', '30,010 t60 K-N2', '30,110 t61 K-A', '30,210 t62 K-B', '30,310 t63 K-A', '30,410 t64 K7',
                 '30,510 t65 K7B', '30,610 t66 K7 (heavier)']),
 'kw43': ('43', ['30,710 t67 K-A (heavier)', '30,810 t68 K-A walk', '30,910 t69 K-B walk', '31,010 t70 K-A walk', '31,110 t71 K-N2 walk',
                 '31,210 t72 K-N2B walk']),
 'gd43': ('43', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
}
PRE = os.environ.get('X', '')   # the tags' prefix (model44.sh X=)
M43 = os.environ.get('M43', G41 + '/model'); PRE43 = os.environ.get('X43', '')   # gl43's model windows (its final build: no prefix)
def P(tag, i):
    if tag.endswith('43'): return os.path.join(M43, PRE43 + tag + suf(i))
    return os.path.join(d, PRE + tag + suf(i))
def pstep(m, e, k7): return m if k7 else max(m, 0.8 * e)
print('== 1. EVERY WINDOW, ms a PSP step [C model]: main (game + runtime + plan + hash + other) | ME (with the digest) | P = max(main, 0.8 ME) [I]'
      ' (K7B, K7BV: main) || main + ME: instr, I-miss, D-miss a step')
for tag in ('ka44', 'kb44', 'kw44', 'gd44'):
    b, names = W[tag]
    for i, n in enumerate(names):
        F = win(b, P(tag, i))
        if F is None: print('  %-8s %-34s NOT RUN' % (tag + suf(i), n)); continue
        m, e, pl, c = parts(F); k7 = 'K7' in n
        print('  %-8s %-34s main %6.2f (game %6.2f, plan %5.2f, hash %5.2f)  ME %6.2f (0.8: %6.2f)  dig %5.2f  P %6.2f %-4s || %9.0f %6.0f %6.0f' % (
            tag + suf(i), n, m, ms(F['game']), pl, ms(F['hash']), e, 0.8 * e, ms(F['dig']), pstep(m, e, k7), '' if k7 else ('main' if m >= 0.8 * e else 'ME'), *c))
def delta(text, A, B):
    FA, FB = win(A[0], P(A[1], A[2])), win(B[0], P(B[1], B[2]))
    if FA is None or FB is None: print('  %-52s NOT RUN (%s / %s)' % (text, A[1] + suf(A[2]), B[1] + suf(B[2]))); return None
    a, b = parts(FA), parts(FB); dm, de = a[0] - b[0], a[1] - b[1]; c = [a[3][q] - b[3][q] for q in range(3)]
    ka = 'K7' in A[3] if len(A) > 3 else False; kb = 'K7' in B[3] if len(B) > 3 else False
    pa, pb = pstep(a[0], a[1], ka), pstep(b[0], b[1], kb)
    dg = ms(FA['game']) - ms(FB['game'])
    print('  %-46s %-15s main %+6.2f (game %+5.2f)  ME %+6.2f (0.8: %+6.2f)  P %6.2f - %6.2f = %+6.2f  [instr %+8.0f I %+6.0f D %+6.0f]' % (
        text, A[1] + suf(A[2]) + ' - ' + B[1] + suf(B[2]), dm, dg, de, 0.8 * de, pa, pb, pa - pb, *c))
    return dm, de, pa - pb, dg
SUM = {}
def grp(name, rows):
    print('== ' + name); out = []
    for t, A, B in rows:
        r = delta(t, A, B)
        if r: out.append(r)
    if out:
        n = len(out); print('   mean of %d: main %+.2f (game %+.2f), ME %+.2f, P %+.2f' % (n, sum(r[0] for r in out) / n, sum(r[3] for r in out) / n, sum(r[1] for r in out) / n, sum(r[2] for r in out) / n))
        SUM[name] = out
a, b, w, g = 'ka44', 'kb44', 'kw44', 'gd44'
grp('2. K-BV (K-B + V44) MINUS K-B, its neighbours (B b b B): V44 on today\'s path', (
    ('rain t52 - t51', ('44', a, 2), ('44', a, 1)), ('rain t53 - t54', ('44', a, 3), ('44', a, 4))))
grp('3. K-N2BV (K-N2B + V44) MINUS K-N2B, its neighbours: V44 on the native path (the delivery\'s kind)', (
    ('rain wait t58 - t57', ('44', a, 7), ('44', a, 6)), ('rain wait t58 - t59', ('44', a, 7), ('44', b, 0)),
    ('rain wait t60 - t59', ('44', b, 1), ('44', b, 0)), ('rain wait t60 - t61', ('44', b, 1), ('44', b, 2)),
    ('rain wait t62 - t61', ('44', b, 3), ('44', b, 2)), ('rain wait t62 - t63', ('44', b, 3), ('44', b, 4)),
    ('walk t69 - t68', ('44', w, 2), ('44', w, 1)), ('walk t69 - t70', ('44', w, 2), ('44', w, 3)),
    ('walk t71 - t70', ('44', w, 4), ('44', w, 3)), ('walk t71 - t72', ('44', w, 4), ('44', w, 5))))
grp('4. K7BV MINUS K7B (nothing drawn: V44\'s logic alone on the main CPU; D d d D)', (
    ('rain wait t65 - t64', ('44', b, 6, 'K7'), ('44', b, 5, 'K7')), ('rain wait t66 - t67', ('44', b, 7, 'K7'), ('44', w, 0, 'K7'))))
grp('5. THE SPREAD: the same kind minus the same kind (no lever: what a single difference moves by)', (
    ('K-B t54 - t51', ('44', a, 4), ('44', a, 1)), ('K-N2B t59 - t57', ('44', b, 0), ('44', a, 6)), ('K-N2B t61 - t59', ('44', b, 2), ('44', b, 0)),
    ('K-N2B t63 - t61', ('44', b, 4), ('44', b, 2)), ('K-N2B walk t70 - t68', ('44', w, 3), ('44', w, 1)), ('K-N2B walk t72 - t70', ('44', w, 5), ('44', w, 3)),
    ('K-N2BV t60 - t58', ('44', b, 1), ('44', a, 7)), ('K7B t67 - t64', ('44', w, 0, 'K7'), ('44', b, 5, 'K7'))))
grp('6. gl44 MINUS gl43, the same turn and kind (no lever: the new code and the new profile/order; the anchors tie gl44 to gl43\'s PSP figures)', (
    ('K-A t50 (anchor)', ('44', a, 0), ('43', 'ka43', 0)), ('K-N2 t56 (anchor)', ('44', a, 5), ('43', 'ka43', 5)),
    ('K-B t51', ('44', a, 1), ('43', 'ka43', 1)), ('K-N2B t57', ('44', a, 6), ('43', 'ka43', 6)), ('K-N2B t59', ('44', b, 0), ('43', 'kb43', 0)),
    ('K-N2B t72 walk', ('44', w, 5), ('43', 'kw43', 5)),
    ('guard 16,110', ('44', g, 0), ('43', 'gd43', 0)), ('guard 21,110', ('44', g, 1), ('43', 'gd43', 1)),
    ('guard 23,110', ('44', g, 2), ('43', 'gd43', 2)), ('guard 27,510', ('44', g, 3), ('43', 'gd43', 3))))
print('== 7. THE GUARD WINDOWS (gl38\'s guard cfg mgs2_m37_rc0, B and V44 off), whole windows: gl44 | gl36 (exp40 x36); I-misses <= gl36 + 2 %?')
X = '/home/claude/main/exp40/model/'
for i, n in enumerate(W['gd44'][1]):
    a_, c_ = head(P('gd44', i)), head(X + 'x36' + suf(i))
    if not (a_ and c_): print('  %-34s NOT RUN' % n); continue
    print('  %-34s gl44 %6.2f gl36 %6.2f | gl44 - gl36 %+5.2f | I a step %6.0f / %6.0f (gl44/gl36 %+5.1f %%: %s) | D %6.0f / %6.0f' % (
        n, ms(a_), ms(c_), ms(a_) - ms(c_), a_[1], c_[1], 100.0 * (a_[1] - c_[1]) / c_[1], 'PASS' if a_[1] <= 1.02 * c_[1] else 'OVER', a_[2], c_[2]))
print('== 8. V44\'s EXPECTED GAIN on the PSP [C model; the PSP\'s rain is main-CPU-bound (gl43\'s log: K-N2B main 69.32, ME 55.25), so main is the figure]:')
for k in SUM:
    if k[:2] in ('2.', '3.', '4.'):
        o = SUM[k]; n = len(o); print('   %-60s n %2d: main %+.2f ms a step (game %+.2f), ME %+.2f, P %+.2f' % (k.split(' MINUS')[0][3:] + ' minus its kind without V', n,
            sum(r[0] for r in o) / n, sum(r[3] for r in o) / n, sum(r[1] for r in o) / n, sum(r[2] for r in o) / n))
