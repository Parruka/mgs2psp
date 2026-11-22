#!/bin/sh
# model.sh <tag> <A> <B> <route in | codec> [cfg lines]: one PspProf window of sb5's model build (sb4 + lane 4, the codec faces, renderer_notes
# 11.32; mkpsp.sh's /home/claude/sb5m/d, no PGO for the picture code), gl3.sh's model step with X = /home/claude/sb5m.  Lane 4 off: the cfg
# line MGS2_OPT=efffffff (OPT_XL_LANE4 0x10000000).  gl10's settings: MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 (gl3.sh adds MGS2_GL_TEXKB=2688)
t=$1; a=$2; b=$3; r=$4; shift 4
GL3=/home/claude/sb5m sh /mnt/project-files/mgs2/psp/gl3.sh model $t $a $b all $r "$@"
