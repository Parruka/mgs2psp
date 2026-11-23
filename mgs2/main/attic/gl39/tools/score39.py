#!/usr/bin/env python3
"""score39.py [MODEL-DIR] (gl39's scoring [C model], after model39.sh; python3 -I).  The port's cost model:
  ms a PSP step = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 / 80 (a window = vblanks 10-90 of a turn = 80 steps).
Every window is split by SIDE with the build's own symbols: game = the recompiled game (P<addr>, S_<ovl>_P<addr>), main-rt = the
runtime and harness, ME = the picture code (ge_* objects but ge_fxthin: it runs on the ME in an ME turn), dig = the frame digest's
own functions (hw, gl_digest, gl_digest_c: a check's cost on the ME, also counted in ME; gl38's build had gl_digest inside its caller,
so only hw + gl_digest together compare), other = fetches outside the two ELFs' functions.  (The waits do not spin in the emulator:
a K7 window is the main CPU's work, a K0 window both processors' work added.)  A lever's figure is a DIFFERENCE of two windows; its
expected PSP size is main (game + main-rt + other) x 1.0 + ME x 0.8 (gl38's PSP run: K7 70.3 model against 69.6 PSP; ME work 0.77-0.88).
Single differences of 'all' windows have been wrong by 2x here: turn-to-turn content moves by about 1 ms."""
import sys, os, re, glob, bisect, subprocess, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
R = '/home/claude/main/build_gl39'; d = sys.argv[1] if len(sys.argv) > 1 else R + '/model'; G38 = '/home/claude/main/build_gl38'
BUILD = {'39': (R + '/psp', R + '/u'), '38': (G38 + '/psp', G38 + '/psp')}   # (ELFs, objects)
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
    return 'ME' if o.startswith('ge_') and o != 'ge_fxthin' else 'main-rt'
SIDES = ('game', 'main-rt', 'other', 'ME', 'dig')
_win = {}
def win(b, p):   # side -> [instr, I-miss, D-miss] a step; None when the file is not there
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
def parts(F):   # main (game + main-rt + other), ME (with the digest), the digest alone: ms a step; and the three counts of main + ME
    m = [sum(F[s][j] for s in ('game', 'main-rt', 'other')) for j in range(3)]; e = [F['ME'][j] + F['dig'][j] for j in range(3)]
    return ms(m), ms(e), ms(F['dig']), [m[j] + e[j] for j in range(3)]
def head(p):
    if not os.path.isfile(p): return None
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 80.0, int(k['imiss']) / 80.0, int(k['dmiss']) / 80.0]
suf = lambda i: '' if i == 0 else '.%d' % i
W = {  # tag -> (build, [(index, text)])
 'ka39': ('39', ['29,510 t55 rain wait K0 digests on', '29,610 t56 rain wait K0 digest-off', '29,710 t57 rain wait K7', '29,810 t58 rain wait K8 no load',
                 '30,210 t62 rain wait K0 digest-off', '30,410 t64 rain wait K7 (bit 18 clear)', '30,910 t69 walk K0 digest-off', '31,010 t70 walk K7']),
 'km39': ('39', ['29,610 t56 K0 digest-off, bit 18 clear', '29,710 t57 K7, bit 18 clear', '30,210 t62 K0 digest-off, bit 18 clear', '31,010 t70 walk K7, bit 18 clear']),
 'kf39': ('39', ['29,610 t56 K0 digest-off, F1 off', '30,210 t62 K0 digest-off, F1 off', '30,710 t67 K7 digests on (gl38\'s t67)', '30,910 t69 walk K0 digest-off, F1 off']),
 'kd39': ('39', ['29,610 t56 K0 digests ON', '30,210 t62 K0 digests ON', '30,910 t69 walk K0 digests ON']),
 'dn39': ('39', ['29,810 no kinds, ME turn', '29,910 no kinds, main-drawing turn']),
 'k38y': ('38', ['29,610 t56 K0 no digests, F1 off (gl38, gl39\'s kinds)', '29,710 t57 K7 (gl38, gl39\'s kinds)', '30,210 t62 K0 no digests, F1 off (gl38)',
                 '30,410 t64 K7 (gl38)', '30,710 t67 K7 no digests (gl38)', '30,910 t69 walk K0 no digests, F1 off (gl38)', '31,010 t70 walk K7 (gl38)']),
 'k38x': ('38', ['29,810 t58 rain wait K0 (gl38)', '30,010 t60 rain wait K0 (gl38)', '30,510 t65 rain wait K7 (gl38)', '30,710 t67 rain wait K7 (gl38)',
                 '30,810 t68 walk K0 (gl38)', '30,910 t69 walk K7 (gl38)']),
}
def P(tag, i, base=None): return os.path.join(base or d, tag + suf(i))
print('== 1. THE EMULATOR CHECK: gl38\'s final build run again now (k38x, PPSSPPHeadless_fcr) against gl38\'s own model runs of the same turns')
for i, (t, j) in ((0, ('k38a', 7)), (1, ('k38b', 1)), (2, ('k38b', 6)), (4, ('k38c', 3)), (5, ('k38c', 4))):
    a, b = head(P('k38x', i)), head(P(t, j, G38 + '/model'))
    if a is None or b is None: print('  %-34s NOT RUN (%s)' % (W['k38x'][1][i], 'k38x' + suf(i) if a is None else t + suf(j))); continue
    print('  %-34s now %7.2f, then (%s) %7.2f ms a step: %+5.2f  (instr %+6.2f %%, I-miss %+6.2f %%, D-miss %+6.2f %%)' % (
        W['k38x'][1][i], ms(a), t + suf(j), ms(b), ms(a) - ms(b), *[100.0 * (a[q] - b[q]) / b[q] for q in range(3)]))
