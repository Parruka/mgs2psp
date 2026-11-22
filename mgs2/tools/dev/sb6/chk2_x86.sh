#!/bin/sh
# chk2_x86.sh <route i | d | c> <tag prefix>: chk_x86.sh with MGS2_HASH_DENSE=15700:16010 on every run (as the routes' PSP cfgs: a hash and
# a "vuh:" line, the VU0 state, at every vblank of that range).  chk_x86.sh: sb6's x86 checks of lane 3 (renderer_notes 11.33) on one route
# from boot, three runs side by side on the twin /home/claude/sb6/x1: lane 3 off (MGS2_SB_LANE=11: lanes 1, 2 and 4, as sb5), on (15), and
# on with MGS2_SB_CHK=1 (every lane 3 effect call also through vc_fpos on the decoded VU memory); rain and splash 1/2 as gl10.  Route i: the
# indoor route to vb 37,800, d: the deck tutorial to 25,200 (tools/dev/gl1_x86.sh), c: the codec route to 40,000 (codec_x86.sh).
# out_<prefix>{off,on,chk}.log in x1.
R=$1; P=$2; X=/home/claude/sb6/x1; S=/home/claude/sb6; MG=/mnt/project-files/mgs2
run() { t=$1; shift
  case $R in
    i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 "$@" ;;
    d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 "$@" ;;
    c) sh $S/codec_x86.sh $X $t 40000 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 "$@" ;;
  esac }
D=MGS2_HASH_DENSE=15700:16010
run ${P}off $D MGS2_SB_LANE=11 & run ${P}on $D MGS2_SB_LANE=15 & run ${P}chk $D MGS2_SB_LANE=15 MGS2_SB_CHK=1 &
wait
