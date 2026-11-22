#!/bin/sh
# gC8_runs36.sh (gl36, the lead's 04:32 ask): an 8-turn cycle key in nat_stub.h's cycle parser, on the twin (build_gl36/tw = t36 with
# fixes A and B), the delivered base (native, in place, fast, keep 1, DZ X whole run), against hashref_gl36 (= gl35's c9ddb593):
#   c8r   MGS2_NAT_REUSE_CYCLE=1,1,1,1,0,0,0,0 (the 8-turn cycle on the reuse itself)
#   c8k   MGS2_NAT_REUSE_CYCLE=1,1,0,0 with MGS2_NAT_KEEP_CYCLE=1,1,1,1,0,0,0,0 (two cycle keys of different lengths together, as the
#         cull's 8-turn key will sit beside the reuse's 4-turn one)
cd /home/claude/main/build_gl36; R=/home/claude/main/build_gl36/cfg/hashref_gl36.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run.sh tw $t "$@" > g1r_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run c8r $N MGS2_NAT_KEEP=1 MGS2_NAT_REUSE_CYCLE=1,1,1,1,0,0,0,0 "$D" & run c8k $N MGS2_NAT_REUSE_CYCLE=1,1,0,0 MGS2_NAT_KEEP_CYCLE=1,1,1,1,0,0,0,0 "$D" & wait
echo done
