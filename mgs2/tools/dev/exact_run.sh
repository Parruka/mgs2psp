#!/bin/sh
# exact17.sh <build dir> <tag> <cfg> <hashref> <out dir>: the delivered mgs2.cfg (+ a 1-vblank PspProf window at 25195 so the run ends
# there) with the split finder against <hashref>, JIT throughout (PSP rounding); log in <out>/log_<tag>.txt
B=$1; t=$2; C=$3; H=$4; O=$5
GG=/home/claude/ms_home/.ppsspp/PSP/GAME
export PPSSPP_VFPU_RN=1 PATH=/home/claude/pspsdk/pspdev/bin:$PATH
mkdir -p $O; G=$GG/MGS2V17_$t; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/split.*
cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak
cp $C $G/mgs2.cfg; echo 'prof 25195 25196' >> $G/mgs2.cfg; cp $H $G/hashref.txt
F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
sh /mnt/project-files/mgs2/tools/dev/prof_run.sh $G 0x$F $O/x_$t ${MAXS:-1500} > $O/run_$t.out 2>&1
cp $G/log.txt $O/log_$t.txt; ls $G/split.* 2>/dev/null > $O/splitfiles_$t.txt
echo done > $O/done_$t
