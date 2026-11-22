#!/bin/sh
# tw_part.sh <build subfolder> <tag> <vblanks> [VAR=value ...]: as tw_run.sh (main/scripts/twin.sh's settings with the PSP cfg's turn
# schedule), but to <vblanks> only and with no reference check: for traces and dumps.  Log: <subfolder>/x86/out_<tag>.log.
M=/home/claude/main; MG=/mnt/project-files/mgs2; R=$M/build_gl35; S=$1; t=$2; n=$3; shift 3
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df
cd $R/$S/x86 || exit 1
env MGS2_DISC="/mnt/user-data/uploads/mg2 port/Builds/MGS2T_gl18/mgs2disc.pak" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$(cat $MG/play/pad_w01af.txt)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 \
  MGS2_XLIVE_GX=0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_SB_LANE=15 MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 \
  MGS2_GL_RAIN_SPRITES=0 MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 MGS2_OPT2=fffff9df MGS2_HASH_DENSE=15700:16010 \
  MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b "MGS2_GL_DZ=29490:31400:X" "$@" \
  ./boot /home/claude/mgs2/SLES_503.83 $n > out_$t.log 2>&1
echo "rc $?"