print('== 2. EVERY WINDOW, ms a PSP step [C model]: main (game + runtime + other) | ME (the picture code, with the digest) | of it the digest || main + ME: instr, I-miss, D-miss a step')
for tag in ('ka39', 'km39', 'kf39', 'kd39', 'dn39', 'k38x', 'k38y'):
    b, names = W[tag]
    for i, n in enumerate(names):
        F = win(b, P(tag, i))
        if F is None: print('  %-5s %-46s NOT RUN' % (tag + suf(i), n)); continue
        m, e, w, c = parts(F)
        print('  %-7s %-46s main %6.2f (game %6.2f)  ME %6.2f  dig %6.2f || %9.0f %6.0f %6.0f' % (tag + suf(i), n, m, ms(F['game']), e, w, *c))
def delta(text, A, B):   # A minus B: ((build, tag, i), (build, tag, i))
    FA, FB = win(A[0], P(A[1], A[2], A[3] if len(A) > 3 else None)), win(B[0], P(B[1], B[2], B[3] if len(B) > 3 else None))
    if FA is None or FB is None: print('  %-62s NOT RUN (%s / %s)' % (text, A[1] + suf(A[2]), B[1] + suf(B[2]))); return None
    a, b = parts(FA), parts(FB); dm, de, dw = a[0] - b[0], a[1] - b[1], a[2] - b[2]; c = [a[3][q] - b[3][q] for q in range(3)]
    print('  %-62s %-17s main %+6.2f  ME %+6.2f  (dig %+6.2f) -> PSP %+6.2f  [instr %+8.0f I %+6.0f D %+6.0f]' % (
        text, A[1] + suf(A[2]) + ' - ' + B[1] + suf(B[2]), dm, de, dw, dm + 0.8 * de, *c))
    return dm, de
print('== 3. THE LEVERS: each line is one window MINUS another, ms a PSP step [C model]; "-> PSP" = main x 1.0 + ME x 0.8 [I]')
print(' -- M5 IS NOT IN THE PSP BUILD (its block is in the twin\'s kernel file only): these lines are TWO RUNS OF THE SAME CODE = the model\'s run-to-run spread')
for t, (i, j) in (('29,610 K0 digest-off', (0, 1)), ('29,710 K7', (1, 2)), ('30,210 K0 digest-off', (2, 4)), ('31,010 walk K7', (3, 7))):
    delta('bit 18 clear - set, ' + t + ' (two runs, the same turn)', ('39', 'km39', i), ('39', 'ka39', j))
delta('K7 30,410 (t64) - 29,710 (t57) (one run, two turns)', ('39', 'ka39', 5), ('39', 'ka39', 2))
print(' -- F1 (ME: the VU writes a later group overwrites left out), OFF minus on: what F1 saves')
for t, (i, j) in (('29,610 K0 digest-off', (0, 1)), ('30,210 K0 digest-off', (1, 4)), ('30,910 walk K0 digest-off', (3, 6))):
    delta('F1 off - on, ' + t, ('39', 'kf39', i), ('39', 'ka39', j))
print(' -- the frame digest, ON minus off: what a digest-off turn leaves out')
for t, (i, j) in (('29,610 K0', (0, 1)), ('30,210 K0', (1, 4)), ('30,910 walk K0', (2, 6))):
    delta('digests on - off, ' + t + ' (two runs, the same turn)', ('39', 'kd39', i), ('39', 'ka39', j))
