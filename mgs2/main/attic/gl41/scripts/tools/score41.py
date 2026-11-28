#!/usr/bin/env python3
"""score41.py [MODEL-DIR] (gl41's scoring [C model], after model41.sh; python3 -I; = build_gl40/tools/score40.py's method).
  ms a PSP step = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 / 80 (a window = vblanks 10-90 of a turn = 80 steps).
Sides by the build's own symbols (score40.py's): main = game + runtime + the copy plan (ge_xlme) + other; ME = the picture code (ge_* but
ge_fxthin and the plan) with the digest.  From vb 24,000 every turn but K7 is the ME's (the bound cycle), so the PSP's step in a drawn
turn is set by the slower CPU: P = max(main, 0.8 x ME) [I: the ME's code at 0.8 of its emulator-thread cost, gl39's rule], and a lever
on one side shows in the step only while that side is the slower (lead's reading of gl40's PSP log: the rain is main-CPU-bound,
main ~73.4 against ME busy ~69.7).  K7: the ME rests, P = main.  Single differences of whole windows have been wrong by 2x; turn-to-turn
content moves by about 1 ms.  PSPD: the gl41 build whose symbols name the model's addresses (default build_gl41/psp)."""
import sys, os, re, glob, bisect, subprocess
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
R = '/home/claude/main/build_gl41'; d = sys.argv[1] if len(sys.argv) > 1 else R + '/model'; G40 = '/home/claude/main/build_gl40'
PD = os.environ.get('PSPD', R + '/psp')
BUILD = {'41': (PD, R + '/u'), '40': (G40 + '/psp', G40 + '/u')}
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
    if GAME.match(n): return 'game'
    OBJ = syms(b)[2]; o = OBJ.get(n, OBJ.get(n.split('.')[0], '?'))
    if o == 'ge_xlme': return 'main-pic'
    return 'ME' if o.startswith('ge_') and o != 'ge_fxthin' else 'main-rt'
SIDES = ('game', 'main-rt', 'main-pic', 'other', 'ME', 'dig')
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
    m = [sum(F[s][j] for s in ('game', 'main-rt', 'main-pic', 'other')) for j in range(3)]; e = [F['ME'][j] + F['dig'][j] for j in range(3)]
    return ms(m), ms(e), ms(F['main-pic']), [m[j] + e[j] for j in range(3)]
