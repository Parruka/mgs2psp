#!/bin/sh
# g1_runs36.sh (gl36 G1): the x86 twin of t36 (build_gl36/tw) to vb 37,800, every run against gl36's hashref (= gl35's, c9ddb593).
# The base in every run: native, in place, fast and MGS2_NAT_KEEP=1 (the drawn-areas keep) in every turn.  Then
#   p     no reuse (the transforms checked, the static streams checked over 8 vblanks)    r    MGS2_NAT_REUSE=1 in every turn
#   rc    reuse with the check mode (MGS2_NAT_RCHK=1)
#   px    no reuse, DZ X in the whole run          rx    the delivered keys MGS2_NAT_REUSE_CYCLE=1,1,0,0, DZ X whole run
#   rx2   the other order MGS2_NAT_REUSE_CYCLE=0,0,1,1, DZ X whole run (the emulator's digest reference is rx)
#   lw2p / lw2r   the PSP's late walk (MGS2_NAT_LW=2), no reuse / reuse      needp / needr   MGS2_SB_NEED=1 (as the PSP), no reuse / reuse
#   rk15  reuse with MGS2_NAT_KEEP=15 (every keep with the reuse)            rnf   MGS2_NAT_FAST_CYCLE=1,1,0,0 with reuse in every turn
cd /home/claude/main/build_gl36; R=/home/claude/main/build_gl36/cfg/hashref_gl36.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"
run() { t=$1; shift; REF=${RF:-$R} sh tw_run.sh tw $t "$@" > g1r_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run p $N MGS2_NAT_XMCHK=1 MGS2_NAT_STCHK=8 & run r $N MGS2_NAT_REUSE=1 & wait
run rc $N MGS2_NAT_REUSE=1 MGS2_NAT_RCHK=1 & run rx $N MGS2_NAT_REUSE_CYCLE=1,1,0,0 "$D" & wait
run px $N "$D" & run rx2 $N MGS2_NAT_REUSE_CYCLE=0,0,1,1 "$D" & wait
run lw2p $N MGS2_NAT_LW=2 & run lw2r $N MGS2_NAT_LW=2 MGS2_NAT_REUSE=1 & wait
run needp $N MGS2_SB_NEED=1 & run needr $N MGS2_SB_NEED=1 MGS2_NAT_REUSE=1 & wait
run rk15 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=15 MGS2_NAT_REUSE=1 &
run rnf MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST_CYCLE=1,1,0,0 MGS2_NAT_KEEP=1 MGS2_NAT_REUSE=1 & wait
echo done
