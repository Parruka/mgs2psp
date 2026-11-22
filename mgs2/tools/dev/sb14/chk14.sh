#!/bin/sh
# chk14.sh <route i | d | c> <tag> [env...] (X=<build dir>, default /home/claude/sb14/x1): one x86 run of sb14 (sb13 + step 3's trims 3v and 3z, task #144); as chk13.sh: one x86 run of sb13 (sb12 + the census of the rooms' meshes, task #144); as chk12x.sh: one x86 run of sb12 (C2 census and GE model, task #140) /home/claude/sb12/x1; as chk11.sh: one x86 run of sb11 (C1 census and GE model, task #134) /home/claude/sb11/x1; as chk10.sh: one x86 run of sb10s twin (Stage C step 1, task #133; else as sb9's chk9.sh:) one x86 run of sb9's twin (Stage C step 0, task #128: 0a texture look-ups
# by upload identity, 0b CLUT loads by identity; g_opt2 bits 1 and 2, MGS2_OPT2 / MGS2_OPT2_CYCLE; MGS2_TX_UMEMO_CHK=1 / MGS2_GU_LDID_CHK=1
# make the bits' "on" the check mode) from boot, lanes 1-4, rain and splash 1/2 as gl12; compare with sb8/x1's all-on runs (out_i11on,
# out_d11on, out_c11on).  Route i: the indoor route to vb 37,800, d: the deck tutorial to 25,200 (tools/dev/gl1_x86.sh), c: the codec
# route to 40,000.  out_<tag>.log in x1.
R=$1; t=$2; shift 2; X=${X:-/home/claude/sb14/x1}; S=/home/claude/sb10; MG=/mnt/project-files/mgs2
case $R in
  i) sh $MG/tools/dev/gl1_x86.sh $X $t i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  d) sh $MG/tools/dev/gl1_x86.sh $X $t d 0:25200 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
  c) sh $S/codec_x86.sh $X $t ${CEND:-40000} MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ;;
esac
