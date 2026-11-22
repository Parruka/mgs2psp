#!/bin/sh
# rh_all36.sh (gl36 G2): the read-hash runs (xt36.sh, rh36.sh) to vb 31,400, base settings of gl36 in every run (native, in place, fast,
# MGS2_NAT_KEEP=1): rp no reuse; rr MGS2_NAT_REUSE=1 in every turn; rrc the delivered keys (MGS2_NAT_REUSE_CYCLE=1,1,0,0).  Compared with
# gl35's rf (xt35/out_rf.log) and with each other (rhcmp.py).
cd /home/claude/main/build_gl36 || exit 1
N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
sh rh36.sh rp 31400 $N & sh rh36.sh rr 31400 $N MGS2_NAT_REUSE=1 & wait
sh rh36.sh rrc 31400 $N MGS2_NAT_REUSE_CYCLE=1,1,0,0
X=xt36; Y=/home/claude/main/build_gl35/xt35
for p in "$Y/out_rf.log $X/out_rp.log gl35 rf vs gl36 rp" "$X/out_rp.log $X/out_rr.log rp vs rr (reuse in every turn)" "$X/out_rp.log $X/out_rrc.log rp vs rrc (reuse by turns)" "$Y/out_rkc.log $X/out_rrc.log gl35 rkc vs gl36 rrc"; do
  set -- $p; a=$1; b=$2; shift 2; echo "== $*"; python3 -I rhcmp.py $a $b; echo "rc $?"; done
cat xt36/rc_*.txt; echo done
