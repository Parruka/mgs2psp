#!/bin/sh
# gerruns.sh: me13 g (task #147) why a fresh texture cache changes the codec route's frames after the take-back at 36,000: two codec runs
# in the xg twin to 36,500 saving the replay files of vb 36001, 36343 and 36487 (MGS2_XLIVE_GER): gr0 without a take-back, gr7 with
# the take-back at 36,000 keeping the translator, the map and the depth window (TBK 7) but not the texture cache; compared texture by
# texture with the scratchpad's gercmp.py; log ger.log
L=/home/claude/me13/ger.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
G=/home/claude/me13/xg/ger
st "start (me13 g, replay files at 36001, 36343, 36487)"
sh /home/claude/me13/run1g.sh c gr0 36500 MGS2_XLIVE_DLF=1 MGS2_XLIVE_GER=${G}0_:36001,36343,36487 &
sh /home/claude/me13/run1g.sh c gr7 36500 MGS2_XLIVE_DLF=1 MGS2_XLIVE_GER=${G}7_:36001,36343,36487 MGS2_XLIVE_TB=36000 MGS2_XLIVE_TBK=7 &
wait
st "gr0 $(cat /home/claude/me13/xg/rc_gr0.txt), gr7 $(cat /home/claude/me13/xg/rc_gr7.txt)"
for v in 36001 36343 36487; do st "vb $v:"; PYTHONDONTWRITEBYTECODE=1 python3 -I /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/me13/gercmp.py ${G}0_$v.ger ${G}7_$v.ger >> $L 2>&1; done
st "done"
