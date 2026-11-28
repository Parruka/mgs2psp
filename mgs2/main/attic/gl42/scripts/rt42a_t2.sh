#!/bin/sh
# rt42a_t2.sh (gl42, 05:10): rt42a.sh's trace t2 again with its windows in ascending order (the first run's list was not sorted:
# pgo.sh stopped the run at 30,711, so 3 of its 8 windows were never traced).  nice 19, one heavy job.
R=/home/claude/main/build_gl42; cd $R/m || exit 1
[ -e $R/lay/t2.it ] && { echo "rt42a_t2.sh: $R/lay/t2.it exists"; exit 1; }
T=$R/cfg/mgs2_gl42pgo.cfg
gd() { [ -d /home/claude/main/build_gl42/ms/$1/.ppsspp ] && rm -rf "/home/claude/main/build_gl42/ms/$1"; }
TCFG=$T nice -n 19 sh scripts/pgo.sh trace $R/u t2 16101 26901 29701 30301 30701 30801 30901 31101 > $R/trace_t2.out 2>&1; echo "t2 rc $? $(date -u +%H:%M:%S)"; gd t2
[ -s $R/lay/t2.it ] || { echo "rt42a_t2.sh: no trace t2"; exit 1; }; tail -n 2 $R/trace_t2.out | cut -c1-200
ls $R/lay/t2 $R/lay/t2.[1-7] 2>&1 | cut -c1-120
