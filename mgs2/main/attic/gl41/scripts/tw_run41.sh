#!/bin/sh
# tw_run41.sh (gl41: tw_run40.sh with R=build_gl41) (gl40: tw_run39.sh with R=build_gl40) tw_run37.sh <build subfolder under build_gl37> (gl37 copy of build_gl36/tw_run.sh) <tag> [VAR=value ...]: one x86 twin run of route i to vb 37,800 through main/scripts/twin.sh
# with the PSP cfg's turn schedule (gl29x's: MGS2_OPT_CYCLE=24000:100 with 8 configurations, MGS2_HALF_CYCLE b, MGS2_GL_DZ X from 29,490),
# so the twin's cuts start where the PSP's do.  (twin.sh's own default starts the cycle at vb 1000: with MGS2_HALF_CYCLE the cut b is then
# on from vb 1001, not 24001.)  Reference: cfg/hashref_g22b.txt unless REF is set.  Logs in <subfolder>/x86/out_<tag>.log.
M=/home/claude/main; R=$M/build_gl41; S=$1; t=$2; shift 2
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df
B=$R/$S REF=${REF:-$M/cfg/hashref_g22b.txt} sh $M/scripts/twin.sh $t MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 \
  MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b "MGS2_GL_DZ=29490:31400:X" "$@"
