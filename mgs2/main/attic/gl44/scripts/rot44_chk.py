# rot44_chk.py <emu_rot> (gl44 H1's checks on rot44.sh's three starts): each generation's file is the start before's, byte for byte.
import sys, os, hashlib
E = sys.argv[1]; bad = []; ok = 0
def md5(p): return hashlib.md5(open(p, 'rb').read()).hexdigest() if os.path.isfile(p) else None
def eq(what, a, b):
    global ok
    ma, mb = md5(a), md5(b)
    if ma is None and mb is None: print('  %-44s both absent: ok' % what); ok += 1; return
    if ma == mb: print('  %-44s %s = %s: ok' % (what, ma[:8], os.path.relpath(b, E))); ok += 1
    else: print('  %-44s %s vs %s: DIFFER' % (what, ma and ma[:8], mb and mb[:8])); bad.append(what)
N = [('log', '.txt'), ('battery', '.txt'), ('crash', '.txt'), ('steps', '.csv'), ('bbox', '.txt')]
S = lambda k, f: os.path.join(E, 's%d' % k, f)
print('rot44_chk: start 2 (after a log WITH its end line): _prev1 = start 1\'s file, no _prev2')
for n, x in N:
    eq('s2 %s_prev1%s = s1 %s%s' % (n, x, n, x), S(2, n + '_prev1' + x), S(1, n + x)); eq('s2 %s_prev2%s absent' % (n, x), S(2, n + '_prev2' + x), '/nonexistent')
print('rot44_chk: start 3 (after a log WITHOUT its end line): _prev1 = start 2\'s file, _prev2 = start 1\'s')
for n, x in N:
    eq('s3 %s_prev1%s = s2 %s%s' % (n, x, n, x), S(3, n + '_prev1' + x), S(2, n + x)); eq('s3 %s_prev2%s = s1 %s%s' % (n, x, n, x), S(3, n + '_prev2' + x), S(1, n + x))
def has_end(p): return any(l.startswith(b'end: ') for l in open(p, 'rb').read().split(b'\n'))
for k, want in ((1, True), (2, False), (3, True)):
    e = has_end(S(k, 'log.txt'))
    print('  start %d log.txt end line: %s (%s)' % (k, e, 'ok' if e == want else 'WRONG')); (bad.append('end%d' % k) if e != want else None)
for k, want in ((2, 'has its end line'), (3, 'has NO end line')):
    t = open(os.path.join(E, 'h1_%d.txt' % k)).read()
    w = want in t; print('  start %d H1 line says the run before\'s log %s: %s' % (k, want, 'ok' if w else 'WRONG')); (bad.append('h1_%d' % k) if not w else None)
for k in (1, 2, 3):
    p = S(k, 'bbox.txt'); sz = os.path.getsize(p) if os.path.isfile(p) else -1
    print('  start %d bbox.txt %d bytes (%s)' % (k, sz, 'ok' if sz == 460800 else 'WRONG')); (bad.append('bbsize%d' % k) if sz != 460800 else None)
sb, sa = open(os.path.join(E, 'shot_before.md5')).read(), open(os.path.join(E, 'shot_after.md5')).read()
print('  the stand-in shot_1.bmp: %s after three starts (%s)' % ('unchanged, not rotated' if sb == sa else 'CHANGED', 'ok' if sb == sa else 'WRONG'))
if sb != sa: bad.append('shot')
print('rot44_chk: %s (%d checks ok, %d wrong%s)' % ('PASS' if not bad else 'FAIL', ok, len(bad), ': ' + ', '.join(bad) if bad else ''))
sys.exit(1 if bad else 0)
