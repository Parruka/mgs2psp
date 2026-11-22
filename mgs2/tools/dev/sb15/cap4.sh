#!/bin/sh
# cap4.sh <d | i> <tag> <first vb> <vblanks>: an x86 twin run (sb14/x1/boot, gl16's x86 tree, gl1_x86.sh's settings as chk14.sh)
# that stops after the window and writes a GS capture of it (MGS2_GSCAP) for gsref: /home/claude/c4cap/<tag>.cap, log <tag>.log
R=$1; t=$2; F=$3; N=$4; X=/home/claude/sb14/x1; O=/home/claude/c4cap
END=$((F + N + 2))
if [ "$R" = i ]; then PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"; DISC=/home/claude/mgs2/disc
else PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"; DISC=/home/claude/psp5/mgs2disc.pak; fi
cd $X || exit 1
env MGS2_DISC=$DISC MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:$END MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=$F:$((F + N)) \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 \
  MGS2_GSCAP=$O/$t.cap:$F:$N ./boot /home/claude/mgs2/SLES_503.83 $END > $O/$t.log 2>&1
echo "rc $?" > $O/rc_$t.txt
