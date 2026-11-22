#!/bin/sh
# master159.sh (task #159): the PPSSPP master of the model build sb17m4/d (sb17 + patch_rainspr.py + patch_trim159.py, mkpsp17.sh, no PGO,
# EBOOT e3427fc3) with s17r's settings: every hash, digest and shot must equal s17r's (sb17m3/d, without the trim), and the break's
# highest point should fall back to where gl16's stands above the heap's start (s17r: 0b85f000 at vb 10,849, 92 KB above the end's break).
B=/home/claude/sb17m4/d sh /mnt/project-files/mgs2/tools/dev/sb15/c4master.sh s17t159 ffffffdf MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1
echo "master159 done $(date +%H:%M:%S)"
