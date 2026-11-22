#!/bin/sh
# digc2_x86.sh <tag prefix> <from> <to>: dig2_x86.sh on the codec route (digc_x86.sh): the per-call digest files with lane 4 off
# (MGS2_SB_LANE=3) and on (11), rain and splash 1/2 as gl10, side by side on the twin /home/claude/sb5/x1; then cmp of the two files
P=$1; F=$2; E=$3; X=/home/claude/sb5/x1; S=/home/claude/sb5
sh $S/digc_x86.sh $X ${P}off $F $E MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=3 &
sh $S/digc_x86.sh $X ${P}on $F $E MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=11 &
wait
cat $X/rc_${P}off.txt $X/rc_${P}on.txt; ls -l $X/dig_${P}off.txt $X/dig_${P}on.txt
cmp $X/dig_${P}off.txt $X/dig_${P}on.txt && echo "dig files identical"
