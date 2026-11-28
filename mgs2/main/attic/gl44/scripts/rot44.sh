#!/bin/sh
# rot44.sh (gl44 block A, H1's emulator test): three starts of the package pk_n44 in ONE game folder (build_gl44/ms/em_rot), with
# cfg_emu/mgs2_gl44_rot.cfg (gl44's cfg, 700 vblanks, steps timed from 400, the black box on):
#   start 1 runs to its end line, start 2 is killed at the window of vb 300 (its log has no end line), start 3 runs to its end line.
# After each start the folder's files and their md5s go to emu_rot/after<k>.md5, the new log's H1 lines to emu_rot/h1_<k>.txt; the files
# themselves are kept as emu_rot/s<k>/.  The checks (rot44_chk.py) then compare the generations.  The game folder is deleted at the end.
. /home/claude/main/scripts/env.sh
R=/home/claude/main/build_gl44; P=${PKG:-$R/pk_n44}; CFG=$R/cfg_emu/mgs2_gl44_rot.cfg; HM=$R/ms/em_rot; GD=$HM/.ppsspp/PSP/GAME/MGS2MAIN; E=${E:-$R/emu_rot}   # PKG=, E=: another package / output folder
[ -e $GD ] && { echo "rot44.sh: $GD exists"; exit 1; }
[ -e $E ] && { echo "rot44.sh: $E exists"; exit 1; }
mkdir -p $GD/memcard0 $E || exit 1
cp $P/EBOOT.PBP $P/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $P/pad.txt $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
  && cp $CFG $GD/mgs2.cfg && cp $P/hashref.txt $GD/hashref.txt || exit 1
printf 'rot44 stand-in shot (not a picture)\n' > $GD/shot_1.bmp; md5sum < $GD/shot_1.bmp > $E/shot_before.md5   # shots are not rotated
FILES="log.txt battery.txt crash.txt steps.csv bbox.txt log_prev1.txt battery_prev1.txt crash_prev1.txt steps_prev1.csv bbox_prev1.txt log_prev2.txt battery_prev2.txt crash_prev2.txt steps_prev2.csv bbox_prev2.txt"
one() {   # <k> <end | kill300>
  k=$1; m=$2; s=$(date +%s); cd /home/claude || exit 1
  old=$(md5sum < $GD/log.txt 2>/dev/null || echo none)   # the run before's log: its lines must not stop this start (H1 renames it first)
  HOME=$HM PPSSPP_VFPU_RN=1 nice -n 19 $PPSSPP $GD/EBOOT.PBP --graphics=software --timeout=900 > $E/run$k.out 2>&1 &
  pid=$!; i=0; why=timeout
  while [ $i -lt 900 ] && kill -0 $pid 2>/dev/null; do
    if [ ! -f $GD/log.txt ] || [ "$(md5sum < $GD/log.txt)" = "$old" ]; then sleep 1; i=$((i + 1)); continue; fi   # still the old log
    if [ $m = end ]; then grep -aq "^gl44 black box (MGS2_BBOX 1): bbox.txt made at" $GD/log.txt 2>/dev/null && { sleep 3; why=end; break; }   # (the end's report line)
    else grep -aq "^window vb 300:" $GD/log.txt 2>/dev/null && { why=kill300; break; }; fi
    sleep 1; i=$((i + 1)); done
  kill $pid 2>/dev/null; wait $pid 2>/dev/null
  echo "rot44.sh: start $k ($m): stopped by $why after $(( $(date +%s) - s )) s"
  mkdir -p $E/s$k; ( cd $GD && for f in $FILES; do [ -f $f ] && { md5sum $f; cp $f $E/s$k/$f; }; done ) > $E/after$k.md5
  grep -a "^gl44 H1" $GD/log.txt > $E/h1_$k.txt; grep -ac "^end: " $GD/log.txt | sed "s/^/rot44.sh: start $k: end lines in log.txt: /"
  cat $E/after$k.md5 | awk '{ print "  " $2 " " substr($1, 1, 8) }'; cat $E/h1_$k.txt | cut -c1-400
}
one 1 end; one 2 kill300; one 3 end
ls -la $GD > $E/folder_final.txt; md5sum < $GD/shot_1.bmp > $E/shot_after.md5
[ -d "/home/claude/main/build_gl44/ms/em_rot/.ppsspp" ] && rm -rf "/home/claude/main/build_gl44/ms/em_rot"
python3 -I $R/rot44_chk.py $E; echo "rot44.sh: done $(date -u +%H:%M:%S)"
