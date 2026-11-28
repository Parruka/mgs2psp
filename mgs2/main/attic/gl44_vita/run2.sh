#!/bin/sh
# run.sh <tag> <opt2 hex> [extra env...]: the twin's route i run (scripts/twin.sh's settings) with MGS2_OPT2 = every cycle word = <opt2>,
# MGS2_HALF=16 and the act log; no reference check
M=/home/claude/main; . $M/scripts/env.sh
t=$1; o=$2; shift 2; X=$B/x86; cd $X || exit 1; s=$(date +%s)
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 \
  MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff \
  MGS2_OPT2_CYCLE=$o,$o MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=$o MGS2_HASH_DENSE=15700:16010 ${NOHALF:-MGS2_HALF=10} MGS2_ACTLOG=/home/claude/v44/act_$t.txt:${AL:-0:37800} "$@" \
  ${BOOT:-./boot} "$SL" ${END:-37800} > /home/claude/v44/out_$t.log 2>&1
echo "run $t rc $? $(( $(date +%s) - s )) s"
