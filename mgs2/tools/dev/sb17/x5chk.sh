#!/bin/sh
# x5chk.sh (task #161): the x86 twin of sb17 + patch_rainspr.py (/home/claude/sb17/x5) on routes d and i, C4 and C4X on, the plan
# recommended in deck_costs.md: the rain's sprites left out (MGS2_GL_RAIN_SPRITES=0), its streaks none (MGS2_GL_STREAKS=0), the drop sheets
# whole (MGS2_GL_RAIN=1), the foot splash 1/2 as gl16; MGS2_THIN_CHK=1 counts the sprite and quad calls by the census's owner.  Against
# x4's runs (d_c3, i_c3: streak calls 1 in 3, sheets 1/2): the same state hashes; frames differ only where a rain actor draws.  Then the
# deck route with only the sprites out (streaks and sheets as x4's d_st2) for the filter alone.
X=/home/claude/sb17/x5; C=/home/claude/sb14/chk14.sh
X=$X sh $C d d_rs MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 MGS2_THIN_CHK=1 &
X=$X sh $C i i_rs MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 MGS2_THIN_CHK=1 &
X=$X sh $C d d_rs2 MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=2 MGS2_THIN_CHK=1 &
wait
for t in d_rs i_rs d_rs2; do echo "$t $(cat $X/rc_$t.txt)"; done; echo "x5chk done $(date +%H:%M:%S)"
