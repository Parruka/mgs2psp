#!/bin/sh
# models_c1.sh: task #135 C1 (the GE skins and lights the faces, OPT2_C1 16, off by default) on sb11m/d (mkpsp.sh: no PGO; EBOOT
# 26bfb955, the per-vertex depth range): the codec route's window 31,510-31,590 (as T2's model), C1 off (the default: T1 and T2 on) and
# on (MGS2_OPT2=ffffffff), in parallel; fitcmp.py (tools/dev/sb6) for the pair.  Rain and splash 1/2 as gl12.
S=/home/claude/sb10; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"; export GL3=/home/claude/sb11m
sh $S/model.sh c1coff 31510 31590 codec $C & sh $S/model.sh c1con 31510 31590 codec $C MGS2_OPT2=ffffffff & wait
echo "$(date +%H:%M:%S) models done" >> /home/claude/sb11/c1x.log
