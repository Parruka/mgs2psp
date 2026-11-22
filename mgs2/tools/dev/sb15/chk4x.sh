#!/bin/sh
# chk4x.sh: task #153 (4x) and the shared scratch, the x86 checks (stagec_notes.md 4.7, 4.8).  Twin builds from /home/claude/sb15 with
# mk.sh: X4 = the tree before 4x (sb15pre4x), X6 = 4x, X7 = 4x + the effect converters' shared scratch.  Each run: /home/claude/sb14/chk14.sh
# (route d: the deck to vb 25,200; i: indoors to 37,800; rain and splash 1/2, lanes 1-4) with MGS2_GX_SEM=1 (each frame's drawn digest).
# Compare the "picture: vb N drawn", "picture: vb N buffer" and "hash" lines between runs:
#   X4 fffffddf (C4 off, the old words) = X6 fffff9df (the default) on d and i;  X6 ffffffdf = X7 ffffffdf on d.
#   X6 fffffbdf (C4), fffffddf (C4X), ffffffdf (both): the C4X frames, the census lines ("converters", "effects C4", "sharing census").
C=/home/claude/sb14/chk14.sh; X4=${X4:-/home/claude/sb15/x4}; X6=${X6:-/home/claude/sb15/x6}; X7=${X7:-/home/claude/sb15/x7}
for w in fffff9df fffffbdf fffffddf ffffffdf; do X=$X6 sh $C d d_$w MGS2_OPT2=$w MGS2_GX_SEM=1 & done; wait
X=$X4 sh $C d d_old_off MGS2_OPT2=fffffddf MGS2_GX_SEM=1 & X=$X4 sh $C i i_old_off MGS2_OPT2=fffffddf MGS2_GX_SEM=1 &
X=$X6 sh $C i i_fffff9df MGS2_OPT2=fffff9df MGS2_GX_SEM=1 & X=$X7 sh $C d d_ffffffdf MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 & wait
