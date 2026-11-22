#!/bin/sh
# models17r.sh (task #161): the model build sb17m3/d (sb17 + patch_rainspr.py; mkpsp17.sh, no PGO, EBOOT 8a41bee9), PspProf on the indoor
# route's aft deck window 30,810-30,890, 80 steps: r_off C4 off (as gl16: sheets 1/2), r_now today's plan (C4 + C4X, streaks 1/2 by strips,
# sheets 1/2), r_n the card's "No streaks" (streaks none, sheets whole), r_rs the recommended plan (r_n + the rain's sprites left out).
S=/home/claude/sb10; export GL3=/home/claude/sb17m3; L=/home/claude/sb17m3/models17r.log
H="MGS2_GL_SPLASH=2"; ON=MGS2_OPT2=ffffffdf; OFF=MGS2_OPT2=fffff9df
echo "$(date +%H:%M:%S) batch" >> $L
sh $S/model.sh r_off 30810 30890 in $H MGS2_GL_RAIN=2 $OFF &
sh $S/model.sh r_now 30810 30890 in $H MGS2_GL_RAIN=2 $ON MGS2_GL_STREAKS=2 &
sh $S/model.sh r_n 30810 30890 in $H MGS2_GL_RAIN=1 $ON MGS2_GL_STREAKS=0 &
sh $S/model.sh r_rs 30810 30890 in $H MGS2_GL_RAIN=1 $ON MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 & wait
echo "$(date +%H:%M:%S) done" >> $L
