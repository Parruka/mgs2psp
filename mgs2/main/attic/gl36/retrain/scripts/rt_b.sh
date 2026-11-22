#!/bin/sh
# retrain/rt_b.sh (prepared 2026-11-22 for gl36's retrain, NOT RUN; look b's plan as the lead set it at 04:32): pgo_b36.sh's steps
# after the profile (pgo_a36.sh, unchanged: its training runs cover vb 15,000-37,790, the deck and the opening included), with the
# order search's new objective:
#  1. the use build u (PGO=use with gcda), pgochk;
#  2. the traces of u: t0 (16,001 17,501 26,501 27,801 29,601 29,801 30,101 31,101), t1 (31,901 .. 37,151), and NEW t2 (16,101 the
#     opening's main turn, 27,501 the steady deck's main turn);
#  3. rt_order.sh: the deck route's 10 windows trained but 17,501 (held out), t1's 8 but 36,401, each window normalised by its
#     seed-order misses (-norm), the guard (-guard 10: no training window more than 1 % above its seed order), from the seed (gl36's
#     last search order, lay_old/order_s1.txt, -inall; the newly hot appended: variant B of order_new36b.sh), 40,000 moves; the
#     per-window table against the seed and gl35's delivered order; then tools/layout_order.py into ord/final as order36.sh;
#  4. the relink (rl36.sh), the relinked build's misses on the traces (layout_sim36 -new, one route at a time), fa36.sh;
#  5. the model (model36.sh) and the pair-mean score (rt_score.sh): deck and opening in both turns against gl35, judged by the pair's mean.
# Run from build_gl36 with nice 10.  Needs: the tree with the cull diff applied (t36 + attic/cull/cull_v3_vs_t36.diff when gated),
# a fresh gcda (pgo_a36.sh), no u/psp/ord folders (move the old ones aside first).
R=${R:-/home/claude/main/build_gl36}; cd $R/m || exit 1
for d in u psp ord; do [ -e $R/$d ] && { echo "rt_b.sh: $R/$d exists: move it aside first"; exit 1; }; done
for f in t0 t0.calls t1 t1.calls t2 t2.calls; do [ -e $R/lay/$f ] && { echo "rt_b.sh: $R/lay/$f exists (pgo.sh would take the old window files as done): move lay aside"; exit 1; }; done
nice -n 10 env PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
nice -n 10 sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1 &
nice -n 10 sh scripts/pgo.sh trace $R/u t0 16001 17501 26501 27801 29601 29801 30101 31101 > $R/trace_t0.out 2>&1 &
TCFG=$R/cfg/mgs2_gl36pgo_t1.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1 &
nice -n 10 sh scripts/pgo.sh trace $R/u t2 16101 27501 > $R/trace_t2.out 2>&1 &
wait; echo "traces and pgochk done $(date -u +%H:%M:%S)"; tail -n 3 $R/pgochk_u.out | cut -c1-200
for t in t0 t1 t2; do [ -s $R/lay/$t.it ] || { echo "rt_b.sh: no trace $t"; exit 1; }; done
cd $R && sh retrain/rt_order.sh > $R/rt_order.out 2>&1; r=$?; echo "order rc $r $(date -u +%H:%M:%S)"; tail -n 30 $R/rt_order.out | cut -c1-200; [ $r = 0 ] || exit 1
sh rl36.sh psp > $R/rl36.out 2>&1; echo "relink rc $? $(date -u +%H:%M:%S)"; tail -6 $R/rl36.out | cut -c1-240
sh retrain/rt_new.sh > $R/rt_new.out 2>&1; cat $R/rt_new.out | cut -c1-240
sh fa36n.sh > $R/fa36.out 2>&1; cat $R/fa36.out | cut -c1-200
sh model36c.sh > $R/model36c.out 2>&1; sh score36c.sh > $R/score36c_run.out 2>&1; cat $R/score36c.out
echo "rt_b.sh: done $(date -u +%H:%M:%S)"
