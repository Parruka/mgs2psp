#!/bin/sh
# masters4x.sh: task #153, the PPSSPP master (c4master.sh: the indoor route to vb 37,800 with gl16's settings and 16 shots, g_opt2 fixed)
# on the PSP model build /home/claude/sb15m2/d (mkpsp.sh from sb15 with 4x and the shared scratch; no PGO): the default fffff9df (must
# equal gl16's master), C4X only fffffddf, C4 + C4X ffffffdf.  Folders /home/claude/msm/PSP/GAME/MGS2M_x4d, _x4x, _x4a.
M=/mnt/project-files/mgs2/tools/dev/sb15/c4master.sh; export B=/home/claude/sb15m2/d
sh $M x4d fffff9df & sh $M x4x fffffddf & wait
sh $M x4a ffffffdf
