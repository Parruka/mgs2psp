"""mkcfg38.py OUT [KEY=VALUE ...]: gl38's cfg from gl37's test cfg (build_gl37/cfg/mgs2_gl37.cfg, md5 34147bea): a gl38 header, the key
MGS2_ME_PIC_GE2_AB taken out, and the gl38 keys (tools/sched38.py) at the end.  Extra KEY=VALUE arguments are appended as emulator test keys."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched38
src = open('/home/claude/main/build_gl37/cfg/mgs2_gl37.cfg').read().split('\n')
if src[-1] == '': src.pop()
head = '''# gl38 (build agent, 2026-11-22; a MEASUREMENT build: parts of the picture are left out in turns on purpose): gl37's test file
# (34147bea) with these changes, each marked 'gl38' where it stands; no other setting changed:
#  1. New key MGS2_BOUND_CYCLE, one digit for each turn of 100 vblanks from vb 24,000 (turn t = (vb - 24000) / 100; past the list: 0):
#       0  K0: everything drawn (gl37's picture).
#       1  K1: the ship (the native ship groups, chain 3) recorded but not drawn.
#       2  K2: the people (chain 2 of the frame's list) not drawn.
#       3  K3: the sorted primitives and meshes (chain 6) not drawn, but for the rain's own calls.
#       4  K4: the rain not drawn (drop sheets, splashes, ripples, slow drops, thunder parts), but for the drop sheets nearest the
#              camera (MGS2_GL_RAIN_NEAR: the last 1 of each picture's 4 sheet calls; they come far to near).
#       5  K5: the stage's own program (chain 10: blur / post) not drawn.
#       6  K6: ship, people, chain 6 and the rain not drawn.
#       7  K7: nothing drawn: the Media Engine rests for the turn and the main CPU runs the game's logic alone.
#     From vb 24,000 every turn is a Media Engine turn (but K7's), no longer every other one.  The game's state is the same in
#     every kind (the PC twin's 685 hashes with every kind in every scene): only the picture and the times change.  A
#     'gl38 bound' line per turn in log.txt names the turn's kind and counts what was left out.
#  2. New key MGS2_GL_RAIN_NEAR=1 (the near sheets K4 keeps; 0 = all four sheets kept).  MGS2_GL_RAIN_CUT=1 (not set here) would
#     leave the rain out as K4 does in the whole run: the later default's own switch.
#  3. New key MGS2_NAT_F1_CYCLE, one digit for each of the same turns (F1: in a run of fast native ship groups that is not replayed,
#     a group whose VU memory writes a later group of the run overwrites does not write them; exact, check mode MGS2_NAT_F1CHK=1
#     found 0 differing).  1 in some of the K0 turns only: K0 turns with F1 against K0 turns without it.  A 'nat f1' line per turn.
#  4. MGS2_ME_PIC_GE2_AB=30500 taken out (it switched the GE overlap off in every other 200 vblanks from 30,500: a second thing
#     changing in the rain walk's turns).  The overlap stays on (MGS2_ME_PIC_GE2=1) in the whole run.
#  5. New timer lines per Media Engine turn (timers only): 'gl38 timers' (the hooks', the list builds' and the records' parts) and
#     'people split' (a people group by stage).  gl36's 'nat split' and 'me chains' lines are kept.
# hashref.txt is gl33's (c9ddb593).  gl37's notes follow.'''
out = head.split('\n'); n = 0
for l in src:
    if l.startswith('MGS2_ME_PIC_GE2_AB='):
        out.append('# gl38: taken out (see the top): ' + l); n += 1
    else: out.append(l)
assert n == 1
out += ['# gl38: the kind of each turn of 100 vblanks from vb 24,000 (see the top; README.txt has the table by vblank).',
        'MGS2_BOUND_CYCLE=' + sched38.bound,
        '# gl38: the near drop sheets K4 keeps (of 4 a picture).',
        'MGS2_GL_RAIN_NEAR=1',
        '# gl38: F1 by the same turns (see the top).',
        'MGS2_NAT_F1_CYCLE=' + sched38.f1]
for a in sys.argv[2:]:
    out += ['# gl38 emulator test key', a]
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
keys = [l.split('=')[0] for l in out if l.startswith('MGS2_')]
assert len(keys) == len(set(keys)), 'a key twice'
print(sys.argv[1], 'MGS2 keys', len(keys), '(+5 defaults in main_psp.c: the table holds 48)')
