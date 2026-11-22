#!/bin/sh
# models_t2.sh: task #133 Stage C step 1, T2 (P07 calls held to the transfer's end, OPT2_C7 8) on sb10m2/d (mkpsp.sh: no PGO; EBOOT
# e1c979e3, with up_last's division-free forms): deck 30,810-30,890, inside 33,410-33,490, codec route 31,510-31,590.  Everything on (T1
# and T2), then T2 off alone (MGS2_OPT2=fffffff7).  Three at a time; fitcmp.py (tools/dev/sb6) for each pair, and T2 off against sb10m's
# T1 on (the translator's slot bookkeeping, which runs either way).
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb10m2
sh $S/model.sh t2on 30810 30890 in $C & sh $S/model.sh t2ion 33410 33490 in $C & sh $S/model.sh t2cod 31510 31590 codec $C & wait
sh $S/model.sh t2off 30810 30890 in $C MGS2_OPT2=fffffff7 & sh $S/model.sh t2ioff 33410 33490 in $C MGS2_OPT2=fffffff7 &
sh $S/model.sh t2coff 31510 31590 codec $C MGS2_OPT2=fffffff7 & wait
echo models done
