#!/bin/sh
# run45.sh <tag> <boot binary> <MGS2_N45 hex> [extra env...]: the twin's route i (scripts/twin.sh's settings, the reference's OPT2 word
# ffbff9df = gl43's B and the gl44 study off), the act log (MGS2_HALF=10 routes every act through it), the native forms of <MGS2_N45>.
M=/home/claude/main; . $M/scripts/env.sh
t=$1; b=$2; n=$3; shift 3; X=$B/x86; cd $X || exit 1; s=$(date +%s); o=ffbff9df
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 \
  MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff \
  MGS2_OPT2_CYCLE=$o,$o MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=$o MGS2_HASH_DENSE=15700:16010 MGS2_HALF=10 MGS2_ACTLOG=/home/claude/n45/act_$t.txt:${AL:-0:37800} MGS2_N45=$n "$@" \
  ./$b "$SL" ${END:-37800} > /home/claude/n45/out_$t.log 2>&1
echo "run45 $t ($b, N45 $n) rc $? $(( $(date +%s) - s )) s"
