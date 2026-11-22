#!/bin/sh
# run.sh <a> <b>: the deck route with GI counts for vblanks a+1..b (x86 play binary, zero-time clock, full renderer)
P=/home/claude/play; D=/home/claude/play/tutb/b$1; mkdir -p $D; cd $D || exit 1
PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"
exec env MGS2_HASH_EVERY=100 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_DISC=/home/claude/mgs2/disc MGS2_DISC_LSN=/home/claude/psp5/mgs2disc.pak MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
  MGS2_GI=$1:$2:$D/gi.txt $P/x/boot /home/claude/mgs2/SLES_503.83 $(($2 + 1)) > $D/run.log 2>&1