def head(p):
    if not os.path.isfile(p): return None
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 80.0, int(k['imiss']) / 80.0, int(k['dmiss']) / 80.0]
suf = lambda i: '' if i == 0 else '.%d' % i
W = {
 'ka41': ('41', ['29,710 t57 P3 off', '29,810 t58 K-A', '29,910 t59 K-N1', '30,010 t60 K-A', '30,110 t61 P1 off', '30,210 t62 K-A',
                 '30,310 t63 P3 off', '30,410 t64 K7']),
 'kb41': ('41', ['30,510 t65 K-N1', '30,610 t66 K-A (heavier)', '30,710 t67 K-N1 (heavier)', '30,810 t68 K-N1 walk', '30,910 t69 K-A walk',
                 '31,010 t70 P1 off walk', '31,110 t71 K-A walk', '31,210 t72 P3 off walk']),
 'kc41': ('41', ['27,010 t30 K-A deck', '27,110 t31 K-N1 deck', '27,210 t32 K-A deck', '27,310 t33 P1 off deck', '27,410 t34 K-A deck',
                 '27,510 t35 K-N1 deck', '27,710 t37 P3 off deck', '27,910 t39 K7 deck']),
 'kd41': ('41', ['29,110 t51 K-N1 first rain', '29,210 t52 K-A first rain', '29,310 t53 P1 off first rain', '29,410 t54 K-A first rain',
                 '29,510 t55 K-A digests XP7', '29,610 t56 K-N1 digests XP7', '31,310 t73 K-N1 digests XP7']),
 'gd41': ('41', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
 'ka40': ('40', ['29,510 t55 K0 XP7 digests on', '29,610 t56 K0 XP0', '29,710 t57 K0 XP3', '29,810 t58 K7 M5 on', '29,910 t59 K0 XP1',
                 '30,010 t60 K0 XP3', '30,110 t61 K0 XP0', '30,410 t64 K7 M5 off']),
 'kb40': ('40', ['30,210 t62 K8 no load', '30,310 t63 K0 XP3', '30,510 t65 K0 XP0 digests on, M5 off', '30,610 t66 K0 XP3 (heavier)',
                 '30,710 t67 K0 XP0 (heavier)', '30,910 t69 walk K0 XP0', '31,010 t70 walk K0 XP3', '31,110 t71 walk K7']),
}
PRE = os.environ.get('X', '')   # the tags' prefix (model41.sh X=)
def P(tag, i):
    # gl40's final build = its pad candidate pf byte for byte (build_gl40 state B8): its rain windows and guard windows are pfka40 and
    # pfgd40; kb40 was run on the final build itself
    if tag.endswith('40'): return os.path.join(G40 + '/model', {'ka40': 'pfka40', 'gd40': 'pfgd40'}.get(tag, tag) + suf(i))
    return os.path.join(d, PRE + tag + suf(i))
def pstep(m, e, k7): return m if k7 else max(m, 0.8 * e)
print('== 1. EVERY WINDOW, ms a PSP step [C model]: main (game + runtime + plan + other) | ME (with the digest) | P = max(main, 0.8 ME) [I]'
      ' (K7: main) || main + ME: instr, I-miss, D-miss a step')
for tag in ('ka41', 'kb41', 'kc41', 'kd41', 'gd41'):
    b, names = W[tag]
    for i, n in enumerate(names):
        F = win(b, P(tag, i))
        if F is None: print('  %-8s %-34s NOT RUN' % (tag + suf(i), n)); continue
        m, e, pl, c = parts(F); k7 = 'K7' in n
        print('  %-8s %-34s main %6.2f (game %6.2f, plan %5.2f)  ME %6.2f (0.8: %6.2f)  dig %5.2f  P %6.2f %-4s || %9.0f %6.0f %6.0f' % (
            tag + suf(i), n, m, ms(F['game']), pl, e, 0.8 * e, ms(F['dig']), pstep(m, e, k7), '' if k7 else ('main' if m >= 0.8 * e else 'ME'), *c))
def delta(text, A, B):
    FA, FB = win(A[0], P(A[1], A[2])), win(B[0], P(B[1], B[2]))
    if FA is None or FB is None: print('  %-52s NOT RUN (%s / %s)' % (text, A[1] + suf(A[2]), B[1] + suf(B[2]))); return None
    a, b = parts(FA), parts(FB); dm, de = a[0] - b[0], a[1] - b[1]; c = [a[3][q] - b[3][q] for q in range(3)]
    ka, kb = 'K7' in text.split(':')[0] and 'K7' in text, False
    pa, pb = max(a[0], 0.8 * a[1]), max(b[0], 0.8 * b[1])
    print('  %-52s %-16s main %+6.2f  ME %+6.2f (0.8: %+6.2f)  P %6.2f - %6.2f = %+6.2f  [instr %+8.0f I %+6.0f D %+6.0f]' % (
        text, A[1] + suf(A[2]) + ' - ' + B[1] + suf(B[2]), dm, de, 0.8 * de, pa, pb, pa - pb, *c))
    return dm, de
print('== 2. K-N1 (the ship native, step 1) MINUS K-A, same scene: main and ME apart; P = max(main, 0.8 ME) of each turn [I]')
for t, A, B in (('rain wait t59 - t58', ('41', 'ka41', 2), ('41', 'ka41', 1)), ('rain wait t59 - t60', ('41', 'ka41', 2), ('41', 'ka41', 3)),
                ('heavier t65 - t66', ('41', 'kb41', 0), ('41', 'kb41', 1)), ('heavier t67 - t66', ('41', 'kb41', 2), ('41', 'kb41', 1)),
                ('walk t68 - t69', ('41', 'kb41', 3), ('41', 'kb41', 4)), ('first rain t51 - t52', ('41', 'kd41', 0), ('41', 'kd41', 1)),
                ('deck t31 - t30', ('41', 'kc41', 1), ('41', 'kc41', 0)), ('deck t31 - t32', ('41', 'kc41', 1), ('41', 'kc41', 2)),
                ('deck t35 - t34', ('41', 'kc41', 5), ('41', 'kc41', 4)), ('digests XP7 t56 - t55', ('41', 'kd41', 5), ('41', 'kd41', 4))):
    delta('K-N1 - K-A: ' + t, A, B)
print('== 3. P1 OFF MINUS ON (K-A), same scene')
for t, A, B in (('rain t61 - t60', ('41', 'ka41', 4), ('41', 'ka41', 3)), ('rain t61 - t62', ('41', 'ka41', 4), ('41', 'ka41', 5)),
                ('walk t70 - t69', ('41', 'kb41', 5), ('41', 'kb41', 4)), ('walk t70 - t71', ('41', 'kb41', 5), ('41', 'kb41', 6)),
                ('first rain t53 - t52', ('41', 'kd41', 2), ('41', 'kd41', 1)), ('first rain t53 - t54', ('41', 'kd41', 2), ('41', 'kd41', 3)),
                ('deck t33 - t32', ('41', 'kc41', 3), ('41', 'kc41', 2)), ('deck t33 - t34', ('41', 'kc41', 3), ('41', 'kc41', 4))):
    delta('P1 off - on: ' + t, A, B)
print('== 4. P3 OFF MINUS ON (K-A), same scene')
for t, A, B in (('rain t57 - t58', ('41', 'ka41', 0), ('41', 'ka41', 1)), ('rain t63 - t62', ('41', 'ka41', 6), ('41', 'ka41', 5)),
                ('walk t72 - t71', ('41', 'kb41', 7), ('41', 'kb41', 6)), ('deck t37 - t34 (not neighbours)', ('41', 'kc41', 6), ('41', 'kc41', 4))):
    delta('P3 off - on: ' + t, A, B)
print('== 5. the kinds among themselves; the spread of K-A against K-A')
for t, A, B in (('K-A t62 - K7 t64 (drawing)', ('41', 'ka41', 5), ('41', 'ka41', 7)), ('K-A t34 - K7 t39 (deck)', ('41', 'kc41', 4), ('41', 'kc41', 7)),
                ('K-A - K-A: t60 - t58', ('41', 'ka41', 3), ('41', 'ka41', 1)), ('K-A - K-A: t62 - t60', ('41', 'ka41', 5), ('41', 'ka41', 3)),
                ('K-A - K-A: t71 - t69 (walk)', ('41', 'kb41', 6), ('41', 'kb41', 4)), ('K-A - K-A: t32 - t30 (deck)', ('41', 'kc41', 2), ('41', 'kc41', 0))):
    delta(t, A, B)
print('== 6. gl41 MINUS gl40, the same turn, both all drawn with XP3 (gl41 K-A: P1 + P3 on; new profile, order, pads, heap fix)')
for t, A, B in (('t60 30,010 K-A / K0 XP3', ('41', 'ka41', 3), ('40', 'ka40', 5)), ('t66 30,610 K-A / K0 XP3 heavier', ('41', 'kb41', 1), ('40', 'kb40', 3)),
                ('K7: gl41 t64 (M5 on) - gl40 t58 (M5 on)', ('41', 'ka41', 7), ('40', 'ka40', 3))):
    delta(t, A, B)
print('== 7. THE GUARD WINDOWS (gl38\'s guard cfg mgs2_m37_rc0), whole windows: gl41 | gl40 (build_gl40 model gd40) | gl36 (exp40 x36);'
      ' I-misses <= gl36 + 2 %?')
X = '/home/claude/main/exp40/model/'
for i, n in enumerate(W['gd41'][1]):
    a, b, c = head(P('gd41', i)), head(G40 + '/model/pfgd40' + suf(i)), head(X + 'x36' + suf(i))
    if not (a and c): print('  %-34s NOT RUN' % n); continue
    bs = ms(b) if b else float('nan')
    print('  %-34s gl41 %6.2f gl40 %6.2f gl36 %6.2f | gl41 - gl40 %+5.2f, gl41 - gl36 %+5.2f | I a step %6.0f / %6.0f / %6.0f (gl41/gl36 %+5.1f %%: %s) | D %6.0f / %6.0f / %6.0f' % (
        n, ms(a), bs, ms(c), ms(a) - bs, ms(a) - ms(c), a[1], b[1] if b else float('nan'), c[1], 100.0 * (a[1] - c[1]) / c[1],
        'PASS' if a[1] <= 1.02 * c[1] else 'OVER', a[2], b[2] if b else float('nan'), c[2]))
