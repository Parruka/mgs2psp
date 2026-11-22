#!/bin/sh
# rh31.sh <tag> <last vb> [VAR=value ...] (gl31 G2; build_gl30n/rh30.sh): one run of the read-hash twin (xt31.sh), the design's recipe (scratchpad
# native/xt/rh.sh: gl28's game settings, the PSP cfg's turn schedule): every vblank's "rh:" line; xt31/out_<tag>.log
M=/home/claude/main; X=$M/build_gl31/xt31; MG=/mnt/project-files/mgs2
PAK="/mnt/user-data/uploads/mg2 port/Builds/MGS2T_gl18/mgs2disc.pak"; PADF=$MG/play/pad_w01af.txt; SL=/home/claude/mgs2/SLES_503.83
t=$1; n=$2; shift 2; cd $X || exit 1; s=$(date +%s)
SK=$(python3 -I $MG/tools/dev/rl_skip.py ./boot 2>/dev/null); G0=$(python3 -I $MG/tools/dev/rl_skip.py ./boot S_w00a_P333380 2>/dev/null)
echo "skip $SK g0 $G0" > rs_$t.txt
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=fffff9df MGS2_READ_HASH=1 MGS2_READ_SKIP=$SK MGS2_READ_G0=$G0 "$@" nice -n 10 ./boot "$SL" $n > out_$t.log 2>&1
echo "$t rc $? $(( $(date +%s) - s )) s" > rc_$t.txt
