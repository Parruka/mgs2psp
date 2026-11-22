#!/bin/sh
# digc_x86.sh <run dir> <tag> <from> <to> [env...]: dig_x86.sh on the codec route (codec_x86.sh's env, codec68/x84r/env.txt: the pad of
# play/pad_codec.txt's calls): the translator from vblank <from> (reset there) to <to>, its digest file dig_<tag>.txt (ge/xldig.c: per call
# the VU memory as it reads and the GS registers, per primitive its fields)
B=$1; t=$2; F=$3; E=$4; shift 4
cd $B || exit 1
env $(grep -v '^MGS2_XLIVE\|^MGS2_HASH_EVERY' /home/claude/codec68/x84r/env.txt) MGS2_HASH_EVERY=100 \
  MGS2_XLIVE=$F:$E:$B/dig_$t.txt MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 "$@" \
  ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
