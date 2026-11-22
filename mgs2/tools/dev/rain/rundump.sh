#!/bin/sh
# rundump.sh <tag> <end vb> <dump vb> [env...]: the indoor route on the study's twin x2 with gl17's settings (as runk2.sh) to <end vb>, EE RAM
# dumped at <dump vb> to /home/claude/sbr/dump/<tag>.<vb> (task #164; game RAM stays in the container)
t=$1; E=$2; V=$3; shift 3
cd /home/claude/sbr/x2 || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 \
  MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=fffff9df,ffffffdf MGS2_EE_DUMP=/home/claude/sbr/dump/$t:$V:0 "$@" \
  nice -n 10 ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "$t rc $? $(date +%H:%M:%S)"
