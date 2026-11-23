"""mkcfg39.py OUT [KEY=VALUE ...]: gl39's cfg from gl38's delivery cfg (build_gl38/cfg/mgs2_gl38.cfg): a gl39 header, gl38's schedule
keys replaced by tools/sched39.py's, MGS2_NAT_REUSE_CYCLE and MGS2_GL_RAIN_NEAR taken out (re-use off, no K4 turn), the gl39 keys at the
end.  Extra KEY=VALUE arguments replace a key of the same name or are appended as test keys.  -KEY removes a key."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched39
src = open('/home/claude/main/build_gl38/cfg/mgs2_gl38.cfg').read().split('\n')
if src[-1] == '': src.pop()
head = '''# gl39 (build agent, 2026-11-23; a MEASUREMENT build: in most turns from vb 24,000 nothing or not everything is drawn, on purpose):
# gl38's file with these changes, each marked 'gl39' where it stands; no other setting changed:
#  1. MGS2_BOUND_CYCLE: gl39's turns (README.txt has the table by vblank).  Kinds used: 0 everything drawn; 5 the stage's program
#     (chain 10) not drawn; 7 nothing drawn, the main CPU's turn with the Media Engine resting; and the new kind
#       8  K8: nothing drawn, but the turn is the Media Engine's: empty pictures go through the hand-over, the ME and the GE.
#  2. New key MGS2_LOAD_CYCLE, one digit for each of the same turns, read in K8 turns only: what the Media Engine does besides, for
#     MGS2_LOAD_US (here 100,000) microseconds for each picture (2 vblanks), on its own memory or read-only memory (no system call, no hardware register):
#       0 nothing; 1 arithmetic in registers; 2 cached reads of the program's code (every read a line from RAM); 3 uncached writes
#       of its own 256-byte block (every write a write to RAM).
#     K7 against K8 with each load tells how much the main CPU's game logic slows when the ME is busy in each way.
#  3. New key MGS2_DIG_CYCLE, one digit a turn: 1 = no frame digest for the pictures that end in the turn (the digest is a check's
#     cost on the Media Engine, about 10-17 ms a step; a real game build makes none).  Digests exist only in MGS2_GL_DIG's range.
#  4. New key MGS2_PROBE9=1: the coupling probes' times (one queue-service call in 8 and every whole-cache write-back of a post are
#     timed: two clock reads each).  A 'gl39 probe' line per turn in log.txt counts what the main CPU did for the queue and the GE.
#  5. MGS2_NAT_F1_CYCLE: F1 on in every turn but two (t63, t65).  MGS2_NAT_REUSE_CYCLE taken out: the re-use is off in the whole run.
#  6. MGS2_OPT_CYCLE, MGS2_OPT2_CYCLE, MGS2_HALF_CYCLE: 16 values (was 8), turn t takes value t % 16.  MGS2_OPT2's bit 18 was meant as
#     the switch of M5 (the game kernel's interrupt walk): THIS EBOOT DOES NOT HAVE M5: the bit does nothing here (fffbf9df and fffff9df act the same).
#  7. MGS2_GL_RAIN_NEAR taken out (no K4 turn).
# The game's state is the same in every kind and with every key (the PC twin's 685 hashes): only the picture and the times change.
# hashref.txt is gl33's (c9ddb593).  gl38's notes follow.'''
K = dict(l.split('=', 1) for l in os.popen('python3 -I %s keys' % os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sched39.py')).read().split('\n') if l)
rep = {'MGS2_OPT_CYCLE': K['MGS2_OPT_CYCLE'], 'MGS2_OPT2_CYCLE': K['MGS2_OPT2_CYCLE'], 'MGS2_HALF_CYCLE': K['MGS2_HALF_CYCLE'],
       'MGS2_BOUND_CYCLE': K['MGS2_BOUND_CYCLE'], 'MGS2_NAT_F1_CYCLE': K['MGS2_NAT_F1_CYCLE']}
drop = {'MGS2_NAT_REUSE_CYCLE', 'MGS2_GL_RAIN_NEAR'}
extra = {}; minus = set()
for a in sys.argv[2:]:
    if a.startswith('-'): minus.add(a[1:])
    else: k, v = a.split('=', 1); extra[k] = v
out = head.split('\n'); seen = set()
for l in src:
    k = l.split('=', 1)[0] if l.startswith('MGS2_') else None
    if k in minus: out.append('# gl39 test file: taken out: ' + l); seen.add(k)
    elif k in extra: out += ['# gl39 test key (was: %s)' % l, '%s=%s' % (k, extra[k])]; seen.add(k)
    elif k in drop: out.append('# gl39: taken out (see the top): ' + l)
    elif k in rep: out += ['# gl39: gl39\'s value (see the top)', '%s=%s' % (k, rep[k])]
    else: out.append(l)
assert not (minus - seen), minus - seen
def tail(k, v):
    if k in extra: seen.add(k); return ['# gl39 test key (the delivery file has %s=%s)' % (k, v), '%s=%s' % (k, extra[k])]
    return ['%s=%s' % (k, v)]
out += ['# gl39: the ME\'s load in K8 turns, the digest-off turns, the probes\' times (see the top).'] + tail('MGS2_LOAD_CYCLE', K['MGS2_LOAD_CYCLE']) \
       + tail('MGS2_DIG_CYCLE', K['MGS2_DIG_CYCLE']) + tail('MGS2_PROBE9', '1')
if os.path.basename(sys.argv[1]) != 'mgs2_gl39pgo.cfg':   # (the training cfg was made before this key: the emulator's thread runs a sixteenth of it either way)
    out += ['# gl39: the load loop\'s time for each picture (2 vblanks) of a K8 turn, microseconds (default 50,000).'] + tail('MGS2_LOAD_US', '100000')
for k, v in extra.items():
    if k not in seen: out += ['# gl39 test key', '%s=%s' % (k, v)]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 64 in gl39, 48 before)')
