#!/bin/bash
# emuchk6.sh: sb6's emulator run of the whole indoor route with gl10's delivery cfg (GL3=/home/claude/sb6m TAG=s6 sh psp/gl3.sh emu hw;
# dl_cfg.txt = gl10's, so lane 3 is on in all of MGS2_OPT_CYCLE's settings) against gl10's (sbg10 hwg10) and sb5's (sb5m hws5): the hash
# and vuh lines, the picture lines, the 199 frame digests of vb 26,061-26,459 (MGS2_GL_DIG) against hwg10's and the x86 twin's indoor run
# with lanes 1-4 on (x1/out_i2on.log.gz, tools/dev/gl1_cmp.py), the shots byte for byte, the memory lines.  renderer_notes 11.33.
E=/home/claude/sb6m/emu; G=/home/claude/sbg10/emu; F=/home/claude/sb5m/emu; MG=/mnt/project-files/mgs2
PY="env PYTHONDONTWRITEBYTECODE=1 python3 -I"; L=$E/log_hws6.txt
cat $E/run_hws6.out | tail -n 2
for r in $G/log_hwg10.txt $F/log_hws5.txt; do
  for k in '^hash:' '^vuh:' '^split' '^picture: ' '^picture: vb [0-9]* buffer'; do
    printf "%-14s %-28s hws6 %5s, it %5s lines, differ %s\n" "$(basename $r)" "$k" "$(grep -c "$k" $L)" "$(grep -c "$k" $r)" \
      "$(diff <(grep "$k" $L) <(grep "$k" $r) | grep -c '^[<>]')"
  done
done
echo "== picture lines that differ from hwg10 (first 6)"; diff <(grep '^picture: ' $L) <(grep '^picture: ' $G/log_hwg10.txt) | head -n 12 | cut -c1-220
echo "== gl1_cmp.py hws6 vs the x86 twin's i2on (lanes 1-4 on)"; $PY $MG/tools/dev/gl1_cmp.py $L <(zcat /home/claude/sb6/x1/out_i2on.log.gz); echo "rc $?"
echo "== gl1_cmp.py hws6 vs hwg10"; $PY $MG/tools/dev/gl1_cmp.py $L $G/log_hwg10.txt; echo "rc $?"
echo "== shots"; n=0; d=0
for s in $E/shots_hws6/*.bmp; do b=$(basename $s); n=$((n+1)); cmp -s $s $G/shots_hwg10/$b || { d=$((d+1)); echo "differs from hwg10: $b"; }
  cmp -s $s $F/shots_hws5/$b || echo "differs from hws5: $b"; done
echo "shots: $n, differ from hwg10 $d; hwg10 has $(ls $G/shots_hwg10 | wc -l), hws5 $(ls $F/shots_hws5 | wc -l)"
echo "== memory"; for r in $L $F/log_hws5.txt $G/log_hwg10.txt; do echo "$(basename $r):"; grep 'free memory now\|^memory' $r | tail -n 3 | cut -c1-200; done
echo "== other lines that differ from hws5 (numbers kept; the timing lines left out)"
diff <(grep -v '^hash:\|^vuh:\|^picture: \|ms\b\|ms a \| ms\|KB\|battery\|temp' $L) <(grep -v '^hash:\|^vuh:\|^picture: \|ms\b\|ms a \| ms\|KB\|battery\|temp' $F/log_hws5.txt) | head -n 30 | cut -c1-200
