"""mkcfg42.py OUT [pgo] [KEY=VALUE ...]: gl42's cfg from gl41's delivery cfg (build_gl41/cfg/mgs2_gl41.cfg): a gl42 header, the schedule keys
replaced by tools/sched42.py's (pgo: its 128-turn training keys and no shots), MGS2_OPT2 with C2 (fffff9ff), gl41's probe keys a/b/c taken
out, gl42's keys at the end.  Extra KEY=VALUE arguments replace a key of the same name or are appended as test keys.  -KEY removes a key."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
src = open('/home/claude/main/build_gl41/cfg/mgs2_gl41.cfg').read().split('\n')
if src[-1] == '': src.pop()
args = sys.argv[2:]; pgo = bool(args) and args[0] == 'pgo'
if pgo: args = args[1:]
head = '''# gl42 (2026-11-24; the RENDERER CHECKPOINT build, a MEASUREMENT build like gl39-gl41: from vb 24,000 the turns switch things on and off,
# on purpose).  gl41's file with these changes, each marked 'gl42' where it stands; no other setting changed:
#  1. C2 on everywhere (the lead's decision, DEVIATIONS.md B7): MGS2_OPT2 and every MGS2_OPT2_CYCLE word fffff9ff (gl41: fffff9df; bit 5
#     is C2).  P1, P3 and M5 stay on in every turn; F1 on and XP 3 in every turn as gl41.
#  2. MGS2_NGE_CYCLE takes the digit 2: the ship AND the people drawn natively (native step 2: each people mesh cooked once into the
#     region, the per-frame part done on the ME).  The regions are 256 KB until vb 24,000 (MGS2_NGE_KB), then 384 KB each (MGS2_NGE_KB2)
#     with the people's block beside each (the carve after the opening); the people go native from vb 24,100 (MGS2_NGE_P6LAG 100).
#  3. MGS2_BOUND_CYCLE takes the kind 9 (K-N2R): K-N2 with only chains 2-4 drawn, the main CPU's share with the native picture.
#     Kinds (README.txt has the table): K-A everything drawn, today's path; K-N1 the ship native; K-N2 the ship and the people native;
#     K-N2R; K7 nothing drawn (the main CPU alone).  Digests on in t55 and t73 (both K-N2), XP 7 (3 + the check) there.
#  4. New key MGS2_NGE_HPROBE=1: the people's reuse check's content hash timed on the main CPU ('reuse hash checks' and 'reuse hash us'
#     in each 'nge people turn' line).
#  5. MGS2_PROBE41 and MGS2_PROBE41_CULL taken out (gl41's one-time lever probes; gl41's run answers them); MGS2_L41_LOG kept.
# The people native change the game's state hash from vb ~24,200 on (the people's packets the builder no longer writes), so hashref.txt
# is gl42's own (the PC twin with these keys), not gl33's.  The state does not depend on the GE's timing (the people's start is a fixed
# vblank).  gl41's notes follow.'''
K = dict(l.split('=', 1) for l in os.popen('python3 -I %s %s' % (os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sched42.py'), 'pgokeys' if pgo else 'keys')).read().split('\n') if l)
rep = {k: K[k] for k in ('MGS2_OPT_CYCLE', 'MGS2_OPT2_CYCLE', 'MGS2_HALF_CYCLE', 'MGS2_BOUND_CYCLE', 'MGS2_NAT_F1_CYCLE', 'MGS2_DIG_CYCLE', 'MGS2_XP_CYCLE', 'MGS2_NGE_CYCLE')}
rep['MGS2_OPT2'] = 'fffff9ff'
gone = ('MGS2_PROBE41', 'MGS2_PROBE41_CULL') + (('MGS2_GL_SHOT',) if pgo else ())
extra = {}; minus = set()
for a in args:
    if a.startswith('-'): minus.add(a[1:])
    else: k, v = a.split('=', 1); extra[k] = v
out = head.split('\n'); seen = set()
for l in src:
    k = l.split('=', 1)[0] if l.startswith('MGS2_') else None
    if k in minus: out.append('# gl42 test file: taken out: ' + l); seen.add(k)
    elif k in extra: out += ['# gl42 test key (was: %s)' % l, '%s=%s' % (k, extra[k])]; seen.add(k)
    elif k in rep: out += ["# gl42: gl42's value (see the top)", '%s=%s' % (k, rep[k])]
    elif k in gone: out.append('# gl42: taken out%s: %s' % (' (the PGO training: no shots)' if k == 'MGS2_GL_SHOT' else '', l))
    else: out.append(l)
assert not (minus - seen), minus - seen
def tail(k, v):
    if k in minus: seen.add(k); return ['# gl42 test file: taken out: %s=%s' % (k, v)]
    if k in extra: seen.add(k); return ['# gl42 test key (the delivery file has %s=%s)' % (k, v), '%s=%s' % (k, extra[k])]
    return ['%s=%s' % (k, v)]
out += ["# gl42: native step 2's regions (the defaults, written out): 256 KB until vb 24,000, then 384 KB; the people from vb 24,100."]
out += tail('MGS2_NGE_KB', '256') + tail('MGS2_NGE_KB2', '384') + tail('MGS2_NGE_P6LAG', '100')
out += ["# gl42: the reuse content hash's probe (see the top)."] + tail('MGS2_NGE_HPROBE', '1')
for k, v in extra.items():
    if k not in seen: out += ['# gl42 test key', '%s=%s' % (k, v)]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 64)')
