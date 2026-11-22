#!/bin/sh
# masters17.sh (task #158): the PPSSPP master pictures of the options on one binary (the model build sb17m/d): s17h C4+C4X, streaks 1/2,
# camera drop sheets 1/2 (the current plan); s17t streaks 1/3 and the sheets whole; s17n the rain's streaks off and the sheets whole.
C=/mnt/project-files/mgs2/tools/dev/sb15/c4master.sh
B=/home/claude/sb17m/d sh $C s17h ffffffdf MGS2_GL_STREAKS=2 &
B=/home/claude/sb17m/d sh $C s17t ffffffdf MGS2_GL_STREAKS=3 MGS2_GL_RAIN=1 &
B=/home/claude/sb17m/d sh $C s17n ffffffdf MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 &
wait; echo "masters17 done $(date +%H:%M:%S)"
