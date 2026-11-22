#!/bin/sh
# v28me_emu.sh (container copy /home/claude/v28/meemu/run.sh; as /home/claude/v26me4/emu/run.sh) <build dir> <tag> <MGS2_SPU_ME value>: the files v28 ships (deliver/mgs2.cfg
# with its MGS2_SPU_ME line set to the value, deliver/hashref.txt = prof/hashref_x86_free_v28.txt) with v25f's pad and pack, on a
# v28 ME build in the container's PPSSPP headless (JIT, PSP rounding), stopped at "disc pack:".  XCFG: lines added to mgs2.cfg
# (MGS2_EXC_DRY=<vb>).  A tag gets a fresh game folder (a tag already used is refused).  Out: /home/claude/v28/meemu/log_<tag>.txt,
# battery_<tag>.txt, steps_<tag>.csv, me_steps_<tag>.csv, crash_<tag>.txt, cfg_<tag>.txt
B=$1; t=$2; ME=$3; MAX=${MAXS:-3600}; O=/home/claude/v28/meemu; D=/home/claude/v28/deliver
GG=/home/claude/ms_home/.ppsspp/PSP/GAME; G=$GG/MGS2V28ME_$t
export PPSSPP_VFPU_RN=1
[ -e $G ] && { echo "tag $t used already" > $O/run_$t.out; exit 1; }
mkdir -p $O $G/memcard0
cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp $D/hashref.txt $G/; cp /home/claude/v25f/dl/pad.txt $G/; cp $GG/MGS2V5/SLES_503.83 $G/
ln -s /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak
sed "s/^MGS2_SPU_ME=.*/MGS2_SPU_ME=$ME/" $D/mgs2.cfg > $G/mgs2.cfg
for x in $XCFG; do printf '%s\n' "$x" >> $G/mgs2.cfg; done
cp $G/mgs2.cfg $O/cfg_$t.txt
cd /home/claude
HOME=/home/claude/ms_home /home/claude/ppsspp_src/ppsspp-1.20.4/build/PPSSPPHeadless $G/EBOOT.PBP --graphics=software --timeout=$MAX > $O/run_$t.out 2>&1 &
P=$!
i=0; while [ $i -lt $MAX ] && kill -0 $P 2>/dev/null; do if grep -q "^disc pack:" $G/log.txt 2>/dev/null; then break; fi; sleep 5; i=$((i+5)); done
kill $P 2>/dev/null; wait $P 2>/dev/null
echo "DONE after $i s" >> $O/run_$t.out
cp $G/log.txt $O/log_$t.txt; cp $G/battery.txt $O/battery_$t.txt 2>/dev/null; cp $G/steps.csv $O/steps_$t.csv 2>/dev/null; cp $G/me_steps.csv $O/me_steps_$t.csv 2>/dev/null; cp $G/crash.txt $O/crash_$t.txt 2>/dev/null
touch $O/$t.done
