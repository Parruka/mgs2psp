#!/usr/bin/env python3
"""score40.py [MODEL-DIR] (gl40's scoring [C model], after model40.sh; python3 -I; = build_gl39/tools/score39.py's method).
  ms a PSP step = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 / 80 (a window = vblanks 10-90 of a turn = 80 steps).
Sides by the build's own symbols: game (P<addr>, S_<ovl>_P<addr>), main-pic = the copy plan (ge/xlme.c's functions: xm_plan, xc_*, run by
the MAIN CPU at each recorded transfer's end; gl40 plan section 1: score39.py filed them under ME), main-rt = the runtime and harness,
other = fetches outside the two ELFs' functions, ME = the picture code (ge_* objects but ge_fxthin and the plan), dig = the frame digest's
own functions.  "-> PSP" = main x 1.0 + ME x 0.8 [I] as gl39.  Single differences of whole windows have been wrong by 2x: turn-to-turn
content moves by about 1 ms."""
import sys, os, re, glob, bisect, subprocess
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
R = '/home/claude/main/build_gl40'; d = sys.argv[1] if len(sys.argv) > 1 else R + '/model'; G39 = '/home/claude/main/build_gl39'
PD = os.environ.get('PSPD', R + '/psp')
BUILD = {'40': (PD, R + '/u'), '39': (G39 + '/psp', G39 + '/u')}
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
 'ka40': ('40', ['29,510 t55 K0 XP7 digests on', '29,610 t56 K0 XP0', '29,710 t57 K0 XP3', '29,810 t58 K7 M5 on', '29,910 t59 K0 XP1',
                 '30,010 t60 K0 XP3', '30,110 t61 K0 XP0', '30,410 t64 K7 M5 off']),
 'kb40': ('40', ['30,210 t62 K8 no load', '30,310 t63 K0 XP3', '30,510 t65 K0 XP0 digests on, M5 off', '30,610 t66 K0 XP3 (heavier)',
                 '30,710 t67 K0 XP0 (heavier)', '30,910 t69 walk K0 XP0', '31,010 t70 walk K0 XP3', '31,110 t71 walk K7']),
 'gd40': ('40', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
 'dn40': ('40', ['29,810 no kinds, ME turn', '29,910 no kinds, main-drawing turn']),
 'ka39': ('39', ['29,510 t55 K0 digests on', '29,610 t56 K0 digest-off', '29,710 t57 K7', '29,810 t58 K8 no load',
                 '30,210 t62 K0 digest-off', '30,410 t64 K7 (M5 inert)', '30,910 t69 walk K0 digest-off', '31,010 t70 walk K7']),
}
def P(tag, i):
    return os.path.join(G39 + '/model' if tag.endswith('39') else d, tag + suf(i))
print('== 1. EVERY WINDOW, ms a PSP step [C model]: main (game + runtime + plan + other) | of it the copy plan (main-pic) | ME (with the digest) || main + ME: instr, I-miss, D-miss a step')
for tag in ('ka40', 'kb40', 'gd40', 'dn40', 'ka39'):
    b, names = W[tag]
    for i, n in enumerate(names):
        F = win(b, P(tag, i))
        if F is None: print('  %-7s %-40s NOT RUN' % (tag + suf(i), n)); continue
        m, e, pl, c = parts(F)
        print('  %-7s %-40s main %6.2f (game %6.2f, plan %5.2f)  ME %6.2f  dig %5.2f || %9.0f %6.0f %6.0f' % (tag + suf(i), n, m, ms(F['game']), pl, e, ms(F['dig']), *c))
def delta(text, A, B):
    FA, FB = win(A[0], P(A[1], A[2])), win(B[0], P(B[1], B[2]))
    if FA is None or FB is None: print('  %-58s NOT RUN (%s / %s)' % (text, A[1] + suf(A[2]), B[1] + suf(B[2]))); return None
    a, b = parts(FA), parts(FB); dm, de, dp = a[0] - b[0], a[1] - b[1], a[2] - b[2]; c = [a[3][q] - b[3][q] for q in range(3)]
    print('  %-58s %-15s main %+6.2f (plan %+5.2f)  ME %+6.2f -> PSP %+6.2f  [instr %+8.0f I %+6.0f D %+6.0f]' % (
        text, A[1] + suf(A[2]) + ' - ' + B[1] + suf(B[2]), dm, dp, de, dm + 0.8 * de, *c))
    return dm, de, dp
