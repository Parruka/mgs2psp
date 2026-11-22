#!/bin/sh
# em30.sh TAG CFG [MAX] (gl30, 2026-11-20; em29.sh pointing at build_gl30): gl30's EBOOT (build_gl30/psp) on the headless PPSSPP 1.20.4 as scripts/emu.sh runs it (PPSSPP_VFPU_RN=1, software
# graphics), with its own memory-stick root /home/claude/ms_gl30_TAG, the cfg CFG and hashref_g22b; log, steps, battery, crash files and
# the pictures (shot_*, live_*, zbuf_*, zbm_*) copied to build_gl30/emu_TAG.  The game folder is left for a look and deleted by hand.
. /home/claude/main/scripts/env.sh
T=$1; CFG=$2; MAX=${3:-3600}; R=/home/claude/main/build_gl30; HM=/home/claude/ms_gl30_$T; GD=$HM/.ppsspp/PSP/GAME/MGS2MAIN; E=$R/emu_$T
REF=$M/cfg/hashref_g22b.txt; P=$R/psp
[ -n "$T" ] && [ -f "$CFG" ] || { echo "em30.sh TAG CFG [MAX]"; exit 1; }
[ -e $GD ] && { echo "em30.sh: $GD exists: look at it and delete it first"; exit 1; }
[ -e $E ] && { echo "em30.sh: $E exists"; exit 1; }
mkdir -p $GD/memcard0 $E || exit 1
cp $P/EBOOT.PBP $P/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $PADF $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
  && cp $CFG $GD/mgs2.cfg && cp $REF $GD/hashref.txt || exit 1
md5sum $GD/EBOOT.PBP $GD/mgs2.cfg > $E/inputs.md5
s=$(date +%s); cd /home/claude || exit 1
HOME=$HM PPSSPP_VFPU_RN=1 $PPSSPP $GD/EBOOT.PBP --graphics=software --timeout=$MAX > $E/run.out 2>&1 &
pid=$!; echo "pid $pid" > $E/pid.txt; i=0
while [ $i -lt $MAX ] && kill -0 $pid 2>/dev/null; do
  [ -f $GD/steps.csv ] && { sleep 5; break; }
  grep -q "^end: the machine stopped" $GD/log.txt 2>/dev/null && { echo "em30.sh: STOPPED"; break; }
  sleep 5; i=$((i + 5)); done
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "em30.sh: $T $(( $(date +%s) - s )) s"
for f in log.txt steps.csv crash.txt battery.txt; do [ -f $GD/$f ] && cp $GD/$f $E/$f; done
mkdir -p $E/pic; for f in $GD/shot_*.bmp $GD/live_*.bmp $GD/zbuf_*.raw $GD/zbm_*.raw; do [ -f "$f" ] && cp "$f" $E/pic/; done
echo "em30.sh: pictures $(ls $E/pic | wc -l)"
python3 -I $M/tools/refcmp.py --partial $REF $E/log.txt > $E/refcmp.out 2>&1; echo "refcmp rc $?" >> $E/refcmp.out; tail -3 $E/refcmp.out
