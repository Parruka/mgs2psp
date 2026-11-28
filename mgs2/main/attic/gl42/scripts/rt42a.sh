#!/bin/sh
# rt42a.sh (gl42; build_gl41/rt41a.sh with gl42's windows, ONE HEAVY JOB AT A TIME under nice 19):
#  1. the use build u (PGO=use with build_gl42/gcda), pgochk;
#  2. traces of u (PPSSPPHeadless_fcr, pgo.sh's default) with the TRAINING cfg (cfg/mgs2_gl42pgo.cfg, gl42's kinds by turn):
#     t0 16,001 17,501 (test) 26,801 (t28 K-A deck) 27,101 (t31 K-N2 deck) 29,801 (t58 K-A rain) 29,901 (t59 K-N2 rain)
#        30,101 (t61 K-N2R rain) 30,601 (t66 K-A, heavier); t1 31,901 .. 37,151 (cfg/mgs2_gl42trc_t1.cfg: deck 31800, as gl39-gl41);
#     t2 16,101 (the opening's main-drawing turn) 30,801 (t68 K-A walk) 30,901 (t69 K-N2 walk) 31,101 (t71 K-N2R walk)
#        30,301 (t63 K7 rain) 29,701 (t57 K-N1 rain) 26,901 (t29 K-N2R deck) 30,701 (t67 K-N2R rain, heavier);
#     (t2 fixed 05:10: pgo.sh trace stops the run 10 vblanks after the LAST window given and the .it holds the windows in time order,
#     so the windows go in ascending order: 16,101 26,901 29,701 30,301 30,701 30,801 30,901 31,101.)
#  3. t3: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg: main-drawing turns in odd hundreds), two 5-vblank windows inside each
#     model window: 16,121 16,161 21,121 21,161 23,121 23,161 27,521 27,561;
#  4. the model of u on the four 80-vblank guard windows (16110:16190 21110:21190 23110:23190 27510:27590, the guard cfg): u's I-misses
#     there, which turn gl36's + 2 % into the search's caps on t3's windows.
R=/home/claude/main/build_gl42; cd $R/m || exit 1
for d in u ord; do [ -e $R/$d ] && { echo "rt42a.sh: $R/$d exists: move it aside first"; exit 1; }; done
for f in t0 t1 t2 t3; do [ -e $R/lay/$f.it ] && { echo "rt42a.sh: $R/lay/$f.it exists: move lay aside"; exit 1; }; done
nice -n 19 env PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
nice -n 19 sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1; echo "pgochk rc $? $(date -u +%H:%M:%S)"; tail -n 3 $R/pgochk_u.out | cut -c1-200
T=$R/cfg/mgs2_gl42pgo.cfg; G=/home/claude/main/build_gl38/cfg_emu/mgs2_m37_rc0.cfg
gd() { [ -d /home/claude/main/build_gl42/ms/$1/.ppsspp ] && rm -rf "/home/claude/main/build_gl42/ms/$1"; }   # the game folder (game data), looked at (a .ppsspp tree)
TCFG=$T nice -n 19 sh scripts/pgo.sh trace $R/u t0 16001 17501 26801 27101 29801 29901 30101 30601 > $R/trace_t0.out 2>&1; echo "t0 rc $? $(date -u +%H:%M:%S)"; gd t0
TCFG=$R/cfg/mgs2_gl42trc_t1.cfg nice -n 19 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1; echo "t1 rc $? $(date -u +%H:%M:%S)"; gd t1
TCFG=$T nice -n 19 sh scripts/pgo.sh trace $R/u t2 16101 26901 29701 30301 30701 30801 30901 31101 > $R/trace_t2.out 2>&1; echo "t2 rc $? $(date -u +%H:%M:%S)"; gd t2
TCFG=$G nice -n 19 sh scripts/pgo.sh trace $R/u t3 16121 16161 21121 21161 23121 23161 27521 27561 > $R/trace_t3.out 2>&1; echo "t3 rc $? $(date -u +%H:%M:%S)"; gd t3
PSPPROF_DHIST=64 nice -n 19 sh scripts/pgo.sh model $R/u mu $G /home/claude/main/build_gl41/cfg/hashref_gl38.txt 16110:16190 21110:21190 23110:23190 27510:27590 > $R/model_mu.out 2>&1; echo "mu rc $? $(date -u +%H:%M:%S)"; gd mu
for t in t0 t1 t2 t3; do [ -s $R/lay/$t.it ] || { echo "rt42a.sh: no trace $t"; exit 1; }; tail -n 2 $R/trace_$t.out | cut -c1-200; done
cat $R/model_mu.out | cut -c1-240
ls -la $R/lay/*.it; echo "rt42a.sh: done $(date -u +%H:%M:%S)"
