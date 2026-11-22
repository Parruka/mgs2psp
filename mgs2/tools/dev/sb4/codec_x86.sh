#!/bin/sh
# codec_x86.sh <run dir> <tag> <end vb> [env...]: the codec route on the x86 twin (codec68/x84r/env.txt's env: the pad of
# play/pad_codec.txt's calls) with the translator live from boot as the PSP runs it and MGS2_XLIVE_GX digests of every frame to <end>
B=$1; t=$2; E=$3; shift 3
cd $B || exit 1
env $(grep -v '^MGS2_XLIVE\|^MGS2_HASH_EVERY' /home/claude/codec68/x84r/env.txt) MGS2_HASH_EVERY=100 MGS2_XLIVE=0:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 \
  MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX=0:$E MGS2_XLIVE_GX_TEXKB=2688 "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
