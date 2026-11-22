#!/bin/sh
# x2chk.sh (task #158): the x86 twin of sb17 with MGS2_GL_STREAKS=0 (the rain's streak calls left out whole), routes d and i, C4 and C4X on,
# rain and splash 1/2 as gl16; against sb17/x1's out_d_st2 / out_i_st2 (streaks 1/2): the same hashes, frames equal but where a rain actor draws.
X=/home/claude/sb17/x2; C=/home/claude/sb14/chk14.sh
sh /home/claude/sb17/mk.sh $X > /home/claude/sb17/mk_x2.out 2>&1 || { echo "build failed"; exit 1; }
X=$X sh $C d d_st0 MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_STREAKS=0 &
X=$X sh $C i i_st0 MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_STREAKS=0 &
wait
for t in d_st0 i_st0; do echo "$t $(cat $X/rc_$t.txt)"; done
