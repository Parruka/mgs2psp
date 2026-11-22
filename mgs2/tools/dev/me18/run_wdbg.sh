#!/bin/sh
# run_wdbg.sh <end vb> <tag> [env...]: me12's tracker (xd/boot_dbg, patch_wtdbg.py) on route i as chkw.sh i, to <end vb>; out_<tag>.log in xd
E=$1; t=$2; shift 2
cd /home/claude/me12/xd || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_WT=1 "$@" \
  timeout 1200 ./boot_dbg /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
