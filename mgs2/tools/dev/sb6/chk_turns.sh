#!/bin/sh
# chk_turns.sh <i | d | c> <tag>: gl11's x86 turns check (task #119): the twin x2 (boot/xlive_chk.c with MGS2_SB_LANE_OPT: the lanes follow
# g_opt's lane bits at each transfer, as psp/xl_psp.c's do) with gl11's cycle 1000:100:ffffffff,dfffffff,ffffffff,efffffff (everything on,
# lane 3 off, everything on, lane 4 off), lanes 1-4, rain and splash 1/2 as gl10; compare (cmp6.py) with x1's everything-on run of the
# same route (out_i2on / out_d2on / out_c2on).  Out: x2/out_<tag>.log.
R=$1; t=$2; X=/home/claude/sb6/x2; S=/home/claude/sb6; MG=/mnt/project-files/mgs2
E="MGS2_SB_LANE=15 MGS2_SB_LANE_OPT=1 MGS2_OPT_CYCLE=1000:100:ffffffff,dfffffff,ffffffff,efffffff MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"
case $R in
  i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 $E ;;
  d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 $E ;;
  c) sh $S/codec_x86.sh $X $t 40000 $E ;;
esac
