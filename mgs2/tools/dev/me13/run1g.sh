#!/bin/sh
# run1g.sh: run1.sh for the me13 g twin in /home/claude/me13/xg (mk_g.sh). run1.sh <d | i | c> <tag> <end vb> [env...]: one me13 x86 run (task #147) in /home/claude/me13/xg, the design's copy set (me11 b's runs_h2.sh
# settings), the deadline copy (MGS2_XLIVE_DL=60 unless given) with the G0 guards (xd/g0.txt); out_<tag>.log, rc_<tag>.txt
R=$1; t=$2; E=$3; shift 3; cd /home/claude/me13/xg || exit 1
D="MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=0 MGS2_XLIVE_SNAPP=1 MGS2_XLIVE_SNAPR=12c..142,1ed MGS2_XLIVE_G0=/home/claude/me13/xg/g0.txt"
C="MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15"
case $R in
  d) PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"
     nice -n 10 env MGS2_DISC=/home/claude/psp5/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
       MGS2_XLIVE=0:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E $C $D MGS2_XLIVE_DL=60 "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1 ;;
  i) PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
     nice -n 10 env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
       MGS2_XLIVE=0:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E $C $D MGS2_XLIVE_DL=60 "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1 ;;
  c) nice -n 10 env $(grep -v '^MGS2_XLIVE\|^MGS2_HASH_EVERY' /home/claude/codec68/x84r/env.txt) MGS2_HASH_EVERY=100 MGS2_XLIVE=0:$E MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 \
       MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:$E $C $D MGS2_XLIVE_DL=60 "$@" ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1 ;;
esac
echo "rc $?" > rc_$t.txt
