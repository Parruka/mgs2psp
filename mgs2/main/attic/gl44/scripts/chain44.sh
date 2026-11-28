#!/bin/sh
# chain44.sh (gl44): after pgo_a44.sh, its gates (rtb and pic trainings rc 0; game, w00a, w01a against hashref_gl44pgo0 0 differ; the
# counts copied), then rt44a.sh (use build u, pgochk, traces t0-t3, model mu) and its gates (every trace and mu 0 differ against its
# reference), then rt44b.sh (merge, caps, order search).  One heavy job at a time; stops at the first failed gate.
R=/home/claude/main/build_gl44; cd $R || exit 1
until grep -q 'pgo_a44.sh: done' pgo_a44.out; do sleep 30; done
ok=1; grep -q 'train_rtb rc 0' pgo_a44.out || ok=0; grep -q 'train_pic rc 0' pgo_a44.out || ok=0
n=$(grep -A2 'against hashref_gl44pgo0' pgo_a44.out | grep -c 'compared .*: 0 differ'); [ "$n" = 3 ] || ok=0
g=$(ls gcda/*.gcda 2>/dev/null | wc -l); [ "$g" -ge 5 ] || ok=0
echo "chain44: trainings: rtb/pic rc 0, against pgo0 $n of 3 with 0 differ, gcda files $g: ok=$ok $(date -u +%H:%M:%S)"; [ $ok = 1 ] || exit 1
sh rt44a.sh > rt44a.out 2>&1
grep -q 'rt44a.sh: done' rt44a.out || { echo "chain44: rt44a.sh did not finish"; tail -n 5 rt44a.out; exit 1; }
grep -q 'pgochk rc 0' rt44a.out || { echo "chain44: pgochk failed"; exit 1; }
for f in trace_t0 trace_t1 trace_t2 trace_t3 model_mu; do grep -q 'compared .*: 0 differ' $f.out || { echo "chain44: $f differs from its reference"; grep 'compared' $f.out; exit 1; }; done
echo "chain44: rt44a gates ok $(date -u +%H:%M:%S)"
sh rt44b.sh > rt44b.out 2>&1; echo "chain44: rt44b.sh rc $? $(date -u +%H:%M:%S)"; tail -n 3 rt44b.out | cut -c1-200
