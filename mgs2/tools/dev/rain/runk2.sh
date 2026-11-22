#!/bin/sh
# runk.sh <tag> <kill spec> [env...]: the indoor route on the study's x86 twin (/home/claude/sbr/x1) with gl17's settings (as sb17/x8's
# out_t17.log, the gl17 reference) and MGS2_KILL_ACT=<kill spec> (task #164)
t=$1; k=$2; shift 2
X=/home/claude/sbr/x2 nice -n 10 sh /home/claude/sb14/chk14.sh i $t MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 \
  MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=fffff9df,ffffffdf MGS2_KILL_ACT=$k "$@"
echo "$t rc $(cat /home/claude/sbr/x2/rc_$t.txt) $(date +%H:%M:%S)"
