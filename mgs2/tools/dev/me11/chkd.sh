#!/bin/sh
# chkd.sh <i|d|c> <tag> [env]: me11 (task #142) x86 run of /home/claude/me11/xd (me10 + the partial copy laid over RAM), the me9 settings
R=$1; t=$2; shift 2; X=/home/claude/me11/xd; S=/home/claude/sb9; MG=/mnt/project-files/mgs2
case $R in
  i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  c) sh $S/codec_x86.sh $X $t ${CEND:-40000} MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
esac
