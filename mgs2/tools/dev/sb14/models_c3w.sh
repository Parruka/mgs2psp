#!/bin/sh
# models_c3w.sh: task #144, 3w on the PSP model build sb14m2/d (sb14 with 3w; mkpsp.sh, no PGO; EBOOT cfb5eed4): the deck window
# 30,810-30,890 and the inside window 33,410-33,490 of the indoor route, 3w off (MGS2_OPT2=fffffedf: 3v and 3z on) and all on (the
# default), four at a time; rain and splash 1/2 as gl15
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb14m2; L=/home/claude/sb14/c4x.log
echo "$(date +%H:%M:%S) models_c3w" >> $L
sh $S/model.sh c3wd0 30810 30890 in $C MGS2_OPT2=fffffedf & sh $S/model.sh c3wd1 30810 30890 in $C &
sh $S/model.sh c3wi0 33410 33490 in $C MGS2_OPT2=fffffedf & sh $S/model.sh c3wi1 33410 33490 in $C & wait
echo "$(date +%H:%M:%S) models_c3w done" >> $L
