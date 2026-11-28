#!/bin/sh
# runs38e.sh (gl38, the twin with the REVISED schedule of 16:20 (tools/sched38.py: the rain wait's turns rearranged), the same twin
# binary as runs38d.sh): sch2 the cfg's keys; sck2 the cfg's keys + MGS2_NAT_F1CHK=1.  Against hashref_gl38 (c9ddb593).
cd /home/claude/main/build_gl38; R=/home/claude/main/build_gl38/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_REUSE_CYCLE=1,1,0,0 MGS2_NAT_CULL=1"
K=$(python3 -I tools/sched38.py keys | tr '\n' ' ')
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run38.sh tw $t "$@" > g_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run sch2 $X $K MGS2_GL_RAIN_NEAR=1 "$D" & run sck2 $X $K MGS2_GL_RAIN_NEAR=1 MGS2_NAT_F1CHK=1 "$D" & wait
for t in sch2 sck2; do echo "$t: $(grep -a 'reference lines\|equals\|differ' g_$t.out | tr '\n' ' ' | cut -c1-200)"; echo "  $(zcat tw/x86/out_$t.log.gz | grep -a '^natf1: totals' | cut -c1-460)"; done
echo "runs38e.sh: done $(date -u +%H:%M:%S)"
