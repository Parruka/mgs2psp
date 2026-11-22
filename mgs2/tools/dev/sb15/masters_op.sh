#!/bin/sh
# task #153: the opening's rain (s87 capture, vb 8,770-8,777, fans off by up to 4.6 px today): master shots of four frames there, the
# default against C4X only (sb15m2/d)
M=/mnt/project-files/mgs2/tools/dev/sb15/c4master.sh; export B=/home/claude/sb15m2/d
echo "$(date -u +%H:%M:%S) start"
sh $M o4d fffff9df MGS2_GL_SHOT=8772,8774,8776,8778 "vblanks 9000" & sh $M o4x fffffddf MGS2_GL_SHOT=8772,8774,8776,8778 "vblanks 9000" & wait
echo "$(date -u +%H:%M:%S) done"
