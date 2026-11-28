#!/usr/bin/env python3
"""score43.py [MODEL-DIR] (gl43's scoring [C model], after model43.sh; python3 -I; build_gl42/tools/score42.py's method and sides,
with option B's pairs: K-B - K-A, K-N2B - K-N2, K7B - K7 in the rain wait and the walk; gl43 - gl42 on the same K-A walk turns).
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
R = '/home/claude/main/build_gl43'; d = sys.argv[1] if len(sys.argv) > 1 else R + '/model'; G41 = '/home/claude/main/build_gl42'
PD = os.environ.get('PSPD', R + '/psp')
BUILD = {'43': (PD, R + '/u'), '42': (os.environ.get('PSPD42', G41 + '/psp'), G41 + '/u')}
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
 'ka43': ('43', ['29,010 t50 K-A', '29,110 t51 K-B', '29,210 t52 K-A', '29,310 t53 K-B', '29,410 t54 K-A', '29,610 t56 K-N2',
                 '29,710 t57 K-N2B', '29,810 t58 K-N2']),
 'kb43': ('43', ['29,910 t59 K-N2B', '30,010 t60 K-N2', '30,110 t61 K-A', '30,210 t62 K-B', '30,310 t63 K-A', '30,410 t64 K7',
                 '30,510 t65 K7B', '30,610 t66 K7 (heavier)']),
 'kw43': ('43', ['30,710 t67 K-A (heavier)', '30,810 t68 K-A walk', '30,910 t69 K-B walk', '31,010 t70 K-A walk', '31,110 t71 K-N2 walk',
                 '31,210 t72 K-N2B walk']),
 'gd43': ('43', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
 'kb42': ('42', ['30,510 t65 K-N2', '30,610 t66 K-A (heavier)', '30,710 t67 K-N2R (heavier)', '30,810 t68 K-A walk', '30,910 t69 K-N2 walk',
                 '31,010 t70 K-A walk', '31,110 t71 K-N2R walk', '31,210 t72 K-A walk']),
 'gd42': ('42', ['16,110 guard cfg main-drawing', '21,110 guard cfg main-drawing', '23,110 guard cfg main-drawing', '27,510 guard cfg main-drawing']),
}
PRE = os.environ.get('X', '')   # the tags' prefix (model42.sh X=)
M41 = os.environ.get('M42', G41 + '/model'); PRE41 = os.environ.get('X42', '')   # gl42's model windows (its final build: no prefix)
def P(tag, i):
    if tag.endswith('42'): return os.path.join(M41, PRE41 + tag + suf(i))
    return os.path.join(d, PRE + tag + suf(i))
def pstep(m, e, k7): return m if k7 else max(m, 0.8 * e)
print('== 1. EVERY WINDOW, ms a PSP step [C model]: main (game + runtime + plan + hash + other) | ME (with the digest) | P = max(main, 0.8 ME) [I]'
      ' (K7, K7B: main) || main + ME: instr, I-miss, D-miss a step')
for tag in ('ka43', 'kb43', 'kw43', 'gd43'):
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
grp('2. K-B (K-A + B, bound 4) MINUS K-A, its neighbours: option B on today\'s path', (
    ('rain t51 - t52', ('43', 'ka43', 1), ('43', 'ka43', 2)),
    ('rain t53 - t52', ('43', 'ka43', 3), ('43', 'ka43', 2)), ('rain t53 - t54', ('43', 'ka43', 3), ('43', 'ka43', 4)),
    ('rain wait t62 - t61', ('43', 'kb43', 3), ('43', 'kb43', 2)), ('rain wait t62 - t63', ('43', 'kb43', 3), ('43', 'kb43', 4)),
    ('walk t69 - t68', ('43', 'kw43', 2), ('43', 'kw43', 1)), ('walk t69 - t70', ('43', 'kw43', 2), ('43', 'kw43', 3))))
print('   (not in the mean: t51 - t50, the rain only beginning in t50 (main 51.6 there, 83-90 in t51-t54): content)'); delta('K-B - K-A: rain begins t51 - t50 (content)', ('43', 'ka43', 1), ('43', 'ka43', 0))
grp('3. K-N2B (K-N2 + B, bound 4) MINUS K-N2, its neighbours: option B on the native path (the default\'s)', (
    ('rain wait t57 - t56', ('43', 'ka43', 6), ('43', 'ka43', 5)), ('rain wait t57 - t58', ('43', 'ka43', 6), ('43', 'ka43', 7)),
    ('rain wait t59 - t58', ('43', 'kb43', 0), ('43', 'ka43', 7)), ('rain wait t59 - t60', ('43', 'kb43', 0), ('43', 'kb43', 1)),
    ('walk t72 - t71', ('43', 'kw43', 5), ('43', 'kw43', 4))))
grp('4. K7B MINUS K7 (nothing drawn: option B\'s logic alone on the main CPU)', (
    ('rain wait t65 - t64', ('43', 'kb43', 6, 'K7'), ('43', 'kb43', 5, 'K7')), ('rain wait t65 - t66', ('43', 'kb43', 6, 'K7'), ('43', 'kb43', 7, 'K7'))))
grp('5. THE SPREAD: K-A MINUS K-A, K-N2 MINUS K-N2 (no lever: what a single difference moves by)', (
    ('K-A t54 - t52', ('43', 'ka43', 4), ('43', 'ka43', 2)),
    ('K-A t63 - t61', ('43', 'kb43', 4), ('43', 'kb43', 2)), ('K-A t70 - t68 (walk)', ('43', 'kw43', 3), ('43', 'kw43', 1)),
    ('K-N2 t58 - t56', ('43', 'ka43', 7), ('43', 'ka43', 5)), ('K-N2 t60 - t58', ('43', 'kb43', 1), ('43', 'ka43', 7))))
grp('6. gl43 MINUS gl42, the same turn and kind (no lever: the new code and the new profile/order)', (
    ('K-A t68 walk', ('43', 'kw43', 1), ('42', 'kb42', 3)), ('K-A t70 walk', ('43', 'kw43', 3), ('42', 'kb42', 5)),
    ('guard 16,110', ('43', 'gd43', 0), ('42', 'gd42', 0)), ('guard 21,110', ('43', 'gd43', 1), ('42', 'gd42', 1)),
    ('guard 23,110', ('43', 'gd43', 2), ('42', 'gd42', 2)), ('guard 27,510', ('43', 'gd43', 3), ('42', 'gd42', 3))))
print('== 7. THE GUARD WINDOWS (gl38\'s guard cfg mgs2_m37_rc0), whole windows: gl43 | gl36 (exp40 x36); I-misses <= gl36 + 2 %?')
X = '/home/claude/main/exp40/model/'
for i, n in enumerate(W['gd43'][1]):
    a, c = head(P('gd43', i)), head(X + 'x36' + suf(i))
    if not (a and c): print('  %-34s NOT RUN' % n); continue
    print('  %-34s gl43 %6.2f gl36 %6.2f | gl43 - gl36 %+5.2f | I a step %6.0f / %6.0f (gl43/gl36 %+5.1f %%: %s) | D %6.0f / %6.0f' % (
        n, ms(a), ms(c), ms(a) - ms(c), a[1], c[1], 100.0 * (a[1] - c[1]) / c[1], 'PASS' if a[1] <= 1.02 * c[1] else 'OVER', a[2], c[2]))
