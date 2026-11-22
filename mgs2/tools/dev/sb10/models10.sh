#!/bin/sh
# models10.sh: task #133 Stage C step 1, T1 (the texture memo's front, OPT2_TX_FRONT 4) on sb10m/d (mkpsp.sh: no PGO): deck 30,810-30,890,
# inside 33,410-33,490, codec route 31,510-31,590.  T1 on (the default), then T1 off alone (MGS2_OPT2=fffffffb: step 0 stays on).
# Three at a time; fitcmp.py (tools/dev/sb6) for each pair.
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb10m
sh $S/model.sh t1on 30810 30890 in $C & sh $S/model.sh t1ion 33410 33490 in $C & sh $S/model.sh t1cod 31510 31590 codec $C & wait
sh $S/model.sh t1off 30810 30890 in $C MGS2_OPT2=fffffffb & sh $S/model.sh t1ioff 33410 33490 in $C MGS2_OPT2=fffffffb &
sh $S/model.sh t1coff 31510 31590 codec $C MGS2_OPT2=fffffffb & wait
echo models done
