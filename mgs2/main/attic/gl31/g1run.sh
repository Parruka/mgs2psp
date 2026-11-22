#!/bin/sh
# g1run.sh (gl31 G1): the three twin runs of build_gl31/tw (tw31.sh), in parallel:
#  a1: gl30's own G1 command (scripts/twin.sh's default schedule, cycle from vb 1000; MGS2_HALF_CYCLE b x 8; MGS2_GL_DZ X), NAT key absent
#  a2: the PSP cfg's schedule (tw_run.sh: MGS2_OPT_CYCLE=24000:100 x 8, cut b, DZ X), NAT key absent
#  b : a2 + MGS2_NAT_CYCLE=0,0,3,3 (the delivered key): hashref_b.txt is the build's hashref.txt
M=/home/claude/main; R=$M/build_gl31
( B=$R/tw REF=$M/cfg/hashref_g22b.txt sh $M/scripts/twin.sh a1 MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b "MGS2_GL_DZ=29490:31400:X" > $R/tw/twin_a1.out 2>&1 ) &
( sh $R/tw_run.sh tw a2 > $R/tw/twin_a2.out 2>&1 ) &
( sh $R/tw_run.sh tw b MGS2_NAT_CYCLE=0,0,3,3 > $R/tw/twin_b.out 2>&1 ) &
wait
