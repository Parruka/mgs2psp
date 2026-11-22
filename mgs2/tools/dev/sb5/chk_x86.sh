#!/bin/sh
# chk_x86.sh <route i | d | c> <tag prefix>: sb5's x86 checks of lane 4 (renderer_notes 11.32) on one route from boot, three runs side by
# side on the twin /home/claude/sb5/x1: lane 4 off (MGS2_SB_LANE=3: lanes 1 and 2, as gl10), on (11), and on with MGS2_SB_CHK=1 (every
# lane P07 call also through vc_p07 on the decoded VU memory); rain and splash 1/2 as gl10.  Route i: the indoor route to vb 37,800, d: the
# deck tutorial to 25,200 (tools/dev/gl1_x86.sh), c: the codec route to 40,000 (codec_x86.sh).  out_<prefix>{off,on,chk}.log in x1.
R=$1; P=$2; X=/home/claude/sb5/x1; S=/home/claude/sb5; MG=/mnt/project-files/mgs2
run() { t=$1; shift
  case $R in
    i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 "$@" ;;
    d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 "$@" ;;
    c) sh $S/codec_x86.sh $X $t 40000 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 "$@" ;;
  esac }
run ${P}off MGS2_SB_LANE=3 & run ${P}on MGS2_SB_LANE=11 & run ${P}chk MGS2_SB_LANE=11 MGS2_SB_CHK=1 &
wait
