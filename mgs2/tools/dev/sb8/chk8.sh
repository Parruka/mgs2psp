#!/bin/sh
# chk8.sh <route i | d | c> <tag> [env...]: one x86 run of sb8's twin /home/claude/sb8/x1 (task #121: lane constants decoded at once,
# faster own[] marking) from boot, lanes 1-4, rain and splash 1/2 as gl11; compare with sb6/x1's all-on runs (out_i2on, out_d2on, out_c2on).
# Route i: the indoor route to vb 37,800, d: the deck tutorial to 25,200 (tools/dev/gl1_x86.sh), c: the codec route to 40,000.  out_<tag>.log in x1.
R=$1; t=$2; shift 2; X=/home/claude/sb8/x1; S=/home/claude/sb8; MG=/mnt/project-files/mgs2
case $R in
  i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  c) sh $S/codec_x86.sh $X $t 40000 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
esac
