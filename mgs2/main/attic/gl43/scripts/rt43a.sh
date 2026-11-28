#!/bin/sh
# rt43a.sh (gl43; build_gl42/rt42a.sh with build_gl43 and gl43's rain windows, ONE HEAVY JOB AT A TIME under nice 19):
#  1. the use build u (PGO=use with build_gl43/gcda), pgochk;
#  2. traces of u (PPSSPPHeadless_fcr) with the TRAINING cfg (cfg/mgs2_gl43pgo.cfg, tools/sched43.py's kinds by turn), windows ascending:
#     t0 16,001 17,501 (test) 26,801 (t28 K-A deck) 27,101 (t31 K-N2 deck) 29,201 (t52 K-A rain) 29,301 (t53 K-B) 29,601 (t56 K-N2)
#        29,701 (t57 K-N2B); t1 31,901 .. 37,151 (cfg/mgs2_gl43trc_t1.cfg: deck 31800, as gl39-gl42);
#     t2 16,101 (the opening's main-drawing turn) 26,901 (t29 K-N2R deck) 30,201 (t62 K-B) 30,401 (t64 K7) 30,501 (t65 K7B)
#        30,801 (t68 K-A walk) 30,901 (t69 K-B walk) 31,201 (t72 K-N2B walk);
#  3. t3: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg), two 5-vblank windows inside each model window, as gl42;
#  4. the model of u on the four 80-vblank guard windows (the guard cfg): the caps of the search (rt43b.sh).
R=/home/claude/main/build_gl43; cd $R/m || exit 1
for d in u ord; do [ -e $R/$d ] && { echo "rt43a.sh: $R/$d exists: move it aside first"; exit 1; }; done
for f in t0 t1 t2 t3; do [ -e $R/lay/$f.it ] && { echo "rt43a.sh: $R/lay/$f.it exists: move lay aside"; exit 1; }; done
nice -n 19 env PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
nice -n 19 sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1; echo "pgochk rc $? $(date -u +%H:%M:%S)"; tail -n 3 $R/pgochk_u.out | cut -c1-200
T=$R/cfg/mgs2_gl43pgo.cfg; G=/home/claude/main/build_gl43/cfg_emu/mgs2_m37_rc0_b43off.cfg
gd() { [ -d /home/claude/main/build_gl43/ms/$1/.ppsspp ] && rm -rf "/home/claude/main/build_gl43/ms/$1"; }   # the game folder (game data), looked at (a .ppsspp tree)
TCFG=$T nice -n 19 sh scripts/pgo.sh trace $R/u t0 16001 17501 26801 27101 29201 29301 29601 29701 > $R/trace_t0.out 2>&1; echo "t0 rc $? $(date -u +%H:%M:%S)"; gd t0
TCFG=$R/cfg/mgs2_gl43trc_t1.cfg nice -n 19 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1; echo "t1 rc $? $(date -u +%H:%M:%S)"; gd t1
TCFG=$T nice -n 19 sh scripts/pgo.sh trace $R/u t2 16101 26901 30201 30401 30501 30801 30901 31201 > $R/trace_t2.out 2>&1; echo "t2 rc $? $(date -u +%H:%M:%S)"; gd t2
TCFG=$G nice -n 19 sh scripts/pgo.sh trace $R/u t3 16121 16161 21121 21161 23121 23161 27521 27561 > $R/trace_t3.out 2>&1; echo "t3 rc $? $(date -u +%H:%M:%S)"; gd t3
PSPPROF_DHIST=64 nice -n 19 sh scripts/pgo.sh model $R/u mu $G /home/claude/main/build_gl41/cfg/hashref_gl38.txt 16110:16190 21110:21190 23110:23190 27510:27590 > $R/model_mu.out 2>&1; echo "mu rc $? $(date -u +%H:%M:%S)"; gd mu
for t in t0 t1 t2 t3; do [ -s $R/lay/$t.it ] || { echo "rt43a.sh: no trace $t"; exit 1; }; tail -n 2 $R/trace_$t.out | cut -c1-200; done
cat $R/model_mu.out | cut -c1-240
ls -la $R/lay/*.it; echo "rt43a.sh: done $(date -u +%H:%M:%S)"
