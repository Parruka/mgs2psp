#!/bin/sh
# runs39d.sh (gl39, on build_gl39/tw2): F1's check mode as gl38 ran it (MGS2_NAT_F1CHK=1 with the re-use recording every run:
# MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1): f1c no kinds; f1s gl39's schedule (the emulator's check cfg's keys).
cd /home/claude/main/build_gl39; R=/home/claude/main/build_gl39/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; K=$(python3 -I tools/sched39.py keys | tr '\n' ' ')
C="MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1 MGS2_NAT_F1CHK=1 MGS2_NAT_CULL=1"
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run39.sh tw2 $t "$@" > g2_$t.out 2>&1; gzip -f tw2/x86/out_$t.log; }
run f1c $N $C "$D" & run f1s $N $C "$D" $K & wait
for t in f1c f1s; do echo "$t: $(grep -a 'reference lines\|equals\|differ' g2_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  $(zcat tw2/x86/out_$t.log.gz | grep -a '^natf1: totals' | cut -c1-110) ... $(zcat tw2/x86/out_$t.log.gz | grep -a '^natf1: totals' | grep -o 'check mode.*' | cut -c1-200)"
  echo "  $(zcat tw2/x86/out_$t.log.gz | grep -a 'NAT PROBLEMS\|^nat: PROBLEMS\|reuse totals' | cut -c1-400 | tr '\n' '|')"; done
echo "runs39d.sh: done $(date -u +%H:%M:%S)"
