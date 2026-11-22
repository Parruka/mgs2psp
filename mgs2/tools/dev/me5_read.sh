#!/bin/bash
# me5_read.sh <run folder> [<log of an earlier finished run, for the vuh lines>]: reads a v30me5 run on v30's indoor route (log.txt,
# battery.txt, crash.txt, and steps.csv / me_steps.csv when it reached vb 31,800): how far it got, exactness against the indoor
# references, the resting ME (rests, wakes, kicks, Sony's request words), the "mix: stopped" / end lines, and the battery.
D=$1; P=/mnt/project-files/mgs2/prof; T=/mnt/project-files/mgs2/tools/dev; R=${2:-$P/hw_v30/log_run5.txt}
echo "== how far"
grep -a -E '^(start|end|home|exit|stop)' $D/battery.txt | cut -c1-160; tail -1 $D/battery.txt | cut -c1-120
grep -a -E '^(mix [a-z ]+: ME|mix: |me: stage|me rest:)' $D/log.txt | grep -av 'mix vb' | cut -c1-400
echo "== exact"
python3 $T/hashcmp.py $D/log.txt $P/hashes_x86_in_v30.txt; python3 $T/hashcmp.py $D/log.txt $P/hashref_x86_in_v30.txt
n=$(grep -ac '^vuh' $D/log.txt); m=$(diff <(grep -a '^vuh' $D/log.txt) <(grep -a '^vuh' $R | head -$n) | grep -c '^<')
echo "vuh: $n lines, $m differ from the first $n of $R"
sh $T/mixcmp.sh $D/log.txt $P/mix_ck_x86_in_v30.txt
grep -a -E 'split|stopped|me_dead|stall|exception' $D/log.txt | grep -av -E '^me: stage|exception EPC 00000000' | head -5
echo "== rests (the 100-vblank lines)"
grep -a '^mix vb [0-9]*: ME rests' $D/log.txt | awk '{
  for (i = 1; i <= NF; i++) { w = $i; gsub(/[(),;:]/, "", w); f[i] = w }
  r += f[6]; h += f[11]; b += f[14]; ni += f[18]; g += f[21]; k += f[29]; ku += f[29] * f[30]; wt += f[34];
  if (f[29] && f[30] + 0 > kmax) kmax = f[30] + 0; c = f[NF - 1]; n++; last = f[3]
  if ($0 !~ /flags 80000000 \(all bits seen 80000000\)/) odd++
  if ($0 !~ /request words 00000187 000001ff 00000000 0000014d 00000002/) rq++ }
  END { printf "%d lines to vb %s: rests %d (in the halt %d, before it %d, no interrupt %d, given up %d); kicks %d (%.1f us mean, max %.1f) and %d while waiting; lines with other flags %d, with other request words %d, request words changed at %s wakes (last line)\n", n, last, r, h, b, ni, g, k, k ? ku / k : 0, kmax, wt, odd + 0, rq + 0, c }'
echo "== battery"
awk '{ if ($0 ~ /external power 1/) ep++; if ($0 ~ /charging 1/) ch++; if ($0 ~ /low 1/) lo++; if ($0 ~ /suspend required 1/) su++; n++ }
  END { printf "%d lines: external power 1 on %d, charging 1 on %d, low 1 on %d, suspend required 1 on %d\n", n, ep + 0, ch + 0, lo + 0, su + 0 }' $D/battery.txt
grep -a -o -E '^[a-z0-9 ]+ t [0-9]+ s, vb -?[0-9]+: [0-9]+%, [0-9]+ mV|capacity [0-9]+ of [0-9]+ mAh' $D/battery.txt | paste - - | awk 'NR == 1 || NR % 60 == 0 { print } { l = $0 } END { print l }'
echo "== crash.txt"; cat $D/crash.txt
if [ -f $D/steps.csv ]; then echo "== steps (against run 5's polling ME)"; python3 $T/steps_in.py $P/hw_v30/steps_run5.csv $D/steps.csv; fi
