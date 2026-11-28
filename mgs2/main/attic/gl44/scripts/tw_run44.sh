#!/bin/sh
# tw_run44.sh <build subfolder> <tag> [VAR=value ...] (gl44: build_gl43/tw_run43.sh with R=build_gl44 and the base words with bits 21 and 22 clear (ff9ff9df: V44 off); gl43: build_gl42/tw_run42.sh with R=build_gl43 and the base OPT2 words without B (bit 21: ffdff9df); gl42: build_gl41/tw_run41.sh with R=build_gl42, the run under nice 19 and the
# split dump off (MGS2_HASH_REF_DUMP into a folder that does not exist: gl42's people change the state hash from vb ~24,200 on, so a
# run against hashref_gl38 differs on purpose and must not write EE RAM)): one x86 twin run of route i to vb 37,800 through
# main/scripts/twin.sh with gl29x's base cycle words (the gl42 keys after them win).  Logs in <subfolder>/x86/out_<tag>.log.
M=/home/claude/main; R=$M/build_gl44; S=$1; t=$2; shift 2
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=ff9ff9df,ff9ff9df,ff9ff9df,ff9ff9df,ff9ff9df,ff9ff9df,ff9ff9df,ff9ff9df
B=$R/$S REF=${REF:-$M/cfg/hashref_g22b.txt} nice -n 19 sh $M/scripts/twin.sh $t MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 \
  MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b MGS2_OPT2=ff9ff9df "MGS2_GL_DZ=29490:31400:X" MGS2_HASH_REF_DUMP=$R/no_such_dir/split_$t "$@"
