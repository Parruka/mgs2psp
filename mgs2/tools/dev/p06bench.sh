#!/bin/sh
# p06bench.sh <build dir with EBOOT.PBP and p06bench.elf> <out prefix> <dump>...: tools/dev/p06bench.c in headless PPSSPP (JIT, the
# PspProf interpreter while g_emuprof is set: tools/ppsspp_mgs2.diff), the dumps linked into the game folder (game data: container only).
# Writes <out>.txt (bench.txt: both passes' hash and counts) and the PspProf dump <out> (tools/dev/prof_fn.py <elf> <out> 1).
B=$1; OUT=$2; shift 2
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH PPSSPP_VFPU_RN=1
G=/home/claude/ms_home/.ppsspp/PSP/GAME/${P6G:-P06BENCH}   # P6G: another game folder, for runs side by side
mkdir -p $G; rm -f $G/bench.txt $G/list.txt $G/p6_*.bin $OUT $OUT.calls $OUT.txt
cp $B/EBOOT.PBP $G/
for d in "$@"; do ln -sf $d $G/$(basename $d); basename $d >> $G/list.txt; done
F=$(psp-nm $B/p06bench.elf | awk '$3=="g_emuprof"{print $1}')
cd /home/claude
HOME=/home/claude/ms_home PSPPROF_FLAG=$F PSPPROF_OUT=$OUT PSPPROF_SWITCH=1 /home/claude/ppsspp_src/ppsspp-1.20.4/build/PPSSPPHeadless $G/EBOOT.PBP --graphics=software --timeout=${MAXS:-1800} > $OUT.run 2>&1
cp $G/bench.txt $OUT.txt 2>/dev/null; cat $OUT.txt; grep PspProf $OUT.run | head -c 600
