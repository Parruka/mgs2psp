#!/bin/sh
# probe_x86.sh <run dir> <tag> <vb> [env...]: the indoor route on the x86 twin (as tools/dev/gl1_x86.sh i, the translator live from boot)
# to vblank <vb> + 1 with MGS2_SB_PROBE=<vb>: every VIF1 tag the generic walk meets in that vblank and the calls between them (dev)
B=$1; t=$2; V=$3; shift 3
cd $B || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"; E=$((V + 1))
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=$V:$V MGS2_SB_PROBE=$V "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
