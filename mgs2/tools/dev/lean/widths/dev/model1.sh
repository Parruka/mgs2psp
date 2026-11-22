#!/bin/sh
# model1.sh <build dir> <tag> [window "A B"] [vblanks]: PspProf (the cost model) over one window of free play (default the walk window
# 31501 31601, everything counted) on the build in <build dir>, v25f's pad and pack (as /home/claude/v26/model2/model.sh); dump in
# /home/claude/lean/model/<tag>, summary line in /home/claude/lean/model/sum_<tag>.txt
D=$1; t=$2; W=${3:-31501 31601}; V=${4:-31610}; O=/home/claude/lean/model
GG=/home/claude/ms_home/.ppsspp/PSP/GAME; G=$GG/MGS2LM_$t
export PPSSPP_VFPU_RN=1 PATH=/home/claude/pspsdk/pspdev/bin:$PATH
mkdir -p $O $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/split.* $G/battery.txt
cp $D/EBOOT.PBP $D/ovl_*.bin /home/claude/v25f/dl/pad.txt $G/; cp $GG/MGS2V5/SLES_503.83 $G/; ln -sf /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak
printf 'clock 333\nMGS2_HASH_EVERY=500\nMGS2_VFPU_STAT=1\nvblanks %d\ndeck 30500\nprof %s all\n' $V "$W" > $G/mgs2.cfg
F=$(psp-nm $D/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
sh /mnt/project-files/mgs2/tools/dev/prof_run.sh $G 0x$F $O/$t 4000 $O/$t.calls > $O/run_$t.out 2>&1
cp $G/log.txt $O/log_$t.txt; cp $G/mgs2.cfg $O/cfg_$t.txt
head -1 $O/$t | awk '{ i=$3; c=$5; m=$7; printf "%s: instr %.3f M  I-miss %.1f k  D-miss %.1f k  per step; %.2f ms\n", "'$t'", i/1e8, c/1e5, m/1e5, (1.4*i + 70*(c+m))/100/333000 }' > $O/sum_$t.txt
cat $O/sum_$t.txt
touch $O/done_$t
