#!/bin/bash
# gl4c_read.sh <run folder: log.txt battery.txt crash.txt [steps.csv] [shot_*.bmp]> [emulator tag, default codec] (task #78, 2026-10-28)
# Reads a gl4c PSP run: md5s, hashes against the x86 references and the emulator run, where it ended, crash.txt, the battery lines along
# route c, the shots and the step cost per part against the emulator run in $E (default /home/claude/gl4c/emu: gl3.sh emu codec output).
D=$1; K=${2:-codec}; E=${E:-/home/claude/gl4c/emu}; MG=/mnt/project-files/mgs2; T=$MG/tools/dev
cd "$D" || exit 1
echo "== files"; md5sum * 2>/dev/null | awk '{printf "%s %s\n", substr($1, 1, 8), $2}'
echo "== hashes against the x86 hashref (ee, vfb; vuh)"; sh $T/emucmp.sh log.txt $MG/prof/hashref_x86_codec_h2.txt
echo "== hashes against the x86 hash file (ee, iop, vfb)"; sh $T/emucmp.sh log.txt $MG/prof/hashes_x86_codec_h2.txt
echo "== against the emulator's log: hash and vuh lines"
n=$(grep -ac '^\(hash\|vuh\):' log.txt); m=$(diff <(grep -a '^\(hash\|vuh\):' log.txt) <(grep -a '^\(hash\|vuh\):' $E/log_$K.txt | head -n $n) | grep -c '^<')
echo "$n lines, $m differ from the first $n of the emulator's ($(grep -ac '^\(hash\|vuh\):' $E/log_$K.txt) there)"
echo "== where it ended"; grep -a '^hash:' log.txt | tail -n 1 | cut -c1-60; grep -a '^window' log.txt | tail -n 1 | cut -c1-90
grep -a -E '^(end|split|picture: [0-9]+ frames|GE|gl_sync)' log.txt | cut -c1-200 | head -n 8
tail -n 3 log.txt | cut -c1-200
echo "== crash.txt"; cat crash.txt
echo "== battery along route c"; python3 -I -B $T/sysdiag_read.py battery.txt --route c --tail 6
echo "== shots against the emulator's"
for f in shot_*.bmp; do [ -f "$f" ] || continue; e=$E/shots_$K/$f; [ -f $e ] && { [ "$(md5sum < $f)" = "$(md5sum < $e)" ] && echo "$f equal" || echo "$f DIFFERS"; } || echo "$f: no emulator shot"; done
echo "== step cost per part (log windows: mean ms a step), PSP / emulator"
for r in "story codec 1:15700:19900" "story codec 2:26100:28800" "Otacon calls:30600:60000"; do
  IFS=: read nm a b <<< "$r"
  p=$(awk -v a=$a -v b=$b '/^window vb/ {v=$3; sub(":","",v); v+=0; if (v>=a && v<=b) {s+=$7; n++}} END {if (n) printf "%.2f (%d)", s/n, n; else printf "-"}' log.txt)
  q=$(awk -v a=$a -v b=$b '/^window vb/ {v=$3; sub(":","",v); v+=0; if (v>=a && v<=b) {s+=$7; n++}} END {if (n) printf "%.2f (%d)", s/n, n; else printf "-"}' $E/log_$K.txt)
  echo "$nm (vb $a-$b): PSP $p, emulator $q"; done
