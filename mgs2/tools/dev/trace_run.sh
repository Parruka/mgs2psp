#!/bin/sh
# it17.sh <build dir> <tag> <deck|w01a|free|in> <out dir> <prof lines (\n-separated)> [<dumps>]: PspProf run (PSP rounding) over up to 8 windows,
# with ITRACE when IT=1 (every window, draw pipeline + GS walk fetches marked); waits for the <dumps>-th dump; profile in <out>/<tag>[.N]
B=$1; t=$2; r=$3; O=$4; PL=$5; ND=${6:-1}
GG=/home/claude/ms_home/.ppsspp/PSP/GAME
export PPSSPP_VFPU_RN=1 PATH=/home/claude/pspsdk/pspdev/bin:$PATH
mkdir -p $O; G=$GG/MGS2V17_$t; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv
cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $G/
if [ $r = free ]; then cp /home/claude/v25f/dl/pad.txt $G/; ln -sf /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak   # v26: the v25f free route
elif [ $r = in ]; then cp /mnt/project-files/mgs2/play/pad_w01af.txt $G/pad.txt; ln -sf /home/claude/v30/pak/mgs2disc.pak $G/mgs2disc.pak   # v30: the indoor route
else cp $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak; fi
printf "clock 333\nMGS2_HASH_EVERY=500\nMGS2_VFPU_STAT=1\nvblanks %s\n%b\n" "${VB:-24110}" "$PL" > $G/mgs2.cfg
[ $r = w01a ] && echo 'MGS2_POKE=21000:1190ed0:61313077,21000:1190f78:61313077,21000:1af8118:61313077,21000:1af81c0:61313077,22950:1190ed0:61313077,22950:1190f78:61313077,22950:1af8118:61313077,22950:1af81c0:61313077' >> $G/mgs2.cfg
F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
if [ "$IT" = 1 ]; then export PSPPROF_ITRACE=$O/$t.it PSPPROF_ITRACE_ALL=1 PSPPROF_ITRACE_MARK=1; fi
if [ $ND -gt 1 ]; then W=$O/$t.$((ND - 1)).calls; else W=$O/$t.calls; fi
sh /mnt/project-files/mgs2/tools/dev/prof_run.sh $G 0x$F $O/$t ${MAXS:-2400} $W > $O/run_$t.out 2>&1
cp $G/log.txt $O/log_$t.txt
echo done > $O/done_$t
