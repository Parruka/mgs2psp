#!/bin/sh
# models_c1c.sh: as models_c1.sh on sb11m3/d (c1opt2: the trims after c1opt, EBOOT 5502f87a).  models_c1b.sh: the same on sb11m2/d (c1opt). models_c1.sh: task #135 C1 (the GE skins and lights the faces, OPT2_C1 16, off by default) on sb11m/d (mkpsp.sh: no PGO; EBOOT
# 26bfb955, the per-vertex depth range): the codec route's window 31,510-31,590 (as T2's model), C1 off (the default: T1 and T2 on) and
# on (MGS2_OPT2=ffffffff), in parallel; fitcmp.py (tools/dev/sb6) for the pair.  Rain and splash 1/2 as gl12.
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb11m3
sh $S/model.sh c3coff 31510 31590 codec $C & sh $S/model.sh c3con 31510 31590 codec $C MGS2_OPT2=ffffffff & wait
echo "$(date +%H:%M:%S) models3 done" >> /home/claude/sb11/c1x.log
