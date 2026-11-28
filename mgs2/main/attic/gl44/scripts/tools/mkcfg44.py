"""mkcfg44.py OUT [pgo] [KEY=VALUE ...]: gl44's cfg from gl43's delivery cfg (build_gl43/cfg/mgs2_gl43.cfg, 533aa271): a gl44 header,
the schedule keys replaced by tools/sched44.py's (pgo: its 128-turn training keys and no shots), MGS2_OPT2 without B and V (ff9ff9ff),
the new key MGS2_BBOX=1 after MGS2_BAT_REC.  Extra KEY=VALUE arguments replace a key of the same name or are appended as test keys.
-KEY removes a key."""
import sys, os, hashlib
here = os.path.dirname(os.path.abspath(__file__))
src = open('/home/claude/main/build_gl43/cfg/mgs2_gl43.cfg').read()
assert hashlib.md5(src.encode()).hexdigest()[:8] == '533aa271'
src = src.split('\n')
if src[-1] == '': src.pop()
args = sys.argv[2:]; pgo = bool(args) and args[0] == 'pgo'
if pgo: args = args[1:]
head = '''# gl44 (2026-11-27; a MEASUREMENT build like gl39-gl43: from vb 24,000 the turns switch things on and off, on purpose).  gl43's file
# with these changes, each marked 'gl44' where it stands; no other setting changed:
#  1. Option B (MGS2_OPT2 bit 21, gl43) is now ON in every rain turn from t50 but the two anchors (t50 K-A, t56 K-N2).
#  2. New in its own turns: V44, MGS2_OPT2's bit 22 (4194304, OPT2_V44): line_sph's drops (already hidden) are kept only as the random
#     numbers they draw and their time, and rain.c's hidden streaks skip their vertex loops; nothing on screen changes and the guards
#     get the same random numbers (the x86 twin's seed log: README.txt).  Words: ff9ff9ff (B and V off: before the turns, the deck
#     t0-t49 and the anchors), ffbff9ff (B), fffff9ff (B + V).  NOTE: gl43's words (ffdff9ff, fffff9ff) have bit 22 set: in gl44
#     ffdff9ff would mean V44 ON.
#  3. The rain turns t50-t73 (README.txt has the table): K-B / K-BV, K-N2B / K-N2BV, K7B / K7BV side by side; digests in t55 and t73
#     (both K-N2BV: V's picture against the twin's).  The deck turns t0-t49 as gl43.
#  4. New key MGS2_BBOX=1: the black box (psp/bbox.c): bbox.txt, made at its full size at the start (3,600 records of 128 bytes,
#     461 KB), gets one text record a second from a thread above the game's priority (README.txt says what is in it).  Off without the
#     key (the code's default).  Not a key: at every start the harness renames the runs before's log.txt, battery.txt, crash.txt,
#     steps.csv and bbox.txt to *_prev1 (and the _prev1 files to _prev2), finished or not.
# The B and V turns change the game's state hash (hidden particles and vertex buffers), so hashref.txt is gl44's own (the PC twin with
# these keys); to vb 29,200 (the end of t51) it is gl43's line for line.
# gl43's notes follow.'''
K = dict(l.split('=', 1) for l in os.popen('python3 -I %s %s' % (os.path.join(here, 'sched44.py'), 'pgokeys' if pgo else 'keys')).read().split('\n') if l)
rep = {k: K[k] for k in ('MGS2_OPT_CYCLE', 'MGS2_OPT2_CYCLE', 'MGS2_HALF_CYCLE', 'MGS2_BOUND_CYCLE', 'MGS2_NAT_F1_CYCLE', 'MGS2_DIG_CYCLE', 'MGS2_XP_CYCLE', 'MGS2_NGE_CYCLE')}
rep['MGS2_OPT2'] = 'ff9ff9ff'
gone = (('MGS2_GL_SHOT',) if pgo else ())
extra = {}; minus = set()
for a in args:
    if a.startswith('-'): minus.add(a[1:])
    else: k, v = a.split('=', 1); extra[k] = v
out = head.split('\n'); seen = set(); bb = False
for l in src:
    k = l.split('=', 1)[0] if l.startswith('MGS2_') else None
    if k in minus: out.append('# gl44 test file: taken out: ' + l); seen.add(k)
    elif k in extra: out += ['# gl44 test key (was: %s)' % l, '%s=%s' % (k, extra[k])]; seen.add(k)
    elif k in rep and rep[k] != l.split('=', 1)[1]: out += ["# gl44: gl44's value (see the top; gl43: %s)" % (l.split('=', 1)[1] if len(l) < 60 else l.split('=', 1)[1][:40] + '...'), '%s=%s' % (k, rep[k])]
    elif k in gone: out.append('# gl44: taken out%s: %s' % (' (the PGO training: no shots)' if k == 'MGS2_GL_SHOT' else '', l))
    else: out.append(l)
    if k == 'MGS2_BAT_REC':
        bb = True
        if 'MGS2_BBOX' in minus: out.append('# gl44 test file: taken out: MGS2_BBOX=1'); seen.add('MGS2_BBOX')
        elif 'MGS2_BBOX' in extra: out += ['# gl44 test key (the delivery file has MGS2_BBOX=1)', 'MGS2_BBOX=%s' % extra['MGS2_BBOX']]; seen.add('MGS2_BBOX')
        else: out += ['# gl44: the black box, a 128-byte record a second into bbox.txt (see the top).', 'MGS2_BBOX=1']
assert bb and not (minus - seen), minus - seen
for k, v in extra.items():
    if k not in seen: out += ['# gl44 test key', '%s=%s' % (k, v)]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
for l in out:   # the trap: every OPT2 word's bits 21 / 22 as the table says
    if l.startswith('MGS2_OPT2'):
        for w in l.split('=', 1)[1].split(','): assert w in ('ff9ff9ff', 'ffbff9ff', 'fffff9ff'), (l[:40], w)
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 64)')
