#!/bin/sh
# rt_b38.sh (gl38; build_gl36/retrain/rt_b.sh's steps after the profile pgo_a38.sh):
#  1. the use build u (PGO=use with build_gl38/gcda), pgochk;
#  2. the traces of u with the TRACE cfg (cfg/mgs2_gl38trc.cfg = the training cfg without MGS2_BOUND_CYCLE: gl36's turns, every kind of
#     drawing in every window, so the order is searched for the whole picture): t0 (16,001 17,501 26,501 27,801 29,601 29,801 30,101
#     31,101), t1 (31,901 .. 37,151; cfg/mgs2_gl38trc_t1.cfg), t2 (16,101 the opening's main turn, 27,501 the steady deck's main turn);
#  3. rt_order38.sh: gl36's objective (each window normalised by its seed-order misses, -guard 10: no training window more than 1 %
#     above its seed order; 17,501 and 36,401 held out), the seed = gl36's final order with the newly hot appended, 40,000 moves;
#  4. the relink (rl38.sh) and its real addresses' misses on the traces (rt_new38.sh).
# Run from build_gl38 with nice 10.
R=/home/claude/main/build_gl38; cd $R/m || exit 1
for d in u psp ord; do [ -e $R/$d ] && { echo "rt_b38.sh: $R/$d exists: move it aside first"; exit 1; }; done
for f in t0 t0.calls t1 t1.calls t2 t2.calls; do [ -e $R/lay/$f ] && { echo "rt_b38.sh: $R/lay/$f exists: move lay aside"; exit 1; }; done
nice -n 10 env PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
nice -n 10 sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1 &
TCFG=$R/cfg/mgs2_gl38trc.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t0 16001 17501 26501 27801 29601 29801 30101 31101 > $R/trace_t0.out 2>&1 &
TCFG=$R/cfg/mgs2_gl38trc_t1.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1 &
TCFG=$R/cfg/mgs2_gl38trc.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t2 16101 27501 > $R/trace_t2.out 2>&1 &
wait; echo "traces and pgochk done $(date -u +%H:%M:%S)"; tail -n 3 $R/pgochk_u.out | cut -c1-200
for t in t0 t1 t2; do rm -rf "/home/claude/main/build_gl38/ms/$t"; [ -s $R/lay/$t.it ] || { echo "rt_b38.sh: no trace $t"; exit 1; }; done
cd $R && sh rt_order38.sh > $R/rt_order.out 2>&1; r=$?; echo "order rc $r $(date -u +%H:%M:%S)"; tail -n 30 $R/rt_order.out | cut -c1-200; [ $r = 0 ] || exit 1
sh rl38.sh psp > $R/rl38.out 2>&1; echo "relink rc $? $(date -u +%H:%M:%S)"; tail -6 $R/rl38.out | cut -c1-240
sh rt_new38.sh > $R/rt_new.out 2>&1; cat $R/rt_new.out | cut -c1-240
echo "rt_b38.sh: done $(date -u +%H:%M:%S)"
