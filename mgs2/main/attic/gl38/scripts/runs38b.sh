#!/bin/sh
# runs38b.sh (gl38, the final code's twin proof, batch b): arms on build_gl38/tw (tw38.sh, built 14:22) against hashref_gl38 (= gl37's, c9ddb593).
#  g37: gl37's cfg keys (no gl38 key: bound, F1, rain all off)
#  sch: the schedule (tools/sched38.py keys: MGS2_BOUND_CYCLE + MGS2_NAT_F1_CYCLE) + MGS2_GL_RAIN_NEAR=1
#  f1w: gl37's keys + MGS2_NAT_F1=1 (F1 in every turn)
#  f1c: F1's check mode: MGS2_NAT_F1CHK=1 with the re-use recording every run (MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1)
cd /home/claude/main/build_gl38; R=/home/claude/main/build_gl38/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_REUSE_CYCLE=1,1,0,0 MGS2_NAT_CULL=1"
K=$(python3 -I tools/sched38.py keys | tr '\n' ' ')
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run38.sh tw $t "$@" > g_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run g37 $X "$D" & run sch $X $K MGS2_GL_RAIN_NEAR=1 "$D" &
run f1w $X MGS2_NAT_F1=1 "$D" & run f1c $N MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1 MGS2_NAT_F1CHK=1 MGS2_NAT_CULL=1 "$D" & wait
echo done
