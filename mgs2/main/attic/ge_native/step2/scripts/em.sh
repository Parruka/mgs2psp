#!/bin/sh
# em.sh TAG CFG [MAX] (st2: st1g/em_b.sh with st2s folders; RF= the hashref, default the packages): EMULATOR ONLY
# build_gl39/cfg/hashref_gl38.txt, build_gl38/pk_f38/pad.txt) with CFG as mgs2.cfg on the headless PPSSPP 1.20.4 as scripts/emu.sh
# runs it (PPSSPP_VFPU_RN=1, software graphics, nice 19), its memory-stick root st1g/ms/em_TAG; log, steps and pictures to st1g/emu_TAG
# (container only).  The game folder is deleted at the end (it holds the game's files).
. /home/claude/main/scripts/env.sh
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st2
T=$1; P=${PKG:-$S/pk}; CFG=$2; MAX=${3:-3600}; HM=$S/ms/em_$T; GD=$HM/.ppsspp/PSP/GAME/MGS2MAIN; E=$S/emu_$T; REF=${RF:-$P/hashref.txt}
[ -n "$T" ] && [ -f "$CFG" ] && [ -f "$REF" ] || { echo "em.sh TAG CFG [MAX]"; exit 1; }
[ -e $GD ] && { echo "em.sh: $GD exists"; exit 1; }; [ -e $E ] && { echo "em.sh: $E exists"; exit 1; }
mkdir -p $GD/memcard0 $E || exit 1
cp $P/EBOOT.PBP $P/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $P/pad.txt $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
  && cp $CFG $GD/mgs2.cfg && cp $REF $GD/hashref.txt || exit 1
( cd $GD && md5sum EBOOT.PBP ovl_*.bin mgs2.cfg hashref.txt pad.txt SLES_503.83 ) > $E/inputs.md5
s=$(date +%s); cd $S || exit 1
HOME=$HM PPSSPP_VFPU_RN=1 nice -n 19 $PPSSPP $GD/EBOOT.PBP --graphics=software --timeout=$MAX > $E/run.out 2>&1 &
pid=$!; echo "pid $pid" > $E/pid.txt; i=0
while [ $i -lt $MAX ] && kill -0 $pid 2>/dev/null; do
  [ -f $GD/steps.csv ] && { sleep 5; break; }
  grep -q "^end: the machine stopped" $GD/log.txt 2>/dev/null && { echo "em.sh: STOPPED"; break; }
  sleep 5; i=$((i + 5)); done
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "em.sh: $T $(( $(date +%s) - s )) s"
for f in log.txt steps.csv crash.txt battery.txt; do [ -f $GD/$f ] && cp $GD/$f $E/$f; done
mkdir -p $E/pic; for f in $GD/shot_*.bmp $GD/live_*.bmp; do [ -f "$f" ] && cp "$f" $E/pic/; done
echo "em.sh: pictures $(ls $E/pic | wc -l)"
python3 -I /home/claude/main/tools/refcmp.py --partial $REF $E/log.txt > $E/refcmp.out 2>&1; echo "refcmp rc $?" >> $E/refcmp.out; tail -3 $E/refcmp.out
rm -f "$GD/SLES_503.83" "$GD/mgs2disc.pak"
