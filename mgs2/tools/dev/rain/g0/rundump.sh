#!/bin/sh
# rundump.sh <build dir> <tag> <OPT2 cycle> <end vb> <dump vb> [env...]: the indoor route on the G0 twin (task #165) with gl17's settings
# (as runr.sh) to <end vb>; EE RAM and the added mask ranges at <dump vb> to /home/claude/sbg0/dump/<tag>.<vb> and <tag>_dyn.<vb> (game RAM
# stays in the container; delete after use)
X=$1; t=$2; c=$3; E=$4; V=$5; shift 5
cd $X || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 \
  MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=$c MGS2_GS_MASK=/mnt/project-files/mgs2/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_EE_DUMP=/home/claude/sbg0/dump/$t:$V:0 MGS2_GS_DYN_OUT=/home/claude/sbg0/dump/${t}_dyn:$V "$@" \
  nice -n 10 ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "$t rc $? $(date +%H:%M:%S)"
