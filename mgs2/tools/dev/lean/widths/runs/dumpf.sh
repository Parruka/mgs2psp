#!/bin/bash
# dumpf.sh <build dir> <tag> <vb>: free route (v25f pad and pack) to vb, EE RAM dump at vb into /home/claude/lean/dmp/<tag>.<vb>
B=$1; T=$2; V=$3; D=/home/claude/lean/dmp/runf_$T; mkdir -p $D/memcard0; cp $B/boot $D/boot || exit 1; cd $D
env MGS2_DISC=/home/claude/v25f/pak/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$(cat /home/claude/v25f/pad_v25f.txt)" MGS2_SIF_LOG=0 MGS2_EE_DUMP=/home/claude/lean/dmp/$T:$V:0 \
  ./boot /home/claude/mgs2/SLES_503.83 $V > out.log 2>&1
echo "$T exit $?"
