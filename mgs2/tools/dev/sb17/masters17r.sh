#!/bin/sh
# masters17r.sh (task #161): the PPSSPP master pictures of the recommended plan on the model build sb17m3/d (sb17 + patch_rainspr.py,
# mkpsp17.sh, no PGO, EBOOT 8a41bee9): s17r = C4 + C4X on, the rain's sprites left out (MGS2_GL_RAIN_SPRITES=0), its streaks none, the
# drop sheets whole.  Against s17n (sb17m/d: the same without the sprite filter) the frames differ only where the rain's sprites draw.
C=/mnt/project-files/mgs2/tools/dev/sb15/c4master.sh
B=/home/claude/sb17m3/d sh $C s17r ffffffdf MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1
echo "masters17r done $(date +%H:%M:%S)"
