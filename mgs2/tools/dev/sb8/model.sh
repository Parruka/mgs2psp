#!/bin/sh
# model.sh <tag> <A> <B> <route in | codec> [cfg lines]: one PspProf window of sb8b/d, the final form (GL3=<dir> for another: sb8n = the dropped leaf form, sb8m = the first version), formerly sb6's model build (sb5 + lane 3, the effects, renderer_notes
# 11.33; mkpsp.sh's /home/claude/sb6m/d, no PGO for the picture code), gl3.sh's model step with X = /home/claude/sb6m.  Lane 3 off: the cfg
# line MGS2_OPT=dfffffff (OPT_XL_LANE3 0x20000000; lane 4 off: efffffff).  gl10's settings: MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 (gl3.sh adds
# MGS2_GL_TEXKB=2688)
t=$1; a=$2; b=$3; r=$4; shift 4
GL3=${GL3:-/home/claude/sb8b} sh /mnt/project-files/mgs2/psp/gl3.sh model $t $a $b all $r "$@"
