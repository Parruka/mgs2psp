#!/bin/sh
# H3R twin (task #167): the indoor route on xh3r with gl17's settings, H3R off (fffef9df,fffeffdf) and on (fffff9df,ffffffdf), G0H, H3, NBS, G0C on in both
cd /home/claude/sbg0
sh runr.sh /home/claude/sbg0/xh3r h3r0 fffef9df,fffeffdf &
sh runr.sh /home/claude/sbg0/xh3r h3r1 fffff9df,ffffffdf &
wait
grep -h 'h3r rain:' xh3r/out_h3r0.log xh3r/out_h3r1.log
