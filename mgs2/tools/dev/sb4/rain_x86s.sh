#!/bin/sh
# rain_x86s.sh <dir> <tag> <end vb> [env...]: rain_x86.sh with the probe's output filtered by fxsrc.py into fs_<tag>.txt (task #114)
B=$1; t=$2; END=$3; shift 3
cd $B || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"; DISC=/home/claude/mgs2/disc
env MGS2_DISC=$DISC MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:$END MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$END MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_SB_LANE=3 \
  "$@" ./boot /home/claude/mgs2/SLES_503.83 $END 2>&1 | PYTHONDONTWRITEBYTECODE=1 python3 -I /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/sc/fxsrc.py > fs_$t.txt
echo "rc $?" > rc_$t.txt
