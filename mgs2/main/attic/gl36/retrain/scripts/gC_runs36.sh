#!/bin/sh
# gC_runs36.sh (gl36 retrain, G1 on the tree with the cull: t36 = t37d, t36_files_C.md5): the twin rebuilt (tw36.sh -> tw), then every
# arm to vb 37,800 against hashref_gl36 (= c9ddb593), two at a time under nice 10.  Tags end in C (the pre-cull gated logs stay).
#  gl36's G1 arms (cull off): pC rC rcC rxC pxC rx2C lw2pC lw2rC needpC needrC rk15C rnfC (as g1_runs36.sh)
#  the cull's arms on the delivered base (rx's keys): dxC (the delivered cfg: + MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0, = cy8), onC
#  (MGS2_NAT_CULL=1), chkC (+ MGS2_NAT_CULLCHK=1), l3offC / l3chkC (MGS2_SB_LANE=11; + cull and its check), cf0offC / cf0chkC
#  (clip-fast off: MGS2_OPT=fbffffff in every turn; + cull and its check), bothC (reuse and cull in every turn with both check modes)
cd /home/claude/main/build_gl36; R=/home/claude/main/build_gl36/cfg/hashref_gl36.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_REUSE_CYCLE=1,1,0,0"
CF0="MGS2_OPT=fbffffff MGS2_OPT_CYCLE=24000:100:fbffffff,fbffffff,fbffffff,fbffffff,fbffffff,fbffffff,fbffffff,fbffffff"
nice -n 10 sh tw36.sh > tw36_C.out 2>&1; tail -n 2 tw36_C.out
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run.sh tw $t "$@" > g1r_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
run pC $N MGS2_NAT_XMCHK=1 MGS2_NAT_STCHK=8 & run rC $N MGS2_NAT_REUSE=1 & wait
run rcC $N MGS2_NAT_REUSE=1 MGS2_NAT_RCHK=1 & run rxC $X "$D" & wait
run dxC $X MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0 "$D" & run onC $X MGS2_NAT_CULL=1 "$D" & wait
run chkC $X MGS2_NAT_CULL=1 MGS2_NAT_CULLCHK=1 "$D" & run bothC $N MGS2_NAT_REUSE=1 MGS2_NAT_RCHK=1 MGS2_NAT_CULL=1 MGS2_NAT_CULLCHK=1 "$D" & wait
run l3offC $X MGS2_SB_LANE=11 "$D" & run l3chkC $X MGS2_SB_LANE=11 MGS2_NAT_CULL=1 MGS2_NAT_CULLCHK=1 "$D" & wait
run cf0offC $X $CF0 "$D" & run cf0chkC $X $CF0 MGS2_NAT_CULL=1 MGS2_NAT_CULLCHK=1 "$D" & wait
run pxC $N "$D" & run rx2C $N MGS2_NAT_REUSE_CYCLE=0,0,1,1 "$D" & wait
run lw2pC $N MGS2_NAT_LW=2 & run lw2rC $N MGS2_NAT_LW=2 MGS2_NAT_REUSE=1 & wait
run needpC $N MGS2_SB_NEED=1 & run needrC $N MGS2_SB_NEED=1 MGS2_NAT_REUSE=1 & wait
run rk15C MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=15 MGS2_NAT_REUSE=1 &
run rnfC MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST_CYCLE=1,1,0,0 MGS2_NAT_KEEP=1 MGS2_NAT_REUSE=1 & wait
echo done
