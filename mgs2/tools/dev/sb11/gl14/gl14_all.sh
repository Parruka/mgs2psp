#!/bin/sh
# gl14_all.sh: gl14's build after the gen step (task #136): sb11's pgo_gl.sh run, use, trace 0, trace 1 (one after the other: the disk
# holds the two traces), the two order searches side by side (seeds 1, 2); gl14_go.sh (pick the seed, final link, folder, checks) runs after it as its own job.
# Log: /home/claude/sbg14/drv.log.  Stops at the first step that fails.
X=/home/claude/sbg14; T=/home/claude/sb11; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl14; L=$X/drv.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
cd $T || exit 1
st "run start"; sh pgo_gl.sh run $X > $T/pgo_run14.out 2>&1; st "run rc $?: $(ls $X/pg/gcda 2>/dev/null | wc -l) gcda"
[ "$(ls $X/pg/gcda 2>/dev/null | wc -l)" -gt 0 ] || { st "stop: no counts"; exit 1; }
st "use start"; sh pgo_gl.sh use $X > $T/pgo_use14.out 2>&1; st "use rc $?"
[ -f $X/u/EBOOT.PBP ] || { st "stop: no u/EBOOT.PBP"; exit 1; }
st "trace 0 start"; sh pgo_gl.sh trace $X 0 > $T/pgo_t0_14.out 2>&1; st "trace 0 rc $?"
[ -f $X/lay/done_t0 ] || { st "stop: trace 0"; exit 1; }
st "trace 1 start"; sh pgo_gl.sh trace $X 1 > $T/pgo_t1_14.out 2>&1; st "trace 1 rc $?"
[ -f $X/lay/done_t1 ] || { st "stop: trace 1"; exit 1; }
st "order start"; ( sh pgo_gl.sh order $X 1 > $X/order1.out 2>&1; st "order 1 rc $?" ) &
sleep 60; ( sh pgo_gl.sh order $X 2 > $X/order2.out 2>&1; st "order 2 rc $?" ) &
wait
st "orders done"   # gl14_go.sh runs as its own job (the two-hour limit of one background job)
