#!/bin/sh
# run.sh <bindir> <tag> <last vb> [VAR=value ...]: route i on the twin with gl28's game settings (as native/x86e/run.sh) and the picture
# translator live (as main/scripts/twin.sh: one walk, lanes 1-4, frames, gl28's rain settings); log $S/out_<tag>.log; hashes vs hashref_g22b
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/n0; MG=/mnt/project-files/mgs2; M=/home/claude/main
PAK="/mnt/user-data/uploads/mg2 port/Builds/MGS2T_gl18/mgs2disc.pak"; PADF=$MG/play/pad_w01af.txt; SL=/home/claude/mgs2/SLES_503.83
B=$1; t=$2; n=$3; shift 3; cd $B || exit 1; s=$(date +%s)
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=fffff9df MGS2_HASH_REF=$M/cfg/hashref_g22b.txt MGS2_HASH_REF_DUMP=$S/split_$t \
  MGS2_XLIVE=0:$n MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$n MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 \
  MGS2_SB_LANE=15 MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 \
  "$@" ./boot "$SL" $n > $S/out_$t.log 2>&1
echo "$t rc $? $(( $(date +%s) - s )) s" | tee $S/rc_$t.txt
python3 -I $M/tools/refcmp.py --partial $M/cfg/hashref_g22b.txt $S/out_$t.log | tail -3
