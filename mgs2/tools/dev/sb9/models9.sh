#!/bin/sh
# models9.sh: sb9's model windows on sb9m/d (mkpsp.sh: no PGO; step 0 with 4-way tables): deck 30,810-30,890, inside 33,410-33,490,
# codec route 31,510-31,590.  Everything on, then g_opt2 off (MGS2_OPT2=0: gl12's path), then on the deck 0a off alone (fffffffe) and
# 0b off alone (fffffffd).  Three at a time; fitcmp.py (tools/dev/sb6) for each pair.
S=/home/claude/sb9; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb9m
sh $S/model.sh s9on 30810 30890 in $C & sh $S/model.sh s9ion 33410 33490 in $C & sh $S/model.sh s9cod 31510 31590 codec $C & wait
sh $S/model.sh s9off 30810 30890 in $C MGS2_OPT2=0 & sh $S/model.sh s9ioff 33410 33490 in $C MGS2_OPT2=0 &
sh $S/model.sh s9coff 31510 31590 codec $C MGS2_OPT2=0 & wait
sh $S/model.sh s9a0 30810 30890 in $C MGS2_OPT2=fffffffe & sh $S/model.sh s9b0 30810 30890 in $C MGS2_OPT2=fffffffd & wait
echo models done
