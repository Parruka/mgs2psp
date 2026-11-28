"""mkcfg41.py OUT [KEY=VALUE ...]: gl41's cfg from gl40's delivery cfg (build_gl40/cfg/mgs2_gl40.cfg): a gl41 header, the schedule keys
replaced by tools/sched41.py's, gl40's ME load keys taken out, gl41's keys at the end.  Extra KEY=VALUE arguments replace a key of the
same name or are appended as test keys.  -KEY removes a key."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
src = open('/home/claude/main/build_gl40/cfg/mgs2_gl40.cfg').read().split('\n')
if src[-1] == '': src.pop()
head = '''# gl41 (build agent, 2026-11-23; a MEASUREMENT build like gl39-gl40: from vb 24,000 the turns switch things on and off, on purpose):
# gl40's file with these changes, each marked 'gl41' where it stands; no other setting changed:
#  1. New key MGS2_NGE_CYCLE, one digit a turn of 100 vblanks from vb 24,000: 1 = the ship's groups drawn natively (native step 1: each
#     ship mesh made once into a list the GE calls, in a 256 KB region at the end of each frame memory); 0 = today's path.
#     Kinds (README.txt has the table): K-A everything drawn, today's path; K-N1 the ship native; K7 nothing drawn (the main CPU alone).
#  2. MGS2_OPT2's bit 19 is P1 (the VU0 call's fast path) and bit 20 P3 (the box cull without its 8 checks): both on (fffff9df), each
#     off in its own K-A turns (fff7f9df: P1 off; ffeff9df: P3 off).  MGS2_OPT_CYCLE / _OPT2_CYCLE / _HALF_CYCLE now hold a word for
#     each of the 74 turns (the EBOOT takes up to 128; gl40 took 16).
#  3. MGS2_BOUND_CYCLE / MGS2_DIG_CYCLE / MGS2_XP_CYCLE: gl41's turns.  Kinds 0 and 7 only (no K5, no K8): MGS2_LOAD_CYCLE and
#     MGS2_LOAD_US taken out.  Digests on in t55 (K-A), t56 (K-N1), t73 (K-N1) and 29,490-29,499; XP 3 in every turn, 7 (3 + the check)
#     in the digest turns.  M5 on in every turn.
#  4. New keys MGS2_PROBE41=3 (probes a and b at the first vblank: a times a walk over the code with and without a cache op, b checks
#     the VFPU's rule P3 relies on) and MGS2_PROBE41_CULL=5000 (probe c at the box cull's 5,000th call: both forms on the same input);
#     MGS2_L41_LOG=1: a 'gl41 levers' line every 100 vblanks.  Lines 'L41 probe a/b/c' in log.txt.
#     If the run stops at its start with an exception and the last lines name 'L41 probe a', set MGS2_PROBE41=2 and run again.
# The game's state is the same in every turn and with every key (the PC twin's 685 hashes): only the picture's timing changes.
# hashref.txt is gl33's (c9ddb593).  gl40's notes follow.'''
K = dict(l.split('=', 1) for l in os.popen('python3 -I %s keys' % os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sched41.py')).read().split('\n') if l)
rep = {k: K[k] for k in ('MGS2_OPT_CYCLE', 'MGS2_OPT2_CYCLE', 'MGS2_HALF_CYCLE', 'MGS2_BOUND_CYCLE', 'MGS2_NAT_F1_CYCLE', 'MGS2_DIG_CYCLE', 'MGS2_XP_CYCLE')}
gone = ('MGS2_LOAD_CYCLE', 'MGS2_LOAD_US')
extra = {}; minus = set()
for a in sys.argv[2:]:
    if a.startswith('-'): minus.add(a[1:])
    else: k, v = a.split('=', 1); extra[k] = v
out = head.split('\n'); seen = set()
for l in src:
    k = l.split('=', 1)[0] if l.startswith('MGS2_') else None
    if k in minus: out.append('# gl41 test file: taken out: ' + l); seen.add(k)
    elif k in extra: out += ['# gl41 test key (was: %s)' % l, '%s=%s' % (k, extra[k])]; seen.add(k)
    elif k in rep: out += ["# gl41: gl41's value (see the top)", '%s=%s' % (k, rep[k])]
    elif k in gone: out.append('# gl41: taken out (no K8 turn): ' + l)
    else: out.append(l)
assert not (minus - seen), minus - seen
def tail(k, v):
    if k in minus: seen.add(k); return ['# gl41 test file: taken out: %s=%s' % (k, v)]
    if k in extra: seen.add(k); return ['# gl41 test key (the delivery file has %s=%s)' % (k, v), '%s=%s' % (k, extra[k])]
    return ['%s=%s' % (k, v)]
out += ["# gl41: native step 1 a turn (see the top)."] + tail('MGS2_NGE_CYCLE', K['MGS2_NGE_CYCLE'])
out += ["# gl41: the levers' probes and counter line (see the top)."] + tail('MGS2_PROBE41', '3') + tail('MGS2_PROBE41_CULL', '5000') + tail('MGS2_L41_LOG', '1')
for k, v in extra.items():
    if k not in seen: out += ['# gl41 test key', '%s=%s' % (k, v)]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 64)')
