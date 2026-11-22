#!/bin/sh
# dig_x86.sh <run dir> <tag> <from> <to> [env...]: the indoor route on the x86 twin with the translator from vblank <from> (reset there) to
# <to>, its digest file dig_<tag>.txt (ge/xldig.c: per call the VU memory as it reads and the GS registers, per primitive its fields)
B=$1; t=$2; F=$3; E=$4; shift 4
cd $B || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=$F:$E:$B/dig_$t.txt MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 "$@" \
  ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
