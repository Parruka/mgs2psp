#!/bin/sh
# runr.sh <build dir> <tag> <OPT2 cycle words> [env...]: the indoor route on the G0 twin (task #165) with gl17's settings (as sb17/x8's
# out_t17.log, the gl17 reference), the game-state hash with v19's renderer mask and the native code's added ranges, and the virtual time
X=$1; t=$2; c=$3; shift 3
X=$X nice -n 10 sh /home/claude/sb14/chk14.sh i $t MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 \
  MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=$c MGS2_GS_MASK=/mnt/project-files/mgs2/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 "$@"
echo "$t rc $(cat $X/rc_$t.txt) $(date +%H:%M:%S)"
