#!/bin/sh
# g1_runs35.sh (gl35 G1): the x86 twin of t35 (build_gl35/tw) to vb 37,800, every run against gl35's hashref (= gl33's, c9ddb593;
# g31k against gl31's).  The keys: native, in place and the fast front end in every turn (gl35's cfg), then
#   f     no keeps (gl34's fast arm; the kept transforms checked)      k     MGS2_NAT_KEEP=15 in every turn
#   kc    MGS2_NAT_KEEP=15 with the check mode (MGS2_NAT_KCHK=1)       k1 k2 k4 k8   one keep bit each, in every turn
#   dxp   no keeps, DZ X in the whole run                              dx    the delivered keys MGS2_NAT_KEEP_CYCLE=15,15,0,0, DZ X whole run
#   dx2   the other order MGS2_NAT_KEEP_CYCLE=0,0,15,15, DZ X whole run (the emulator's digest reference is dx)
#   lw2p / lw2k   the PSP's late walk (MGS2_NAT_LW=2), no keeps / keeps 15     needp / needk   MGS2_SB_NEED=1 (as the PSP), no keeps / keeps 15
#   fk    MGS2_NAT_FAST_CYCLE=1,1,0,0 with MGS2_NAT_KEEP=15 (keep bits left standing in turns with no fast group)
#   g31k  gl31's route (the builder in two turns of four, MGS2_NAT_CYCLE=0,0,3,3) with fast and keeps 15 in the native turns
cd /home/claude/main/build_gl35; R=/home/claude/main/build_gl35/cfg/hashref_gl35.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"
run() { t=$1; shift; REF=${RF:-$R} sh tw_run.sh tw $t "$@" > g1r_$t.out 2>&1; }
run f $N MGS2_NAT_XMCHK=1 MGS2_NAT_STCHK=8 & run k $N MGS2_NAT_KEEP=15 & wait
run kc $N MGS2_NAT_KEEP=15 MGS2_NAT_KCHK=1 & run dx $N MGS2_NAT_KEEP_CYCLE=15,15,0,0 "$D" & wait
run dxp $N "$D" & run dx2 $N MGS2_NAT_KEEP_CYCLE=0,0,15,15 "$D" & wait
run k1 $N MGS2_NAT_KEEP=1 & run k2 $N MGS2_NAT_KEEP=2 & wait
run k4 $N MGS2_NAT_KEEP=4 & run k8 $N MGS2_NAT_KEEP=8 & wait
run lw2p $N MGS2_NAT_LW=2 & run lw2k $N MGS2_NAT_LW=2 MGS2_NAT_KEEP=15 & wait
run needp $N MGS2_SB_NEED=1 & run needk $N MGS2_SB_NEED=1 MGS2_NAT_KEEP=15 & wait
run fk MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST_CYCLE=1,1,0,0 MGS2_NAT_KEEP=15 &
RF=/home/claude/main/build_gl31/deliver/hashref.txt run g31k MGS2_NAT_CYCLE=0,0,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=15 MGS2_NAT_STCHK=8 & wait
echo done
