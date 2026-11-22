#!/bin/sh
# twin.sh [tag]: step 4b, the x86 twin's run (x86.sh twin's boot): route i to vb 37,800 as gl18me's reference twin r18a was run
# (tools/dev/rain/g0/runr.sh -> tools/dev/sb14/chk14.sh i -> tools/dev/gl1_x86.sh: gl17's rain settings, the picture translator live from
# boot with its digests 0-37,800) with the rain's seven skips always on (MGS2_OPT2=fffff9df, also as each cycle word), the dense hashes
# 15,700-16,010 with their vuh lines, and every hash checked against prof/hashref_x86_in_gl18me.txt as it is taken (MGS2_HASH_REF:
# "split:" lines on a difference).  The disc is the pack (MGS2_DISC=<pak>), the pad play/pad_w01af.txt.  Then tools/refcmp.py: done
# when every one of the reference's 685 lines is equal (ee, vfb, vuh).  The log (game state hashes only) stays in $B/x86.
. "$(dirname "$0")/env.sh"
t=${1:-r18a}; [ $# -gt 0 ] && shift; X=$B/x86; REF=${REF:-$MG/prof/hashref_x86_in_gl18me.txt}
[ -x $X/boot ] || { echo "twin.sh: no $X/boot (x86.sh twin)"; exit 1; }
cd $X || exit 1; s=$(date +%s)
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 \
  MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff \
  MGS2_OPT2_CYCLE=fffff9df,fffff9df MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=fffff9df MGS2_HASH_DENSE=15700:16010 MGS2_HASH_REF=$REF MGS2_HASH_REF_DUMP=$X/split_$t "$@" \
  ./boot "$SL" 37800 > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
echo "twin.sh: $t $(cat rc_$t.txt), $(( $(date +%s) - s )) s; split lines: $(grep -c '^split: first\|^split: VU0' out_$t.log)"
python3 -I $M/tools/refcmp.py $REF out_$t.log hashref_$t.txt
r=$?; cmp -s hashref_$t.txt $REF && echo "twin.sh: hashref_$t.txt equals $(basename $REF) byte for byte"; exit $r
