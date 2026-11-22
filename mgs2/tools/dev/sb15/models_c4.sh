#!/bin/sh
# models_c4.sh: task #152, step 4 (C4: the effects drawn) on the PSP model build sb15m/d (mkpsp.sh: no PGO): the indoor route's deck
# window 30,810-30,890 and inside window 33,410-33,490, each with C4 off (the default, fffffddf) and on (MGS2_OPT2=ffffffdf); rain and
# splash 1/2 as gl16.  Two at a time (two master runs share the machine).  fitcmp.py (tools/dev/sb6) for the pairs.
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb15m; L=/home/claude/sb15m/models_c4.log
echo "$(date +%H:%M:%S) models_c4: deck" >> $L
sh $S/model.sh c4d0 30810 30890 in $C MGS2_OPT2=fffffddf & sh $S/model.sh c4d1 30810 30890 in $C MGS2_OPT2=ffffffdf & wait
echo "$(date +%H:%M:%S) models_c4: inside" >> $L
sh $S/model.sh c4i0 33410 33490 in $C MGS2_OPT2=fffffddf & sh $S/model.sh c4i1 33410 33490 in $C MGS2_OPT2=ffffffdf & wait
echo "$(date +%H:%M:%S) models_c4 done" >> $L
