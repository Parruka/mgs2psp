#!/bin/sh
# gl1_x86.sh <check build dir> <tag> <d | i> <digest from:to> [env...]: the x86 twin of a psp/gl1.sh emulator run (renderer_notes 11.13
# "3e", task #56): boot/xlive_chk.c (tools/dev/build_xlive.sh, GU=lean) with the translator live from boot as the PSP harness runs it (spans,
# the one walk, shown frames only, no packet hash) and MGS2_XLIVE_GX: the PSP harness's frames and GE lists, the "picture:" digest lines of
# the frames whose last vblank lies in from..to.  Route d: the deck tutorial to vb 25,200; i: the indoor route to vb 37,800 (as runx.sh).
# out_<tag>.log and rc_<tag>.txt in the build dir; compare its "picture:" lines with the PSP log's (grep '^picture: vb').
B=$1; t=$2; R=$3; D=$4; shift 4
cd $B || exit 1
if [ "$R" = i ]; then PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"; DISC=/home/claude/mgs2/disc; END=37800
else PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"; DISC=/home/claude/psp5/mgs2disc.pak; END=25200; fi
env MGS2_DISC=$DISC MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:$END MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=$D "$@" ./boot /home/claude/mgs2/SLES_503.83 $END > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
