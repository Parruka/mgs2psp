#!/bin/sh
# thin_x86.sh <route i | d> <tag prefix> <N>: sb6's thinning checks of lane 3 (renderer_notes 11.33, 11.30) from boot, two runs side by side
# on the twin /home/claude/sb6/x1: lane 3 off (MGS2_SB_LANE=11) and on (15), the rain's sheets and the splash at 1 strip in N
# (MGS2_GL_RAIN=N MGS2_GL_SPLASH=N; 1: as the PS2) and MGS2_THIN_CHK=1 (each thinned call converted again without thinning, compared).
# Route i: the indoor route to vb 37,800 (it crosses the aft deck in the rain), d: the deck tutorial to 25,200.  out_<prefix>{off,on}.log
R=$1; P=$2; N=$3; X=/home/claude/sb6/x1; MG=/mnt/project-files/mgs2
run() { t=$1; shift
  case $R in
    i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=$N MGS2_GL_SPLASH=$N MGS2_THIN_CHK=1 "$@" ;;
    d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=$N MGS2_GL_SPLASH=$N MGS2_THIN_CHK=1 "$@" ;;
  esac }
run ${P}off MGS2_SB_LANE=11 & run ${P}on MGS2_SB_LANE=15 &
wait
