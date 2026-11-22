#!/bin/sh
# g1_runs34.sh (gl34 G1): the x86 twin of t34 (build_gl34/tw) to vb 37,800, every run against gl34's hashref (= gl33's, c9ddb593;
# g31f against gl31's).  The keys: native in every turn and the in-place rule in every turn (gl34's cfg), then
#   p     plain (= gl33's in-place arm)                         f     the fast front end in every turn, the kept transforms checked
#   dx    the delivered keys: MGS2_NAT_FAST_CYCLE=1,1,0,0 and DZ X in the whole run (the emulator's digest reference)
#   fc2   the other order of turns: MGS2_NAT_FAST_CYCLE=0,1,1,0  lw2p  the PSP's late walk (plan, copy, view), plain
#   (lw2f: the late walk, fast, the hinted pointers checked: run apart, tw_lw2f.out)
#   g31f  gl31's route (the builder in two turns of four, MGS2_NAT_CYCLE=0,0,3,3) with the fast front end in the native turns
#   need  fast in every turn with MGS2_SB_NEED=1 (as the PSP's cfg: a converter call decodes only what it reads; the fast groups skip
#         xl_vm_need);  needp  the same, plain
cd /home/claude/main/build_gl34; R=/home/claude/main/build_gl34/cfg/hashref_gl34.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1"
REF=$R sh tw_run.sh tw p $N MGS2_NAT_STCHK=8 > g1r_p.out 2>&1 &
REF=$R sh tw_run.sh tw f $N MGS2_NAT_FAST=1 MGS2_NAT_XMCHK=1 > g1r_f.out 2>&1 &
REF=$R sh tw_run.sh tw dx $N MGS2_NAT_FAST_CYCLE=1,1,0,0 MGS2_NAT_XMCHK=1 "MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X" > g1r_dx.out 2>&1 &
wait
REF=$R sh tw_run.sh tw fc2 $N MGS2_NAT_FAST_CYCLE=0,1,1,0 > g1r_fc2.out 2>&1 &
REF=$R sh tw_run.sh tw lw2p $N MGS2_NAT_LW=2 > g1r_lw2p.out 2>&1 &
REF=/home/claude/main/build_gl31/deliver/hashref.txt sh tw_run.sh tw g31f MGS2_NAT_CYCLE=0,0,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_STCHK=8 > g1r_g31f.out 2>&1 &
wait
REF=$R sh tw_run.sh tw need $N MGS2_NAT_FAST=1 MGS2_SB_NEED=1 > g1r_need.out 2>&1 &
REF=$R sh tw_run.sh tw needp $N MGS2_SB_NEED=1 > g1r_needp.out 2>&1 &
wait
echo done
