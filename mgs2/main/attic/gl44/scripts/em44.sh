#!/bin/sh
# em44.sh TAG [CFG] [MAX] (gl44: build_gl43/em43.sh with build_gl44 and pk_n44; bbox.txt and the _prev files copied too; gl43: build_gl42/em42.sh with build_gl43 and pk_n43; gl42: build_gl41/em41.sh with build_gl43, pk_n43 and the emulator under nice 19) (gl41: em40.sh with build_gl41 and pk_n41) (gl40; = build_gl39/em39.sh with build_gl40 and the package build_gl40/pk_n40 by default, PKG= another): the
# package (EBOOT.PBP, the 8 overlays, mgs2.cfg, hashref.txt, pad.txt) on the headless PPSSPP 1.20.4 as scripts/emu.sh runs
# it (PPSSPP_VFPU_RN=1, software graphics), with its own memory-stick root build_gl40/ms/em_TAG.  CFG: another cfg in place of the
# package's (a test variant); everything else is the package's.  Log, steps, battery and crash files and the pictures (shot_*, live_*,
# zbuf_*, zbm_*) are copied to build_gl40/emu_TAG.  The game folder is left for a look and deleted by hand.
. /home/claude/main/scripts/env.sh
T=$1; R=/home/claude/main/build_gl44; P=${PKG:-$R/pk_n44}; CFG=${2:-$P/mgs2.cfg}; MAX=${3:-7200}; HM=$R/ms/em_$T; GD=$HM/.ppsspp/PSP/GAME/MGS2MAIN; E=$R/emu_$T
REF=${REF:-$P/hashref.txt}   # gl42: REF= another (the h0 run: nothing native, hashref_gl38)
[ -n "$T" ] && [ -f "$CFG" ] && [ -f "$REF" ] || { echo "em44.sh TAG [CFG] [MAX]"; exit 1; }
[ -e $GD ] && { echo "em44.sh: $GD exists: look at it and delete it first"; exit 1; }
[ -e $E ] && { echo "em44.sh: $E exists"; exit 1; }
mkdir -p $GD/memcard0 $E || exit 1
cp $P/EBOOT.PBP $P/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $P/pad.txt $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
  && cp $CFG $GD/mgs2.cfg && cp $REF $GD/hashref.txt || exit 1
( cd $GD && md5sum EBOOT.PBP ovl_*.bin mgs2.cfg hashref.txt pad.txt SLES_503.83 ) > $E/inputs.md5
s=$(date +%s); cd /home/claude || exit 1
HOME=$HM PPSSPP_VFPU_RN=1 nice -n 19 $PPSSPP $GD/EBOOT.PBP --graphics=software --timeout=$MAX > $E/run.out 2>&1 &
pid=$!; echo "pid $pid" > $E/pid.txt; i=0
while [ $i -lt $MAX ] && kill -0 $pid 2>/dev/null; do
  [ -f $GD/steps.csv ] && { sleep 5; break; }
  grep -q "^end: the machine stopped" $GD/log.txt 2>/dev/null && { echo "em44.sh: STOPPED"; break; }
  sleep 5; i=$((i + 5)); done
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "em44.sh: $T $(( $(date +%s) - s )) s"
for f in log.txt steps.csv crash.txt battery.txt bbox.txt log_prev1.txt log_prev2.txt battery_prev1.txt battery_prev2.txt crash_prev1.txt crash_prev2.txt steps_prev1.csv steps_prev2.csv bbox_prev1.txt bbox_prev2.txt; do [ -f $GD/$f ] && cp $GD/$f $E/$f; done
mkdir -p $E/pic; for f in $GD/shot_*.bmp $GD/live_*.bmp $GD/zbuf_*.raw $GD/zbm_*.raw; do [ -f "$f" ] && cp "$f" $E/pic/; done
echo "em44.sh: pictures $(ls $E/pic | wc -l)"
ls $GD | grep -c "split\." | sed 's/^/em44.sh: split dumps in the game folder: /'
python3 -I $M/tools/refcmp.py --partial $REF $E/log.txt > $E/refcmp.out 2>&1; echo "refcmp rc $?" >> $E/refcmp.out; tail -3 $E/refcmp.out
