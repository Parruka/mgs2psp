#!/bin/sh
# g1_runs2.sh: G1's late-walk runs (the PSP's plan / copy / view walk on the twin, MGS2_NAT_LW=2) with the rule in every turn and
# without it, and gl31's route (NAT 0,0,3,3) with the rule in the native turns
cd /home/claude/main/build_gl33; R=/home/claude/main/build_gl32/deliver/hashref.txt
REF=$R sh tw_run.sh tw lw2i MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_LW=2 MGS2_NAT_STCHK=8 > g1_lw2i.out 2>&1 &
REF=$R sh tw_run.sh tw lw2p MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_LW=2 > g1_lw2p.out 2>&1 &
REF=/home/claude/main/build_gl31/deliver/hashref.txt sh tw_run.sh tw g31 MGS2_NAT_CYCLE=0,0,3,3 MGS2_NAT_INPL_CYCLE=0,0,1,1 MGS2_NAT_STCHK=8 > g1_g31.out 2>&1 &
wait; echo done
