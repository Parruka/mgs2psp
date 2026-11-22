#!/bin/sh
# models17c.sh (task #158): the rain's streak calls 1 in N (whole calls by block, MGS2_GL_STREAK_CALLS) on the model build sb17m2/d (sb17 +
# patch_calls.py; mkpsp17.sh, no PGO, EBOOT 00255284), PspProf on the indoor route's deck window 30,810-30,890, 80 steps; with its own
# effects-off run and the current plan (strips 1/2) to compare with sb17m/d's.
S=/home/claude/sb10; export GL3=/home/claude/sb17m2; L=/home/claude/sb17m2/models17c.log
H="MGS2_GL_SPLASH=2"; ON=MGS2_OPT2=ffffffdf; OFF=MGS2_OPT2=fffff9df
echo "$(date +%H:%M:%S) batch" >> $L
sh $S/model.sh c_off 30810 30890 in $H MGS2_GL_RAIN=2 $OFF &
sh $S/model.sh c_s2 30810 30890 in $H MGS2_GL_RAIN=2 $ON MGS2_GL_STREAKS=2 &
sh $S/model.sh c_c3w 30810 30890 in $H MGS2_GL_RAIN=1 $ON MGS2_GL_STREAK_CALLS=3 &
sh $S/model.sh c_c2 30810 30890 in $H MGS2_GL_RAIN=2 $ON MGS2_GL_STREAK_CALLS=2 & wait
echo "$(date +%H:%M:%S) done" >> $L
