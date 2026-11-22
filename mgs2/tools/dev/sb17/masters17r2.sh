#!/bin/sh
# masters17r2.sh (task #161): more pictures of the rain's sprites left out, at the frames where the x86 twin's lists lose the most to the
# filter (x2 i_st0 against x5 i_rs): the opening 10,372 and 10,872; single frames 15,505, 22,978, 26,007; the deck 24,318; the aft deck
# 30,700; inside after the deck 31,701, 31,851, 32,001, 32,151, 32,195 (frames 31,663-32,197 lose 22-24 PRIMs each).  Both on the model
# build sb17m3/d (sb17 + patch_rainspr.py, EBOOT 8a41bee9), C4 + C4X on, streaks none, sheets whole: s17n2 without the filter (as s17n),
# s17r2 with it (as s17r).  MGS2_GL_SHOT holds hook vblanks (frame + 2).
C=/mnt/project-files/mgs2/tools/dev/sb15/c4master.sh; S=MGS2_GL_SHOT=10374,10874,15507,22980,24320,26009,30702,31703,31853,32003,32153,32197
B=/home/claude/sb17m3/d sh $C s17n2 ffffffdf MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 $S &
B=/home/claude/sb17m3/d sh $C s17r2 ffffffdf MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 $S &
wait; echo "masters17r2 done $(date +%H:%M:%S)"
