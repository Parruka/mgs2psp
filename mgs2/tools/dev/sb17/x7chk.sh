#!/bin/sh
# x7chk.sh (task #161): the x86 twin of sb17 + patch_rainspr.py + patch_rainspr2.py + patch_rainspr3.py (+ patch_trim159.py, PSP-only
# code: on x86 only its counters) in /home/claude/sb17/x7: x5chk.sh's d_rs and i_rs again, now counting every sprite call left out with no
# census block (the header's third word, the vblanks).  The state hashes must equal x5's.
T=/home/claude/sb17; X=$T/x7; C=/home/claude/sb14/chk14.sh
sh $T/mk.sh $X > $T/mk_x7.out 2>&1 || { echo "x7chk: build failed"; exit 1; }
X=$X sh $C d d_rs MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 MGS2_THIN_CHK=1 &
X=$X sh $C i i_rs MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 MGS2_THIN_CHK=1 &
wait
for t in d_rs i_rs; do echo "$t $(cat $X/rc_$t.txt)"; grep -a "thin check sprites: left out with no census\|thin check sprites: sprite and quad" $X/out_$t.log; done
echo "x7chk done $(date +%H:%M:%S)"
