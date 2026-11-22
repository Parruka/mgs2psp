#!/bin/sh
# chkw.sh <i|d|c> <tag> [env]: me12 (task #142) x86 run of /home/claude/me12/xd (the writer tracker), late by 2 steps with the copy
# (DEFER=2 SNAP=2) and MGS2_XLIVE_WT=1; me10's settings otherwise; out_<tag>.log in xd.
R=$1; t=$2; shift 2; X=/home/claude/me12/xd; S=/home/claude/sb9; MG=/mnt/project-files/mgs2
case $R in
  i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_WT=1 "$@" ;;
  d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_WT=1 "$@" ;;
  c) sh $S/codec_x86.sh $X $t ${CEND:-40000} MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_WT=1 "$@" ;;
esac
