"""mkcfg40.py OUT [KEY=VALUE ...]: gl40's cfg from gl39's delivery cfg (build_gl39/cfg/mgs2_gl39.cfg): a gl40 header, the schedule keys
replaced by tools/sched40.py's, MGS2_XP_CYCLE and MGS2_PROBE10=1 at the end.  Extra KEY=VALUE arguments replace a key of the same name or
are appended as test keys.  -KEY removes a key."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
src = open('/home/claude/main/build_gl39/cfg/mgs2_gl39.cfg').read().split('\n')
if src[-1] == '': src.pop()
head = '''# gl40 (build agent, 2026-11-23; a MEASUREMENT build like gl39: in some turns from vb 24,000 nothing or not everything is drawn, on purpose):
# gl39's file with these changes, each marked 'gl40' where it stands; no other setting changed:
#  1. New key MGS2_XP_CYCLE, one digit a turn of 100 vblanks from vb 24,000: the main CPU's copy plan (made as each recorded transfer ends):
#       0 gl39's plan; 1 its merge sort alternates two arrays instead of copying back after every pass; 3 that, and a range that does not
#       meet the last one is joined with any of the 7 before it that it meets; 7 = 3 plus a check: gl39's plan made again and compared.
#     Every form gives the same copies (the check proves it plan by plan); only the main CPU's time changes.
#  2. New key MGS2_PROBE10=1: the plan's time by part, a 'gl40 plan' line per turn in log.txt (and M5's walks a turn).
#  3. MGS2_BOUND_CYCLE / MGS2_DIG_CYCLE / MGS2_LOAD_CYCLE: gl40's turns (README.txt has the table): from t51 the rain wait and the walk
#     are mostly K0 turns taking XP in turns; digests on only in t55, t65, t68 (and 29,490-29,499); no ME load turn after t50.
#  4. MGS2_NAT_F1_CYCLE: F1 on in every turn.  MGS2_OPT2's bit 18 is M5 again, and this EBOOT has it (k_take_m5): off in turns t % 16 = 0, 1.
# The game's state is the same in every turn and with every key (the PC twin's 685 hashes): only the picture's timing changes.
# hashref.txt is gl33's (c9ddb593).  gl39's notes follow (its item 6 about M5 no longer holds: see 4 above).'''
K = dict(l.split('=', 1) for l in os.popen('python3 -I %s keys' % os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sched40.py')).read().split('\n') if l)
rep = {k: K[k] for k in ('MGS2_OPT_CYCLE', 'MGS2_OPT2_CYCLE', 'MGS2_HALF_CYCLE', 'MGS2_BOUND_CYCLE', 'MGS2_NAT_F1_CYCLE', 'MGS2_LOAD_CYCLE', 'MGS2_DIG_CYCLE')}
extra = {}; minus = set()
for a in sys.argv[2:]:
    if a.startswith('-'): minus.add(a[1:])
    else: k, v = a.split('=', 1); extra[k] = v
out = head.split('\n'); seen = set()
for l in src:
    k = l.split('=', 1)[0] if l.startswith('MGS2_') else None
    if k in minus: out.append('# gl40 test file: taken out: ' + l); seen.add(k)
    elif k in extra: out += ['# gl40 test key (was: %s)' % l, '%s=%s' % (k, extra[k])]; seen.add(k)
    elif k in rep: out += ["# gl40: gl40's value (see the top)", '%s=%s' % (k, rep[k])]
    else: out.append(l)
assert not (minus - seen), minus - seen
def tail(k, v):
    if k in extra: seen.add(k); return ['# gl40 test key (the delivery file has %s=%s)' % (k, v), '%s=%s' % (k, extra[k])]
    return ['%s=%s' % (k, v)]
out += ["# gl40: the copy plan's lever XP a turn and the plan probe (see the top)."] + tail('MGS2_XP_CYCLE', K['MGS2_XP_CYCLE']) + tail('MGS2_PROBE10', '1')
for k, v in extra.items():
    if k not in seen: out += ['# gl40 test key', '%s=%s' % (k, v)]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 64)')
