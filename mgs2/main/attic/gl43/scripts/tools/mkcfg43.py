"""mkcfg43.py OUT [pgo] [KEY=VALUE ...]: gl43's cfg from gl42's delivery cfg (build_gl42/cfg/mgs2_gl42.cfg, 0f77271b): a gl43 header,
the schedule keys replaced by tools/sched43.py's (pgo: its 128-turn training keys and no shots), MGS2_OPT2 without B (ffdff9ff), the new
key MGS2_GL_RAIN_NEAR=1 at the end.  Extra KEY=VALUE arguments replace a key of the same name or are appended as test keys.  -KEY removes a key."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hashlib
src = open('/home/claude/main/build_gl42/cfg/mgs2_gl42.cfg').read()
assert hashlib.md5(src.encode()).hexdigest()[:8] == '0f77271b'
src = src.split('\n')
if src[-1] == '': src.pop()
args = sys.argv[2:]; pgo = bool(args) and args[0] == 'pgo'
if pgo: args = args[1:]
head = '''# gl43 (2026-11-27; a MEASUREMENT build like gl39-gl42: from vb 24,000 the turns switch things on and off, on purpose).  gl42's file
# with these changes, each marked 'gl43' where it stands; no other setting changed:
#  1. Ffm's rain option "B" (2026-11-22 13:03/13:05) on the game side, in its own turns: MGS2_OPT2's bit 21 (2097152, OPT2_B43).  In a B
#     turn the aft deck's floor splashes and falling splashes keep only what the game needs (the random numbers they draw, their time,
#     their slots' lives), and their prims and the ripple rings are not drawn; the far drop sheets, the foot splash and the rain's other
#     effect calls are left out of the picture (MGS2_BOUND_CYCLE 4, BD_RAIN), only the nearest drop-sheet call kept (MGS2_GL_RAIN_NEAR=1).
#     The guards get the same random numbers as on the PS2 (the x86 twin's seed log: README.txt).  MGS2_OPT2 is ffdff9ff (B off) before
#     the turns and in every other turn; the B turns' words are fffff9ff.
#  2. The rain turns t50-t73 (README.txt has the table): K-B (K-A + B, bound 4) between two K-A turns, K-N2B (K-N2 + B, bound 4) between
#     two K-N2 turns, K7B (K7 + B) between two K7 turns; digests in t55 (K-N2, as gl42) and t73 (K-N2B: B's picture against the twin's).
#     The deck turns t0-t49 as gl42.
#  3. New key MGS2_GL_RAIN_NEAR=1 (gl38's key: the last 1 of each picture's 4 drop-sheet calls, the nearest, kept in bound-4 turns).
# The B turns change the game's state hash (the slots' particles and prims), so hashref.txt is gl43's own (the PC twin with these keys).
# gl42's notes follow.'''
K = dict(l.split('=', 1) for l in os.popen('python3 -I %s %s' % (os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sched43.py'), 'pgokeys' if pgo else 'keys')).read().split('\n') if l)
rep = {k: K[k] for k in ('MGS2_OPT_CYCLE', 'MGS2_OPT2_CYCLE', 'MGS2_HALF_CYCLE', 'MGS2_BOUND_CYCLE', 'MGS2_NAT_F1_CYCLE', 'MGS2_DIG_CYCLE', 'MGS2_XP_CYCLE', 'MGS2_NGE_CYCLE')}
rep['MGS2_OPT2'] = 'ffdff9ff'
gone = (('MGS2_GL_SHOT',) if pgo else ())
extra = {}; minus = set()
for a in args:
    if a.startswith('-'): minus.add(a[1:])
    else: k, v = a.split('=', 1); extra[k] = v
out = head.split('\n'); seen = set()
for l in src:
    k = l.split('=', 1)[0] if l.startswith('MGS2_') else None
    if k in minus: out.append('# gl43 test file: taken out: ' + l); seen.add(k)
    elif k in extra: out += ['# gl43 test key (was: %s)' % l, '%s=%s' % (k, extra[k])]; seen.add(k)
    elif k in rep and rep[k] != l.split('=', 1)[1]: out += ["# gl43: gl43's value (see the top; gl42: %s)" % (l.split('=', 1)[1] if len(l) < 60 else l.split('=', 1)[1][:40] + '...'), '%s=%s' % (k, rep[k])]
    elif k in gone: out.append('# gl43: taken out%s: %s' % (' (the PGO training: no shots)' if k == 'MGS2_GL_SHOT' else '', l))
    else: out.append(l)
assert not (minus - seen), minus - seen
def tail(k, v):
    if k in minus: seen.add(k); return ['# gl43 test file: taken out: %s=%s' % (k, v)]
    if k in extra: seen.add(k); return ['# gl43 test key (the delivery file has %s=%s)' % (k, v), '%s=%s' % (k, extra[k])]
    return ['%s=%s' % (k, v)]
out += ["# gl43: the near drop-sheet call kept in the bound-4 (B) turns (see the top)."] + tail('MGS2_GL_RAIN_NEAR', '1')
for k, v in extra.items():
    if k not in seen: out += ['# gl43 test key', '%s=%s' % (k, v)]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 64)')