print('== 2. XP: each line one window MINUS another of the same run, ms a PSP step [C model]; plan section 1e expects XP3 - XP0 = -1.7 to -2.6 model')
print('   (-1.7 to -3.4 PSP), XP1 - XP0 = -0.6 to -0.8 model (-0.6 to -1.0 PSP) [I]')
for t, A, B in (('XP3 - XP0: t57 - t56', ('40', 'ka40', 2), ('40', 'ka40', 1)), ('XP3 - XP0: t60 - t61', ('40', 'ka40', 5), ('40', 'ka40', 6)),
                ('XP3 - XP0: t63 - t61 (two runs)', ('40', 'kb40', 1), ('40', 'ka40', 6)), ('XP3 - XP0: t66 - t67 heavier', ('40', 'kb40', 3), ('40', 'kb40', 4)),
                ('XP3 - XP0: walk t70 - t69', ('40', 'kb40', 6), ('40', 'kb40', 5)), ('XP1 - XP0: t59 - t56', ('40', 'ka40', 4), ('40', 'ka40', 1)),
                ('XP0 - XP0: t61 - t56 (the spread)', ('40', 'ka40', 6), ('40', 'ka40', 1)), ('XP3 - XP3: t60 - t57 (the spread)', ('40', 'ka40', 5), ('40', 'ka40', 2))):
    delta(t, A, B)
print('== 3. M5 (K7, M5 on minus off; plan: ~-0.11 model [C side build])')
delta('M5 on - off: t58 - t64 (K7)', ('40', 'ka40', 3), ('40', 'ka40', 7))
print('== 4. the kinds among themselves')
delta('K0 XP0 t56 - K7 t58: drawing, both CPUs', ('40', 'ka40', 1), ('40', 'ka40', 3))
delta('K8 no load t62 - K7 t58', ('40', 'kb40', 0), ('40', 'ka40', 3))
delta('K0 XP7 digests on t55 - K0 XP0 t56 (digest + check)', ('40', 'ka40', 0), ('40', 'ka40', 1))
delta('walk K0 XP0 t69 - walk K7 t71', ('40', 'kb40', 5), ('40', 'kb40', 7))
print('== 5. gl40 MINUS gl39 (new profile and order, pads, the probe, M5): the same turn and kind where the schedules agree')
delta('K0 digest-off XP0, t56 29,610', ('40', 'ka40', 1), ('39', 'ka39', 1))
delta('K7, t64 30,410 (M5 off in both: gl39 inert)', ('40', 'ka40', 7), ('39', 'ka39', 5))
delta('walk K0 digest-off XP0, t69 30,910', ('40', 'kb40', 5), ('39', 'ka39', 6))
delta('K7 t58 (gl40) - K7 t57 (gl39): other turns', ('40', 'ka40', 3), ('39', 'ka39', 2))
delta('K8 no load t62 (gl40) - t58 (gl39): other turns', ('40', 'kb40', 0), ('39', 'ka39', 3))
delta('K0 XP3 t57 (gl40) - K0 t56 (gl39): XP3 against gl39, neighbour turns', ('40', 'ka40', 2), ('39', 'ka39', 1))
print('== 6. THE GUARD WINDOWS (gl38\'s guard cfg mgs2_m37_rc0), whole windows, ms a step [C model]: gl40 | gl39 (exp40 x39) | gl36 (exp40 x36); I-misses <= gl36 + 2 %?')
X = '/home/claude/main/exp40/model/'
for i, n in enumerate(W['gd40'][1]):
    a, b, c = head(P('gd40', i)), head(X + 'x39' + suf(i)), head(X + 'x36' + suf(i))
    if not (a and b and c): print('  %-34s NOT RUN' % n); continue
    print('  %-34s gl40 %6.2f gl39 %6.2f gl36 %6.2f | gl40 - gl39 %+5.2f, gl40 - gl36 %+5.2f | I a step %6.0f / %6.0f / %6.0f (gl40/gl36 %+5.1f %%: %s) | D %6.0f / %6.0f / %6.0f' % (
        n, ms(a), ms(b), ms(c), ms(a) - ms(b), ms(a) - ms(c), a[1], b[1], c[1], 100.0 * (a[1] - c[1]) / c[1], 'PASS' if a[1] <= 1.02 * c[1] else 'OVER', a[2], b[2], c[2]))
print('== 7. gl38\'s second guard cfg (mgs2_m36_c, no kinds): gl40 | gl39 (dn39) | gl38 (d38) | gl36 (d36), whole windows')
for i, n in enumerate(W['dn40'][1]):
    a, b = head(P('dn40', i)), head(G39 + '/model/dn39' + suf(i))
    c, e = head('/home/claude/main/build_gl38/model/d38' + suf(i)), head('/home/claude/main/build_gl38/model/d36' + suf(i))
    if not (a and b and c and e): print('  %-34s NOT RUN' % n); continue
    print('  %-34s gl40 %6.2f gl39 %6.2f gl38 %6.2f gl36 %6.2f | gl40 - gl39 %+5.2f, - gl36 %+5.2f | I %6.0f / %6.0f / %6.0f / %6.0f | D %6.0f / %6.0f / %6.0f / %6.0f' % (
        n, ms(a), ms(b), ms(c), ms(e), ms(a) - ms(b), ms(a) - ms(e), a[1], b[1], c[1], e[1], a[2], b[2], c[2], e[2]))
