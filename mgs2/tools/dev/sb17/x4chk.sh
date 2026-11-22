#!/bin/sh
# x4chk.sh (task #158): the x86 twin of sb17 with MGS2_GL_STREAK_CALLS=3 (the rain's streak calls 1 in 3, whole calls by block), routes d
# and i, C4 and C4X on, rain and splash 1/2 as gl16; then the deck route with the setting unset (must equal x1's st... runs) is not needed:
# unset = the code path before (gl_line_none's kcall 1).  Against sb17/x1 (streaks 1/2) and x2 (none): the same hashes, frames differ
# only where a rain actor draws.
X=/home/claude/sb17/x4; C=/home/claude/sb14/chk14.sh
X=$X sh $C d d_c3 MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_STREAK_CALLS=3 &
X=$X sh $C i i_c3 MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_STREAK_CALLS=3 &
X=$X sh $C d d_st2 MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_STREAKS=2 &
wait
for t in d_c3 i_c3 d_st2; do echo "$t $(cat $X/rc_$t.txt)"; done; echo "x4chk done $(date +%H:%M:%S)"
