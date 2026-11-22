#!/bin/sh
# g1_cmp35.sh (gl35 G1, after g1_runs35.sh; bash): frame by frame (g1cmp35.py: list and frame digests, counts, dz text, content, natb / natz /
# natq, the translator's state 'nats') and every other line of the logs but the keeps' own lines, the static-stream check's lines (only in
# runs with MGS2_NAT_STCHK) and the times ('alllines'), pairs: f against k, kc, k1, k2, k4, k8, fk, needk; needp against needk; lw2p
# against lw2k; dxp against dx and dx2 (with the cycle); gl34's dx2 (its emulator reference) against gl35's dx.  Logs may be gzipped
# (out_<tag>.log.gz); g1cmp35.py then names them /dev/fd/N.
cd /home/claude/main/build_gl35/tw/x86 || exit 1
lg() { case $1 in /*) cat $1.log ;; *) if [ -f out_$1.log ]; then cat out_$1.log; else zcat out_$1.log.gz; fi ;; esac; }
al() { lg $1 | grep -av "^natst:\|^natkeep\|^nat: gl35\|time\|ms\b\|^twin.sh\|split_"; }
for p in "f k" "f kc" "f k1" "f k2" "f k4" "f k8" "f fk" "f needk" "needp needk" "lw2p lw2k" "dxp dx 15,15,0,0" "dxp dx2 0,0,15,15" "/home/claude/main/build_gl34/tw/x86/out_dx2 dx 15,15,0,0"; do
  set -- $p; echo "=== $1 against $2"
  python3 -I /home/claude/main/build_gl35/g1cmp35.py <(lg $1) <(lg $2) $3
  n=$(diff <(al $1) <(al $2) | grep -c '^[<>]'); echo "alllines: lines differing (but the keeps' own lines, natst and times) $n"
  [ "$n" = 0 ] || diff <(al $1) <(al $2) | head -6 | cut -c1-200
done
echo "=== the keeps' totals and the fast totals by run"
for t in f k kc k1 k2 k4 k8 fk dx dx2 dxp lw2p lw2k needp needk g31k; do echo "$t: $(lg $t | grep -a '^natkeep: totals' | cut -c17-)"; echo "$t: $(lg $t | grep -a '^natfast: totals: groups' | cut -c17-)"; done
