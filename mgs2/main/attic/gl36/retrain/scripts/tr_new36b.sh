#!/bin/sh
# tr_new36b.sh: tr_new36.sh's t0 and t1 again (the first try stopped at once: the old run's window files lay/t0.7.calls and
# lay/t1.7.calls, which pgo.sh waits for, were still there; they are kept in lay_old and were removed first)
R=/home/claude/main/build_gl36; cd $R/m || exit 1
nice -n 10 sh scripts/pgo.sh trace $R/u t0 16001 17501 26501 27801 29601 29801 30101 31101 > $R/trace_t0n.out 2>&1 &
TCFG=$R/cfg/mgs2_gl36pgo_t1.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1n.out 2>&1 &
wait; echo "traces done $(date -u +%H:%M:%S)"
