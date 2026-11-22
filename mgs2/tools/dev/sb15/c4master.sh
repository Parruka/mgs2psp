#!/bin/sh
# c4master.sh <tag> <MGS2_OPT2 hex> [extra cfg lines...]: task #152, step 4's pictures: the PPSSPP master (exact picture reference,
# tools/dev/ppsspp_master_run.sh) on the model build /home/claude/sb15m/d (mkpsp.sh, no PGO) with gl16's delivery settings and shots,
# g_opt2 fixed for the run (gl16's turns removed): fffffddf = the default (C4 off, as gl16), ffffffdf = C4 on.  Game data passes through
# it: personal use only; the shots stay in the container or go to Ffm's PC folder only.
t=$1; o=$2; shift 2
M=/home/claude/msm/PSP/GAME/MGS2M_$t; S16=/home/claude/msm/PSP/GAME/MGS2M_gl16; B=${B:-/home/claude/sb15m/d}
mkdir -p $M/memcard0 && cp $B/EBOOT.PBP $B/ovl_*.bin $M/ && cp $S16/SLES_503.83 $S16/pad.txt $S16/hashref.txt $M/ || exit 1
ln -sf /home/claude/v30/pak/mgs2disc.pak $M/mgs2disc.pak
grep -v '^MGS2_OPT_CYCLE=\|^MGS2_OPT2_CYCLE=\|^MGS2_EXC=' /home/claude/gl16/mgs2.cfg > $M/mgs2.cfg
{ echo "MGS2_EXC=0"; echo "MGS2_OPT2=$o"; for l in "$@"; do echo "$l"; done; } >> $M/mgs2.cfg
rm -f $M/log.txt $M/steps.csv $M/battery.txt $M/crash.txt $M/shot_*.bmp
sh /mnt/project-files/mgs2/tools/dev/ppsspp_master_run.sh $M /home/claude/sb15m/master_$t.out
echo "rc $?" >> /home/claude/sb15m/master_$t.out
