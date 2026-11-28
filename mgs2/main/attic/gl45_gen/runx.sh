#!/bin/sh
# gl45 batch 3: runx.sh <tag> <boot binary> [extra env...]: the twin's route i with batch 1's settings (scripts/twin.sh's, the reference's
# OPT2 word ffbff9df, the act log through MGS2_HALF=10): out/out_<tag>.log, out/act_<tag>.txt.  END=<vb> stops earlier.
M=/home/claude/main; . $M/scripts/env.sh
t=$1; b=$2; shift 2; X=$B/x86; cd $X || exit 1; s=$(date +%s); o=ffbff9df; O=/home/claude/g45/out
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 \
  MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff \
  MGS2_OPT2_CYCLE=$o,$o MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=$o MGS2_HASH_DENSE=15700:16010 MGS2_HALF=10 MGS2_ACTLOG=$O/act_$t.txt:${AL:-0:37800} "$@" \
  ./$b "$SL" ${END:-37800} > $O/out_$t.log 2>&1
echo "runx $t ($b) rc $? $(( $(date +%s) - s )) s"
