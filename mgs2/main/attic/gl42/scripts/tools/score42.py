#!/usr/bin/env python3
"""score42.py [MODEL-DIR] (gl42's scoring [C model], after model42.sh; python3 -I; = build_gl41/tools/score41.py's method).
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
R = '/home/claude/main/build_gl42'; d = sys.argv[1] if len(sys.argv) > 1 else R + '/model'; G41 = '/home/claude/main/build_gl41'
PD = os.environ.get('PSPD', R + '/psp')
BUILD = {'42': (PD, R + '/u'), '41': (os.environ.get('PSPD41', G41 + '/psp'), G41 + '/u')}
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
 'ka42': ('42', ['29,710 t57 K-N1', '29,810 t58 K-A', '29,910 t59 K-N2', '30,010 t60 K-A', '30,110 t61 K-N2R', '30,210 t62 K-A',
                 '30,310 t63 K7', '30,410 t64 K-A']),
 'kb42': ('42', ['30,510 t65 K-N2', '30,610 t66 K-A (heavier)', '30,710 t67 K-N2R (heavier)', '30,810 t68 K-A walk', '30,910 t69 K-N2 walk',
                 '31,010 t70 K-A walk', '31,110 t71 K-N2R walk', '31,210 t72 K-A walk']),
 'kc42': ('42', ['27,010 t30 K-A deck', '27,110 t31 K-N2 deck', '27,210 t32 K-A deck', '27,310 t33 K-N2R deck', '27,410 t34 K-A deck',
                 '27,510 t35 K-N1 deck', '27,610 t36 K-A deck', '27,910 t39 K7 deck']),
 'gd42': ('42', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
 'ka41': ('41', ['29,710 t57 P3 off', '29,810 t58 K-A', '29,910 t59 K-N1', '30,010 t60 K-A', '30,110 t61 P1 off', '30,210 t62 K-A',
                 '30,310 t63 P3 off', '30,410 t64 K7']),
 'kb41': ('41', ['30,510 t65 K-N1', '30,610 t66 K-A (heavier)', '30,710 t67 K-N1 (heavier)', '30,810 t68 K-N1 walk', '30,910 t69 K-A walk',
                 '31,010 t70 P1-off walk', '31,110 t71 K-A walk', '31,210 t72 P3 off walk']),
}
PRE = os.environ.get('X', '')   # the tags' prefix (model42.sh X=)
M41 = os.environ.get('M41', G41 + '/model'); PRE41 = os.environ.get('X41', '')   # gl41's model windows (its final build's prefix)
def P(tag, i):
    if tag.endswith('41'): return os.path.join(M41, PRE41 + tag + suf(i))
    return os.path.join(d, PRE + tag + suf(i))
def pstep(m, e, k7): return m if k7 else max(m, 0.8 * e)
print('== 1. EVERY WINDOW, ms a PSP step [C model]: main (game + runtime + plan + hash + other) | ME (with the digest) | P = max(main, 0.8 ME) [I]'
      ' (K7: main) || main + ME: instr, I-miss, D-miss a step')
for tag in ('ka42', 'kb42', 'kc42', 'gd42'):
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
    print('  %-52s %-16s main %+6.2f (hash %+5.2f)  ME %+6.2f (0.8: %+6.2f)  P %6.2f - %6.2f = %+6.2f  [instr %+8.0f I %+6.0f D %+6.0f]' % (
        text, A[1] + suf(A[2]) + ' - ' + B[1] + suf(B[2]), dm, ms(FA['hash']) - ms(FB['hash']), de, 0.8 * de, pa, pb, pa - pb, *c))
    return dm, de
print('== 2. K-N2 (ship + people native) MINUS K-A, same scene: main and ME apart; P = max(main, 0.8 ME) of each turn [I]')
for t, A, B in (('rain wait t59 - t58', ('42', 'ka42', 2), ('42', 'ka42', 1)), ('rain wait t59 - t60', ('42', 'ka42', 2), ('42', 'ka42', 3)),
                ('heavier t65 - t66', ('42', 'kb42', 0), ('42', 'kb42', 1)), ('walk t69 - t68', ('42', 'kb42', 4), ('42', 'kb42', 3)),
                ('walk t69 - t70', ('42', 'kb42', 4), ('42', 'kb42', 5)), ('deck t31 - t30', ('42', 'kc42', 1), ('42', 'kc42', 0)),
                ('deck t31 - t32', ('42', 'kc42', 1), ('42', 'kc42', 2))):
    delta('K-N2 - K-A: ' + t, A, B)
print('== 3. K-N2R (native, chains 2-4 only) MINUS K-A, same scene')
for t, A, B in (('rain wait t61 - t60', ('42', 'ka42', 4), ('42', 'ka42', 3)), ('rain wait t61 - t62', ('42', 'ka42', 4), ('42', 'ka42', 5)),
                ('heavier t67 - t66', ('42', 'kb42', 2), ('42', 'kb42', 1)), ('walk t71 - t70', ('42', 'kb42', 6), ('42', 'kb42', 5)),
                ('walk t71 - t72', ('42', 'kb42', 6), ('42', 'kb42', 7)), ('deck t33 - t32', ('42', 'kc42', 3), ('42', 'kc42', 2)),
                ('deck t33 - t34', ('42', 'kc42', 3), ('42', 'kc42', 4))):
    delta('K-N2R - K-A: ' + t, A, B)
print('== 4. K-N1 (ship native) MINUS K-A; K-N2 MINUS K-N1 (the people alone; not neighbours)')
for t, A, B in (('K-N1 - K-A: rain wait t57 - t58', ('42', 'ka42', 0), ('42', 'ka42', 1)), ('K-N1 - K-A: deck t35 - t34', ('42', 'kc42', 5), ('42', 'kc42', 4)),
                ('K-N1 - K-A: deck t35 - t36', ('42', 'kc42', 5), ('42', 'kc42', 6)), ('K-N2 - K-N1: rain wait t59 - t57', ('42', 'ka42', 2), ('42', 'ka42', 0)),
                ('K-N2 - K-N1: deck t31 - t35', ('42', 'kc42', 1), ('42', 'kc42', 5))):
    delta(t, A, B)
print('== 5. K-A MINUS K7 (the drawing); the spread of K-A against K-A')
for t, A, B in (('K-A t62 - K7 t63 (rain wait)', ('42', 'ka42', 5), ('42', 'ka42', 6, 'K7')), ('K-A t64 - K7 t63 (rain wait)', ('42', 'ka42', 7), ('42', 'ka42', 6, 'K7')),
                ('K-A t36 - K7 t39 (deck, not neighbours)', ('42', 'kc42', 6), ('42', 'kc42', 7, 'K7')),
                ('K-A - K-A: t60 - t58', ('42', 'ka42', 3), ('42', 'ka42', 1)), ('K-A - K-A: t62 - t60', ('42', 'ka42', 5), ('42', 'ka42', 3)),
                ('K-A - K-A: t70 - t68 (walk)', ('42', 'kb42', 5), ('42', 'kb42', 3)), ('K-A - K-A: t32 - t30 (deck)', ('42', 'kc42', 2), ('42', 'kc42', 0))):
    delta(t, A, B)
print('== 6. gl42 MINUS gl41, the same turn (gl42 C2 on, step 2 code, new profile/order; gl41\'s model windows if they exist)')
for t, A, B in (('t60 30,010 K-A / K-A', ('42', 'ka42', 3), ('41', 'ka41', 3)), ('t58 29,810 K-A / K-A', ('42', 'ka42', 1), ('41', 'ka41', 1)),
                ('t66 30,610 K-A / K-A heavier', ('42', 'kb42', 1), ('41', 'kb41', 1)), ('K7: gl42 t63 - gl41 t64', ('42', 'ka42', 6, 'K7'), ('41', 'ka41', 7, 'K7')),
                ('K-N2 t59 / K-N1 t59 (gl41)', ('42', 'ka42', 2), ('41', 'ka41', 2))):
    delta(t, A, B)
print('== 7. THE GUARD WINDOWS (gl38\'s guard cfg mgs2_m37_rc0), whole windows: gl42 | gl36 (exp40 x36); I-misses <= gl36 + 2 %?')
X = '/home/claude/main/exp40/model/'
for i, n in enumerate(W['gd42'][1]):
    a, c = head(P('gd42', i)), head(X + 'x36' + suf(i))
    if not (a and c): print('  %-34s NOT RUN' % n); continue
    print('  %-34s gl42 %6.2f gl36 %6.2f | gl42 - gl36 %+5.2f | I a step %6.0f / %6.0f (gl42/gl36 %+5.1f %%: %s) | D %6.0f / %6.0f' % (
        n, ms(a), ms(c), ms(a) - ms(c), a[1], c[1], 100.0 * (a[1] - c[1]) / c[1], 'PASS' if a[1] <= 1.02 * c[1] else 'OVER', a[2], c[2]))
