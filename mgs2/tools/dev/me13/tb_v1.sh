#!/bin/sh
# tb.sh <d | i | c> <tag> <from vb> <end vb> [env...]: me13 (task #147) the take-back's x86 check: sb14's twin (x1/boot, today's walk) with the
# translator started cold at <from> (MGS2_XLIVE=<from>:<end>: no translator state, no texture map or cache before it), as the main CPU
# takes the picture back from the ME at a shown frame; frames from <from> on against sb14's run from vb 0 (out_<r>t0.log)
R=$1; t=$2; F=$3; E=$4; shift 4; cd /home/claude/me13/tb || exit 1
C="MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15"
case $R in
  d) PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"
     nice -n 10 env MGS2_DISC=/home/claude/psp5/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
       MGS2_XLIVE=$F:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E $C "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1 ;;
  i) PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
     nice -n 10 env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
       MGS2_XLIVE=$F:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E $C "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1 ;;
  c) nice -n 10 env $(grep -v '^MGS2_XLIVE\|^MGS2_HASH_EVERY' /home/claude/codec68/x84r/env.txt) MGS2_HASH_EVERY=100 MGS2_XLIVE=$F:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 \
       MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E $C "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1 ;;
esac
echo "rc $?" > rc_$t.txt
