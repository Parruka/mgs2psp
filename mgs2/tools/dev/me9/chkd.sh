#!/bin/sh
# chkd.sh <i|d|c> <tag> [env]: me9 (task #130) x86 run of the study twin /home/claude/me9/xd (sb9's x1 + boot/xlive_chk.c with
# MGS2_XLIVE_DEFER), the chk9.sh settings; out_<tag>.log in xd.
R=$1; t=$2; shift 2; X=/home/claude/me9/xd; S=/home/claude/sb9; MG=/mnt/project-files/mgs2
case $R in
  i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  c) sh $S/codec_x86.sh $X $t ${CEND:-40000} MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
esac
