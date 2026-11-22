#!/bin/sh
# G0R: EE dumps in both modes at vb $1 (H3R on in both), the two modes side by side, then dmask.py; the dumps deleted after
V=$1; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/h3r
cd /home/claude/sbg0
sh rundump.sh /home/claude/sbg0/xg0r dg0r0 fffdf9df,fffdffdf $V $V &
sh rundump.sh /home/claude/sbg0/xg0r dg0r1 fffff9df,ffffffdf $V $V &
wait
ls -la /home/claude/sbg0/dump/dg0r0.$V /home/claude/sbg0/dump/dg0r1.$V
python3 -I $S/dmask.py /home/claude/sbg0/dump/dg0r0.$V /home/claude/sbg0/dump/dg0r1.$V /home/claude/sbg0/dump/dg0r0_dyn.$V /home/claude/sbg0/dump/dg0r1_dyn.$V > $S/dmask_g0r_$V.txt 2>&1
head -30 $S/dmask_g0r_$V.txt
