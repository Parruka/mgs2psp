#!/bin/sh
# dig2_x86.sh <tag prefix> <from> <to>: the per-call digest files (dig_x86.sh: the indoor route, the translator from vblank <from> to <to>;
# ge/xldig.c: per call the VU memory as it reads and the GS registers, per primitive its fields) with lane 3 off (MGS2_SB_LANE=11) and on
# (15), rain and splash 1/2 as gl10, side by side on the twin /home/claude/sb6/x1; then cmp of the two files
P=$1; F=$2; E=$3; X=/home/claude/sb6/x1; S=/home/claude/sb6
sh $S/dig_x86.sh $X ${P}off $F $E MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=11 &
sh $S/dig_x86.sh $X ${P}on $F $E MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 &
wait
cat $X/rc_${P}off.txt $X/rc_${P}on.txt; ls -l $X/dig_${P}off.txt $X/dig_${P}on.txt
cmp $X/dig_${P}off.txt $X/dig_${P}on.txt && echo "dig files identical"
