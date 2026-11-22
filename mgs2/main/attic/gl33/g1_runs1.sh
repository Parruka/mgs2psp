#!/bin/sh
# g1_runs1.sh: G1's first three twin runs in parallel (reference: gl32's hashref, native every turn)
cd /home/claude/main/build_gl33; R=/home/claude/main/build_gl32/deliver/hashref.txt
REF=$R sh tw_run.sh tw a3 MGS2_NAT_CYCLE=3,3,3,3 > g1_a3.out 2>&1 &
REF=$R sh tw_run.sh tw ai MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL_CYCLE=0,0,1,1 MGS2_NAT_STCHK=8 > g1_ai.out 2>&1 &
REF=$R sh tw_run.sh tw dx MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL_CYCLE=0,0,1,1 "MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X" > g1_dx.out 2>&1 &
wait; echo done
