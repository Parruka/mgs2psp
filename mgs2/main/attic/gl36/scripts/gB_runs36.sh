#!/bin/sh
# gB_runs36.sh (gl36 look a, fix B: the XmeRec/MpRec nru packing and the reuse's last writers moved into the table's block): the twin
# (build_gl36/tw, rebuilt) in the arms that reach the change: p (no reuse) and rx (the delivered keys) as fix A's gate, plus r (reuse in
# every turn) and rc (the check mode: it compares the S bits the last writers make).  Each against hashref_gl36 (= gl35's c9ddb593).
cd /home/claude/main/build_gl36; R=/home/claude/main/build_gl36/cfg/hashref_gl36.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run.sh tw $t "$@" > g1r_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run pB $N MGS2_NAT_XMCHK=1 MGS2_NAT_STCHK=8 & run rxB $N MGS2_NAT_REUSE_CYCLE=1,1,0,0 "$D" & wait
run rB $N MGS2_NAT_REUSE=1 & run rcB $N MGS2_NAT_REUSE=1 MGS2_NAT_RCHK=1 & wait
echo done
