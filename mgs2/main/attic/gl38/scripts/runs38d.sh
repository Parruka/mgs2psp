#!/bin/sh
# runs38d.sh A|B (gl38, the FINAL code's twin proof: the twin built 14:44 with F1 planned by segments and the refined check mode), against
# hashref_gl38 (= gl37's, c9ddb593).
#  A: g37d  gl37's cfg keys alone (no gl38 key)
#     schd  the cfg's keys: the schedule (tools/sched38.py keys: MGS2_BOUND_CYCLE + MGS2_NAT_F1_CYCLE) + MGS2_GL_RAIN_NEAR=1
#     f1cd  F1's check mode: MGS2_NAT_F1CHK=1 with the re-use recording every run (MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1)
#     rcut  gl37's keys + the rain cut in the whole run (MGS2_GL_RAIN_CUT=1 MGS2_GL_RAIN_NEAR=1) + MGS2_NAT_F1=1
#  B: f1wd  gl37's keys + MGS2_NAT_F1=1 (F1 in every turn)
#     rotA / rotB  every kind in turn (kinds 1..7,0 / 4..7,0..3, repeated to vb 36,800) + MGS2_GL_RAIN_NEAR=1 + MGS2_NAT_F1=1
#     schk  the cfg's keys + MGS2_NAT_F1CHK=1 (F1's plan checked against the notes where the re-use records, in the schedule's turns)
cd /home/claude/main/build_gl38; R=/home/claude/main/build_gl38/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_REUSE_CYCLE=1,1,0,0 MGS2_NAT_CULL=1"
K=$(python3 -I tools/sched38.py keys | tr '\n' ' ')
rep() { s=; i=0; while [ $i -lt 16 ]; do s="$s$1"; i=$((i + 1)); done; echo $s; }
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run38.sh tw $t "$@" > g_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
case "$1" in
A) run g37d $X "$D" & run schd $X $K MGS2_GL_RAIN_NEAR=1 "$D" &
   run f1cd $N MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1 MGS2_NAT_F1CHK=1 MGS2_NAT_CULL=1 "$D" &
   run rcut $X MGS2_GL_RAIN_CUT=1 MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1=1 "$D" & wait ;;
B) run f1wd $X MGS2_NAT_F1=1 "$D" & run rotA $X MGS2_BOUND_CYCLE=$(rep 12345670) MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1=1 "$D" &
   run rotB $X MGS2_BOUND_CYCLE=$(rep 45670123) MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1=1 "$D" & run schk $X $K MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1CHK=1 "$D" & wait ;;
*) echo "runs38d.sh A|B"; exit 1 ;; esac
for t in $(ls g_*.out | sed 's/^g_//; s/\.out$//'); do [ g_$t.out -nt runs38d.sh ] && { echo "$t: $(grep -a 'reference lines\|equals\|differ' g_$t.out | tr '\n' ' ' | cut -c1-200)"; echo "  $(zcat tw/x86/out_$t.log.gz | grep -a '^natf1: totals' | cut -c1-460)"; }; done
echo "done $1"
