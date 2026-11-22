#!/bin/bash
# ww.sh <build dir> <tag> <last vb> <watch "from:addr,addr">: deck route with the write watch
B=$1; T=$2; V=$3; WW=$4; D=/home/claude/lean/dmp/ww_$T; mkdir -p $D/memcard0; cp $B/boot $D/boot || exit 1; cd $D
env MGS2_DISC=/home/claude/psp5/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)" MGS2_SIF_LOG=0 MGS2_WWATCH=$WW \
  setarch -R ./boot /home/claude/mgs2/SLES_503.83 $V > out.log 2>&1
echo "$T exit $?"; grep -c '^wwatch' out.log