delta('digests on - off, 29,510 (t55) - 29,610 (t56) (one run)', ('39', 'ka39', 0), ('39', 'ka39', 1))
print(' -- the kinds of the delivery cfg among themselves (one run)')
delta('K0 digest-off 29,610 - K7 29,710: drawing, both CPUs', ('39', 'ka39', 1), ('39', 'ka39', 2))
delta('K8 no load 29,810 - K7 29,710: the ME\'s turn with nothing drawn', ('39', 'ka39', 3), ('39', 'ka39', 2))
delta('K0 digest-off 30,210 - 29,610: two turns of one kind (the spread)', ('39', 'ka39', 4), ('39', 'ka39', 1))
delta('walk K0 digest-off 30,910 - walk K7 31,010', ('39', 'ka39', 6), ('39', 'ka39', 7))
print(' -- gl39 MINUS gl38 (code placement M7, the hot data block and pads, the probes): negative = gl39 is cheaper; "other turns" lines are NOT comparisons of builds')
delta('K7 30,710 (t67: the same turn, digests on)', ('39', 'kf39', 2), ('38', 'k38x', 3))
delta('K7 29,710 (t57) - gl38\'s K7 30,510 (t65): other turns', ('39', 'ka39', 2), ('38', 'k38x', 2))
delta('K7 30,410 (t64) - gl38\'s K7 30,510 (t65): other turns', ('39', 'ka39', 5), ('38', 'k38x', 2))
delta('walk K7 31,010 (t70) - gl38\'s walk K7 30,910 (t69): other turns', ('39', 'ka39', 7), ('38', 'k38x', 5))
delta('K0 digests on, F1 on 29,610 (kd39) - gl38\'s K0 29,810 (t58)', ('39', 'kd39', 0), ('38', 'k38x', 0))
delta('K0 digests on, F1 on 30,210 (kd39) - gl38\'s K0 30,010 (t60)', ('39', 'kd39', 1), ('38', 'k38x', 1))
delta('K0 digests on 29,510 (t55, F1 on) - gl38\'s K0 29,810 (t58)', ('39', 'ka39', 0), ('38', 'k38x', 0))
delta('walk K0 digests on 30,910 (kd39) - gl38\'s walk K0 30,810 (t68)', ('39', 'kd39', 2), ('38', 'k38x', 4))
print(' -- gl39 MINUS gl38, THE SAME TURN AND KIND (k38y: gl38\'s final build with gl39\'s kinds at the same vblanks, no digests, re-use and F1 off)')
delta('K0 digest-off, F1 off, 29,610 (t56)', ('39', 'kf39', 0), ('38', 'k38y', 0))
delta('K0 digest-off, F1 off, 30,210 (t62)', ('39', 'kf39', 1), ('38', 'k38y', 2))
delta('walk K0 digest-off, F1 off, 30,910 (t69)', ('39', 'kf39', 3), ('38', 'k38y', 5))
delta('K0 digest-off, F1 ON in gl39 only, 29,610 (t56)', ('39', 'ka39', 1), ('38', 'k38y', 0))
delta('K0 digest-off, F1 ON in gl39 only, 30,210 (t62)', ('39', 'ka39', 4), ('38', 'k38y', 2))
delta('walk K0 digest-off, F1 ON in gl39 only, 30,910 (t69)', ('39', 'ka39', 6), ('38', 'k38y', 5))
delta('K7 29,710 (t57)', ('39', 'ka39', 2), ('38', 'k38y', 1))
delta('K7 29,710 (t57), bit 18 clear (km39)', ('39', 'km39', 1), ('38', 'k38y', 1))
delta('K7 30,410 (t64), bit 18 clear', ('39', 'ka39', 5), ('38', 'k38y', 3))
delta('K7 30,710 (t67) (gl39 with digests on)', ('39', 'kf39', 2), ('38', 'k38y', 4))
delta('walk K7 31,010 (t70)', ('39', 'ka39', 7), ('38', 'k38y', 6))
delta('walk K7 31,010 (t70), bit 18 clear (km39)', ('39', 'km39', 3), ('38', 'k38y', 6))
print(' -- THE RETRAIN\'S GUARD (gl38\'s guard cfg, no kinds): gl39 MINUS gl38 and MINUS gl36, the same cfg and windows')
for i, t in ((0, 'ME turn 29,810'), (1, 'main-drawing turn 29,910')):
    delta('gl39 - gl38 (d38), ' + t, ('39', 'dn39', i), ('38', 'd38', i, G38 + '/model'))
    a, b, c = head(P('dn39', i)), head(P('d38', i, G38 + '/model')), head(P('d36', i, G38 + '/model'))
    if a and b and c: print('     whole windows, ms a step: gl39 %7.2f, gl38 %7.2f, gl36 %7.2f: gl39 - gl38 %+5.2f, gl39 - gl36 %+5.2f (D-misses a step %0.f / %0.f / %0.f)' % (
        ms(a), ms(b), ms(c), ms(a) - ms(b), ms(a) - ms(c), a[2], b[2], c[2]))
