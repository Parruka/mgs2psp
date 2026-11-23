#!/usr/bin/env python3
"""ed5_m5_inert.py (gl39, 2026-11-23, build agent): M5 is NOT in the PSP EBOOT (its block went into runtime/kernel.h, the PC twin's file;
the PSP's boot_rt.o takes rt/kernel.h, which is gl38's).  MGS2_OPT2's bit 18 therefore does nothing on the PSP.  This script changes
the WORDS only: the cfg's comment (no key changes), tools/mkcfg39.py's comment, and the reader template (turns are no longer split or
labelled by M5).  Run once, from anywhere: python3 -I ed5_m5_inert.py"""
import sys
R = '/home/claude/main/build_gl39'
def rep(p, pairs):
    s = open(p).read()
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:60], s.count(a)); s = s.replace(a, b)
    open(p, 'w').write(s); print('edited', p)
OLD6 = ("#  6. MGS2_OPT_CYCLE, MGS2_OPT2_CYCLE, MGS2_HALF_CYCLE: 16 values (was 8), turn t takes value t % 16.  MGS2_OPT2's bit 18 is the new\n"
        "#     switch M5 (the game kernel's interrupt walk; exact): on (fffff9df) but in the turns with t % 16 = 0 or 1 (fffbf9df).\n")
NEW6 = ("#  6. MGS2_OPT_CYCLE, MGS2_OPT2_CYCLE, MGS2_HALF_CYCLE: 16 values (was 8), turn t takes value t % 16.  MGS2_OPT2's bit 18 was meant as\n"
        "#     the switch of M5 (the game kernel's interrupt walk): THIS EBOOT DOES NOT HAVE M5: the bit does nothing here (fffbf9df and fffff9df act the same).\n")
for p in (R + '/cfg/mgs2_gl39.cfg', R + '/tools/mkcfg39.py'):
    s = open(p).read()
    if OLD6 in s: rep(p, [(OLD6, NEW6)])
    else:   # mkcfg39.py may hold the lines inside a string with other quoting: show what is there
        print('NOT FOUND AS TEXT in', p); [print('   ', l) for l in s.split('\n') if 'M5' in l]
T = R + '/rd/rd39pc.tpl.py'
rep(T, [
 ("the ME by MGS2_LOAD_CYCLE), F1 on or off, the frame digests on or off, M5 on or off (README.txt has the table).",
  "the ME by MGS2_LOAD_CYCLE), F1 on or off, the frame digests on or off (README.txt has the table).  M5 is NOT in this EBOOT."),
 ("# digests, F1, M5); the coupling probe lines", "# digests, F1); the coupling probe lines"),
 ("SCHED = @@SCHED@@   # (turn, vb, kind, F1, digest off, load, M5, scene)\n",
  "SCHED = @@SCHED@@   # (turn, vb, kind, F1, digest off, load, MGS2_OPT2's bit 18, scene)\n"
  "SCHED = [(t, vb, k, f1, d, l, 1, sc) for (t, vb, k, f1, d, l, _b18, sc) in SCHED]   # bit 18 (M5) does nothing in this EBOOT: turns are not split by it\n"),
 ("print('== MAIN-DRAWING STEPS before vb 24,000",
  "print('== NOTE: M5 (the game kernel\\'s interrupt walk, MGS2_OPT2 bit 18) is NOT in this EBOOT: the bit does nothing here, and the turns are not split by it.')\n"
  "print('== MAIN-DRAWING STEPS before vb 24,000"),
 ("print('== TURNS  vb | t scene | kind F1 digests load M5 | who |", "print('== TURNS  vb | t scene | kind F1 digests load | who |"),
 ("F%d dig %-4s %-15s M5 %d | %-4s |", "F%d dig %-4s %-15s | %-4s |"),
 ("LN[l] if k == 8 else '-', m5, x.get('who', '?'),", "LN[l] if k == 8 else '-', x.get('who', '?'),"),
 ("if rs: line('%s, digests %s%s, F1 %s, M5 %s' % (KN[k], d, ', ' + LN[l] if k == 8 else '', 'on' if f1 else 'OFF', 'on' if m5 else 'OFF'), rs)",
  "if rs: line('%s, digests %s%s, F1 %s' % (KN[k], d, ', ' + LN[l] if k == 8 else '', 'on' if f1 else 'OFF'), rs)"),
 ("    delta('K7 M5 OFF minus on (digests off)', rows(scene, 7, d='off', m5=0), rows(scene, 7, d='off', m5=1))\n", ""),
 ("delta('K0 all off minus all on (digests on, F1 off, M5 off | digests off, F1 on, M5 on)', rows(scene, 0, d='on', f1=0, m5=0), rows(scene, 0, d='off', f1=1, m5=1))",
  "delta('K0 digests on, F1 OFF minus digests off, F1 on (gl38\\'s K0 against gl39\\'s play K0)', rows(scene, 0, d='on', f1=0), rows(scene, 0, d='off', f1=1))"),
])
