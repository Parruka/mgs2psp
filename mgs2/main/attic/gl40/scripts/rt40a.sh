#!/bin/sh
# rt40a.sh (gl40; build_gl39/rt39a.sh's steps with gl40's windows, plus the main-drawing guard windows):
#  1. the use build u (PGO=use with build_gl40/gcda), pgochk;
#  2. traces of u (PPSSPPHeadless_fcr, pgo.sh's default) with the TRAINING cfg (cfg/mgs2_gl40pgo.cfg, gl40's kinds by turn):
#     t0 16,001 17,501 26,901 (t29 K0 deck) 27,101 (t31 K7 deck) 29,601 (t56 K0 XP0) 29,701 (t57 K0 XP3) 29,801 (t58 K7)
#        30,601 (t66 K0 XP3); t1 31,901 .. 37,151 (cfg/mgs2_gl40trc_t1.cfg: deck 31800, as gl39); t2 16,101 (the opening's
#        main-drawing turn) 30,901 (t69 walk K0 XP0) 31,001 (t70 walk K0 XP3) 31,101 (t71 walk K7);
#  3. t3: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg: main-drawing turns in odd hundreds, the cfg of the plan's section 3
#     model table), two 5-vblank windows inside each model window: 16,121 16,161 21,121 21,161 23,121 23,161 27,521 27,561;
#  4. the model of u (PspProf counts, 80 vblanks: 16110:16190 21110:21190 23110:23190 27510:27590, the guard cfg, as exp40's x36 and
#     x39): u's I-misses there, which turn gl36's + 2 % into the search's caps on t3's windows.
# Run from build_gl40 with nice 10.
R=/home/claude/main/build_gl40; cd $R/m || exit 1
for d in u ord; do [ -e $R/$d ] && { echo "rt40a.sh: $R/$d exists: move it aside first"; exit 1; }; done
for f in t0 t1 t2 t3; do [ -e $R/lay/$f.it ] && { echo "rt40a.sh: $R/lay/$f.it exists: move lay aside"; exit 1; }; done
nice -n 10 env PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
nice -n 10 sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1 &
T=$R/cfg/mgs2_gl40pgo.cfg; G=/home/claude/main/build_gl38/cfg_emu/mgs2_m37_rc0.cfg
TCFG=$T nice -n 10 sh scripts/pgo.sh trace $R/u t0 16001 17501 26901 27101 29601 29701 29801 30601 > $R/trace_t0.out 2>&1 &
TCFG=$R/cfg/mgs2_gl40trc_t1.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1 &
TCFG=$T nice -n 10 sh scripts/pgo.sh trace $R/u t2 16101 30901 31001 31101 > $R/trace_t2.out 2>&1 &
wait; echo "traces t0 t1 t2 and pgochk done $(date -u +%H:%M:%S)"; tail -n 3 $R/pgochk_u.out | cut -c1-200
TCFG=$G nice -n 10 sh scripts/pgo.sh trace $R/u t3 16121 16161 21121 21161 23121 23161 27521 27561 > $R/trace_t3.out 2>&1 &
PSPPROF_DHIST=64 nice -n 10 sh scripts/pgo.sh model $R/u mu $G $R/cfg/hashref_gl38.txt 16110:16190 21110:21190 23110:23190 27510:27590 > $R/model_mu.out 2>&1 &
wait
for t in t0 t1 t2 t3; do d=/home/claude/main/build_gl40/ms/$t; [ -d $d/.ppsspp ] && rm -rf "$d"
  [ -s $R/lay/$t.it ] || { echo "rt40a.sh: no trace $t"; exit 1; }; tail -n 2 $R/trace_$t.out | cut -c1-200; done
rm -rf "/home/claude/main/build_gl40/ms/mu"; cat $R/model_mu.out | cut -c1-240
ls -la $R/lay/*.it; echo "rt40a.sh: done $(date -u +%H:%M:%S)"
