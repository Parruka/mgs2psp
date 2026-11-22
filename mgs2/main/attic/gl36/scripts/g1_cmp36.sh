#!/bin/bash
# g1_cmp36.sh (gl36 G1, after g1_runs36.sh): frame by frame (gl35's g1cmp35.py: list and frame digests, counts, dz text, content, natb /
# natz / natq, the translator's state 'nats') and every other line of the logs but the reuse's and the keeps' own lines, the static-stream
# check's lines (only in runs with MGS2_NAT_STCHK) and the times ('alllines'), pairs: p against r, rc, rk15, rnf, needr; needp against
# needr; lw2p against lw2r; px against rx and rx2 (with the cycle); gl35's dx (its emulator reference) against gl36's rx.
cd /home/claude/main/build_gl36/tw/x86 || exit 1
lg() { case $1 in /*) if [ -f $1.log ]; then cat $1.log; else zcat $1.log.gz; fi ;; *) if [ -f out_$1.log ]; then cat out_$1.log; else zcat out_$1.log.gz; fi ;; esac; }
al() { lg $1 | grep -av "^natst:\|^natkeep\|^natreuse\|^nat: gl35\|^nat: gl36\|^nrdbg\|time\|ms\b\|^twin.sh\|split_"; }
for p in "p r" "p rc" "p rk15" "p rnf" "p needr" "needp needr" "lw2p lw2r" "px rx 1,1,0,0" "px rx2 0,0,1,1" "/home/claude/main/build_gl35/tw/x86/out_dx rx 1,1,0,0"; do
  set -- $p; echo "=== $1 against $2"
  python3 -I /home/claude/main/build_gl35/g1cmp35.py <(lg $1) <(lg $2) $3
  n=$(diff <(al $1) <(al $2) | grep -c '^[<>]'); echo "alllines: lines differing (but the reuse's and keeps' own lines, natst and times) $n"
  [ "$n" = 0 ] || diff <(al $1) <(al $2) | head -6 | cut -c1-200
done
echo "=== the reuse's totals and the fast and keep totals by run"
for t in p r rc rk15 rnf rx rx2 px lw2p lw2r needp needr; do echo "$t: $(lg $t | grep -a '^natreuse: totals' | cut -c18-)"; echo "$t: $(lg $t | grep -a '^natkeep: totals' | cut -c17-)"; echo "$t: $(lg $t | grep -a '^natfast: totals: groups' | cut -c17-)"; done
