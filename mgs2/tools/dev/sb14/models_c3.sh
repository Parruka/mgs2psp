#!/bin/sh
# models_c3.sh: task #144, step 3's trims on the PSP model build sb14m/d (mkpsp.sh: no PGO; EBOOT 8537e496): the deck window
# 30,810-30,890 and the inside window 33,410-33,490 of the indoor route, each with both trims off (MGS2_OPT2=ffffff1f), 3v alone
# (ffffff5f) and both on (the default), three at a time; rain and splash 1/2 as gl15.  fitcmp.py (tools/dev/sb6) for the pairs.
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb14m; L=/home/claude/sb14/c4x.log
echo "$(date +%H:%M:%S) models_c3: deck" >> $L
sh $S/model.sh c3d0 30810 30890 in $C MGS2_OPT2=ffffff1f & sh $S/model.sh c3d1 30810 30890 in $C MGS2_OPT2=ffffff5f & sh $S/model.sh c3d2 30810 30890 in $C & wait
echo "$(date +%H:%M:%S) models_c3: inside" >> $L
sh $S/model.sh c3i0 33410 33490 in $C MGS2_OPT2=ffffff1f & sh $S/model.sh c3i1 33410 33490 in $C MGS2_OPT2=ffffff5f & sh $S/model.sh c3i2 33410 33490 in $C & wait
echo "$(date +%H:%M:%S) models_c3 done" >> $L
