#!/bin/sh
# runs38a.sh (gl38, the bound turns' first twin proof): arms on build_gl38/tw (tw38.sh) against hashref_gl38 (= gl37's, c9ddb593).
#  b0p: gl37's keys (no bound cycle) + the probe of the rain owners' calls (MGS2_BD_PROBE=100: the scan lists the owners' blocks, nothing left out)
#  bA / bB / bC: every kind in turn from vb 24,000, three phases (kinds 1..7,0 / 5..7,0..4 / 3..7,0..2 repeated)
cd /home/claude/main/build_gl38; R=/home/claude/main/build_gl38/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_REUSE_CYCLE=1,1,0,0 MGS2_NAT_CULL=1"
rep() { s=; i=0; while [ $i -lt 16 ]; do s="$s$1"; i=$((i + 1)); done; echo $s; }
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run38.sh tw $t "$@" > g_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run b0p $X MGS2_BD_PROBE=100 "$D" & run bA $X MGS2_BOUND_CYCLE=$(rep 12345670) "$D" &
run bB $X MGS2_BOUND_CYCLE=$(rep 56701234) "$D" & run bC $X MGS2_BOUND_CYCLE=$(rep 34567012) "$D" & wait
echo done
