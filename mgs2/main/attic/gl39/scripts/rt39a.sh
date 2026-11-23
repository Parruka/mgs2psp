#!/bin/sh
# rt39a.sh (gl39; build_gl38/rt_b38.sh's steps 1-2 after the profile pgo_a39.sh):
#  1. the use build u (PGO=use with build_gl39/gcda), pgochk;
#  2. the traces of u with the TRAINING cfg (cfg/mgs2_gl39pgo.cfg: gl39's turns, so the rain windows are digest-off K0 turns and K7
#     turns, as the order is to be judged): t0 16,001 17,501 26,901 (t29 K0 deck) 27,101 (t31 K7 deck) 29,601 (t56 K0 digest-off)
#     29,701 (t57 K7) 30,201 (t62 K0 digest-off) 30,601 (t66 K0 digest-off); t1 31,901 .. 37,151 (the w01a route: ME turns, K0, no
#     digests); t2 16,101 (the opening's main-drawing turn) 30,901 (t69 walk K0 digest-off) 31,001 (t70 walk K7).
# Run from build_gl39 with nice 10.
R=/home/claude/main/build_gl39; cd $R/m || exit 1
for d in u ord; do [ -e $R/$d ] && { echo "rt39a.sh: $R/$d exists: move it aside first"; exit 1; }; done
for f in t0 t0.calls t1 t1.calls t2 t2.calls; do [ -e $R/lay/$f ] && { echo "rt39a.sh: $R/lay/$f exists: move lay aside"; exit 1; }; done
nice -n 10 env PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
nice -n 10 sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1 &
T=$R/cfg/mgs2_gl39pgo.cfg
TCFG=$T nice -n 10 sh scripts/pgo.sh trace $R/u t0 16001 17501 26901 27101 29601 29701 30201 30601 > $R/trace_t0.out 2>&1 &
TCFG=$T nice -n 10 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1.out 2>&1 &
TCFG=$T nice -n 10 sh scripts/pgo.sh trace $R/u t2 16101 30901 31001 > $R/trace_t2.out 2>&1 &
wait; echo "traces and pgochk done $(date -u +%H:%M:%S)"; tail -n 3 $R/pgochk_u.out | cut -c1-200
for t in t0 t1 t2; do case $t in t0) rm -rf "/home/claude/main/build_gl39/ms/t0" ;; t1) rm -rf "/home/claude/main/build_gl39/ms/t1" ;; t2) rm -rf "/home/claude/main/build_gl39/ms/t2" ;; esac
  [ -s $R/lay/$t.it ] || { echo "rt39a.sh: no trace $t"; exit 1; }; tail -n 2 $R/trace_$t.out | cut -c1-200; done
ls -la $R/lay/*.it; echo "rt39a.sh: done $(date -u +%H:%M:%S)"
