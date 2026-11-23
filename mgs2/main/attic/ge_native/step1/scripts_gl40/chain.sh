#!/bin/sh
# chain.sh (st1g, gl40 + step 1): after mk.sh and pspb.sh: the twin off and on (check mode, XP check in every turn) runs, then the
# package st1g/pk and the emulator runs g0 (off), g1 and g1b (on, 256 KB), one at a time.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1g; cd $S || exit 1
grep -q "^boot link rc 0" mk.out || { echo "chain: twin link failed"; exit 1; }
grep -q "^rc 0" pspb.out || { echo "chain: psp build failed"; exit 1; }
sh run.sh off > run_off.out 2>&1; cat bld/rc_off.txt
sh run.sh on MGS2_NGE=1 MGS2_NGE_CHK=1 MGS2_XP_CYCLE=77777777777777777777777777777777777777777777777777777777777777777777777777 > run_on.out 2>&1; cat bld/rc_on.txt
mkdir -p pk && cp psp/EBOOT.PBP psp/ovl_*.bin pk/ && cp /home/claude/main/build_gl40/deliver/hashref.txt /home/claude/main/build_gl40/deliver/pad.txt pk/ && (cd pk && md5sum EBOOT.PBP hashref.txt)
for t in g0:nge0 g1:nge1 g1b:nge1; do sh em.sh ${t%%:*} cfg/${t#*:}.cfg 5400 > em_${t%%:*}.out 2>&1; cat em_${t%%:*}.out; done
echo "chain: done $(date -u +%H:%M:%S)"
