#!/bin/sh
# models17.sh (task #158, Ffm 13:27): the effects' cost by streak share on the model build sb17m/d (sb17 + MGS2_GL_STREAKS=0 none; mkpsp17.sh,
# no PGO), PspProf on the indoor route's deck window 30,810-30,890 (aft deck in the rain), 80 steps.  Four at a time.
S=/home/claude/sb10; export GL3=/home/claude/sb17m; L=/home/claude/sb17m/models17.log
H="MGS2_GL_SPLASH=2"; ON=MGS2_OPT2=ffffffdf; OFF=MGS2_OPT2=fffff9df
echo "$(date +%H:%M:%S) batch 1" >> $L
sh $S/model.sh m_off 30810 30890 in $H MGS2_GL_RAIN=2 $OFF &
sh $S/model.sh m_s1 30810 30890 in $H MGS2_GL_RAIN=2 $ON &
sh $S/model.sh m_s2 30810 30890 in $H MGS2_GL_RAIN=2 $ON MGS2_GL_STREAKS=2 &
sh $S/model.sh m_s3 30810 30890 in $H MGS2_GL_RAIN=2 $ON MGS2_GL_STREAKS=3 & wait
echo "$(date +%H:%M:%S) batch 2" >> $L
sh $S/model.sh m_s0 30810 30890 in $H MGS2_GL_RAIN=2 $ON MGS2_GL_STREAKS=0 &
sh $S/model.sh m_s3w 30810 30890 in $H MGS2_GL_RAIN=1 $ON MGS2_GL_STREAKS=3 &
sh $S/model.sh m_s0w 30810 30890 in $H MGS2_GL_RAIN=1 $ON MGS2_GL_STREAKS=0 &
sh $S/model.sh m_offw 30810 30890 in $H MGS2_GL_RAIN=1 $OFF & wait
echo "$(date +%H:%M:%S) done" >> $L
