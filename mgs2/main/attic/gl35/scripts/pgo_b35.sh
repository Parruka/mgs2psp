#!/bin/sh
# pgo_b35.sh (gl35, after pgo_a35.sh): the use build u (PGO=use with build_gl35/gcda; psp.sh stops on a split native wrapper), the
# lost-profile check (pgochk.sh), the two instruction traces of u (deck route t0, inside route t1: 8 windows of 5 steps each), the
# order search (order35.sh: 40,000 moves, 4 threads, seed 1, from gl33's order), the relink with the order (rl35.sh -> psp), the
# relinked build's misses on the traces (pgo.sh new) and the fully associative figure (fa35.sh).
# The deck route's windows are gl20's but one: 29,801 (a plain ME turn in the rain wait) stands in place of 30,701 (a plain main turn),
# so the rain has each arm of the keeps in an ME turn and in a main turn: 29,601 keep ME, 29,801 plain ME, 30,101 keep main, 31,101
# plain main (MGS2_NAT_KEEP_CYCLE=15,15,0,0 from vb 24,001).  gl35: held out of the search only 27,801 (and t1's last window): the main-turn windows of both arms are in it (m/scripts/pgo2.sh).
R=/home/claude/main/build_gl35; cd $R/m || exit 1
[ -e $R/u ] && { echo "pgo_b35.sh: $R/u exists"; exit 1; }
PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1 &
sh scripts/pgo.sh trace $R/u t0 16001 17501 26501 27801 29601 29801 30101 31101 > $R/trace_t0.out 2>&1 &
TCFG=$R/cfg/mgs2_gl35pgo_t1.cfg sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1 &
wait; echo "traces and pgochk done $(date -u +%H:%M:%S)"; head -3 $R/trace_t0.out $R/trace_t1.out | cut -c1-200; tail -3 $R/pgochk_u.out | cut -c1-200
cd $R && sh order35.sh > $R/order35.out 2>&1; echo "order rc $? $(date -u +%H:%M:%S)"; tail -12 $R/order35.out | cut -c1-240
sh rl35.sh psp > $R/rl35.out 2>&1; echo "relink rc $? $(date -u +%H:%M:%S)"; tail -6 $R/rl35.out | cut -c1-240
cd $R/m && sh scripts/pgo2.sh new $R/psp f > $R/new_f.out 2>&1; cat $R/new_f.out | cut -c1-240
cd $R && sh fa35.sh > $R/fa35.out 2>&1; cat $R/fa35.out | cut -c1-200
echo "pgo_b35.sh: done $(date -u +%H:%M:%S)"
