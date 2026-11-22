#!/bin/bash
# dump2.sh <build dir> <tag> <vb list "a:b:c"> : deck route to the last vb, EE RAM dumps at those vblanks into /home/claude/lean/dmp/<tag>.<vb>
B=$1; T=$2; V=$3; D=/home/claude/lean/dmp/run_$T; mkdir -p $D/memcard0; cp $B/boot $D/boot || exit 1; cd $D
last=${V##*:}; DV=${DV:-$last}
env MGS2_DISC=/home/claude/psp5/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)" MGS2_SIF_LOG=0 MGS2_EE_DUMP=/home/claude/lean/dmp/$T:$DV:0 \
  ./boot /home/claude/mgs2/SLES_503.83 $last > out.log 2>&1
echo "$T exit $?"
