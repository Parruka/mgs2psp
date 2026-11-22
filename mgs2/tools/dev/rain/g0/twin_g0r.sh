#!/bin/sh
# G0R twin (task #167): the indoor route on xg0r with gl17's settings, G0R off (fffdf9df,fffdffdf) and on (fffff9df,ffffffdf), H3R, G0H, H3, NBS, G0C on in both
cd /home/claude/sbg0
sh runr.sh /home/claude/sbg0/xg0r g0r0 fffdf9df,fffdffdf &
sh runr.sh /home/claude/sbg0/xg0r g0r1 fffff9df,ffffffdf &
wait
grep -h 'native rain:\|h3r rain:' xg0r/out_g0r0.log xg0r/out_g0r1.log | cut -c1-300
