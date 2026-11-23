#!/bin/sh
# em41.sh TAG PKG [CFG] [MAX] (gl41s; build_gl39/em39.sh with its folders in the scratch): the package PKG (EBOOT.PBP, the 8 overlays,
# mgs2.cfg, hashref.txt, pad.txt) on the normal headless PPSSPP 1.20.4 (the JIT; PPSSPP_VFPU_RN=1, software graphics), memory-stick root
# ms/em_TAG; log, steps and battery files to emu_TAG; then refcmp --partial against the package's hashref.  The game folder (game data:
# the disc image's link, SLES) is deleted by the caller with its literal path.
. /home/claude/main/scripts/env.sh
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s
T=$1; P=$2; CFG=${3:-$P/mgs2.cfg}; MAX=${4:-3600}; HM=$S/ms/em_$T; GD=$HM/.ppsspp/PSP/GAME/MGS2MAIN; E=$S/emu_$T; REF=$P/hashref.txt
[ -n "$T" ] && [ -f "$CFG" ] && [ -f "$REF" ] || { echo "em41.sh TAG PKG [CFG] [MAX]"; exit 1; }
[ -e $GD ] && { echo "em41.sh: $GD exists"; exit 1; }; [ -e $E ] && { echo "em41.sh: $E exists"; exit 1; }
mkdir -p $GD/memcard0 $E || exit 1
cp $P/EBOOT.PBP $P/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $P/pad.txt $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
  && cp $CFG $GD/mgs2.cfg && cp $REF $GD/hashref.txt || exit 1
( cd $GD && md5sum EBOOT.PBP ovl_*.bin mgs2.cfg hashref.txt pad.txt ) > $E/inputs.md5
s=$(date +%s); cd /home/claude || exit 1
HOME=$HM PPSSPP_VFPU_RN=1 nice -n 10 $PPSSPP $GD/EBOOT.PBP --graphics=software --timeout=$MAX > $E/run.out 2>&1 &
pid=$!; echo "pid $pid" > $E/pid.txt; i=0
while [ $i -lt $MAX ] && kill -0 $pid 2>/dev/null; do
  [ -f $GD/steps.csv ] && { sleep 5; break; }
  grep -q "^end: the machine stopped" $GD/log.txt 2>/dev/null && { echo "em41.sh: STOPPED"; break; }
  sleep 5; i=$((i + 5)); done
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "em41.sh: $T $(( $(date +%s) - s )) s"
for f in log.txt steps.csv crash.txt battery.txt; do [ -f $GD/$f ] && cp $GD/$f $E/$f; done
ls $GD | grep -c "split\." | sed 's/^/em41.sh: split dumps in the game folder: /'
python3 -I $M/tools/refcmp.py --partial $REF $E/log.txt > $E/refcmp.out 2>&1; echo "refcmp rc $?" >> $E/refcmp.out; tail -3 $E/refcmp.out
