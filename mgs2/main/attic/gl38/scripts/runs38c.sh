#!/bin/sh
# runs38c.sh (gl38, the final code's twin proof, batch c: the twin built 14:5x with F1 planned by segments): against hashref_gl38 (c9ddb593).
#  schc: the schedule (tools/sched38.py keys) + MGS2_GL_RAIN_NEAR=1 (= the cfg's keys)
#  f1wc: gl37's keys + MGS2_NAT_F1=1 (F1 in every turn)
#  f1cc: F1's check mode: MGS2_NAT_F1CHK=1 with the re-use recording every run (MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1)
#  second round (ARG 2): rcut: gl37's keys + MGS2_GL_RAIN_CUT=1 MGS2_GL_RAIN_NEAR=1 (the rain cut in the whole run) + MGS2_NAT_F1=1;
#  rotA / rotB: every kind in turn (kinds 1..7,0 / 4..7,0..3 repeated to vb 36,800) + MGS2_GL_RAIN_NEAR=1 + MGS2_NAT_F1=1;
#  g37c: gl37's keys alone
cd /home/claude/main/build_gl38; R=/home/claude/main/build_gl38/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_REUSE_CYCLE=1,1,0,0 MGS2_NAT_CULL=1"
K=$(python3 -I tools/sched38.py keys | tr '\n' ' ')
rep() { s=; i=0; while [ $i -lt 16 ]; do s="$s$1"; i=$((i + 1)); done; echo $s; }
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run38.sh tw $t "$@" > g_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
if [ "$1" = 2 ]; then
  run rcut $X MGS2_GL_RAIN_CUT=1 MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1=1 "$D" & run rotA $X MGS2_BOUND_CYCLE=$(rep 12345670) MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1=1 "$D" &
  run rotB $X MGS2_BOUND_CYCLE=$(rep 45670123) MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1=1 "$D" & run g37c $X "$D" & wait
else
  run schc $X $K MGS2_GL_RAIN_NEAR=1 "$D" & run f1wc $X MGS2_NAT_F1=1 "$D" &
  run f1cc $N MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1 MGS2_NAT_F1CHK=1 MGS2_NAT_CULL=1 "$D" & wait
fi
echo done
