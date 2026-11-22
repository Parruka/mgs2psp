#!/bin/sh
# gl18_all.sh: gl18's build after its training run (task #179): sb18's pgo_gl.sh use, trace 0, trace 1 (one after the other: the disk
# holds the two traces), the two order searches side by side (seeds 1, 2); gl18_go.sh (pick the seed, final link, folder, checks)
# runs after it as its own job.  Order 2 starts once order 1 has made the merged traces (bases.txt is written after them).  The training run and both traces take pgo_gl.sh's XCFG: the rain's skips in turns (hide-only
# fffd21df, all on fffff9df), C4 and C4X off, the rain as Ffm chose.  Log: /home/claude/sbg18/drv.log.  Stops at the first step
# that fails.
X=/home/claude/sbg18; T=/home/claude/sb18; L=$X/drv.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
cd $T || exit 1
[ "$(ls $X/pg/gcda 2>/dev/null | wc -l)" -gt 0 ] || { st "stop: no counts"; exit 1; }
st "use start"; sh pgo_gl.sh use $X > $X/pgo_use.out 2>&1; st "use rc $?"
[ -f $X/u/EBOOT.PBP ] || { st "stop: no u/EBOOT.PBP"; exit 1; }
st "trace 0 start"; sh pgo_gl.sh trace $X 0 > $X/pgo_t0.out 2>&1; st "trace 0 rc $?"
[ -f $X/lay/done_t0 ] || { st "stop: trace 0"; exit 1; }
st "trace 1 start"; sh pgo_gl.sh trace $X 1 > $X/pgo_t1.out 2>&1; st "trace 1 rc $?"
[ -f $X/lay/done_t1 ] || { st "stop: trace 1"; exit 1; }
st "order start"; ( sh pgo_gl.sh order $X 1 > $X/order1.out 2>&1; st "order 1 rc $?" ) &
until [ -f $X/lay/bases.txt ] || grep -q 'order 1 rc' $L; do sleep 5; done; ( sh pgo_gl.sh order $X 2 > $X/order2.out 2>&1; st "order 2 rc $?" ) &
wait
st "orders done"
