#!/bin/sh
# x8chk.sh (task #161): the x86 twin of sb17 + patch_rainspr.py + patch_rainspr2-4.py (+ patch_trim159.py, PSP-only code: on x86 only its
# counters) in /home/claude/sb17/x8: x5chk.sh's d_rs and i_rs again, now with the rain's four sprite actors' calls by actor and by part of
# the route.  The state hashes must equal x5's.
T=/home/claude/sb17; X=$T/x8; C=/home/claude/sb14/chk14.sh
sh $T/mk.sh $X > $T/mk_x8.out 2>&1 || { echo "x8chk: build failed"; exit 1; }
X=$X sh $C d d_rs MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 MGS2_THIN_CHK=1 &
X=$X sh $C i i_rs MGS2_OPT2=ffffffdf MGS2_GX_SEM=1 MGS2_GL_RAIN_SPRITES=0 MGS2_GL_STREAKS=0 MGS2_GL_RAIN=1 MGS2_THIN_CHK=1 &
wait
for t in d_rs i_rs; do echo "$t $(cat $X/rc_$t.txt)"; grep -a "thin check sprites" $X/out_$t.log | grep -v ": vb "; done
echo "x8chk done $(date +%H:%M:%S)"
