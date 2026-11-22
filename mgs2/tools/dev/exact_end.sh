#!/bin/sh
# exact_end.sh <build dir> <tag> <cfg> <hashref> <out dir>: exact_run.sh run to the harness's own end instead of a PspProf window (so the
# last hashes, the end-of-run memory report and steps.csv are there): the delivered mgs2.cfg with the split finder against <hashref>, JIT
# throughout (PSP rounding), stopped once log.txt holds the summary's last line ("disc pack:", v24); log and steps.csv in <out>
B=$1; t=$2; C=$3; H=$4; O=$5; MAX=${MAXS:-2400}
GG=/home/claude/ms_home/.ppsspp/PSP/GAME
export PPSSPP_VFPU_RN=1
mkdir -p $O; G=$GG/MGS2V17_$t; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/split.*
cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak
cp $C $G/mgs2.cfg; cp $H $G/hashref.txt
cd /home/claude
HOME=/home/claude/ms_home /home/claude/ppsspp_src/ppsspp-1.20.4/build/PPSSPPHeadless $G/EBOOT.PBP --graphics=software --timeout=$MAX > $O/run_$t.out 2>&1 &
P=$!
i=0; while [ $i -lt $MAX ] && kill -0 $P 2>/dev/null; do if grep -q "^disc pack:" $G/log.txt 2>/dev/null; then break; fi; sleep 5; i=$((i+5)); done
kill $P 2>/dev/null; wait $P 2>/dev/null
echo "DONE after $i s" >> $O/run_$t.out
cp $G/log.txt $O/log_$t.txt; cp $G/steps.csv $O/steps_$t.csv 2>/dev/null; ls $G/split.* 2>/dev/null > $O/splitfiles_$t.txt
