#!/bin/bash
# gB_cmp36.sh (fix B, after gB_runs36.sh): each fix-B twin run against the gated run of the same arm (pB/p, rxB/rx, rB/r, rcB/rc), frame by
# frame (gl35's g1cmp35.py) and every other line but the times ('alllines', as g1_cmp36.sh; here the reuse's own lines are compared too)
cd /home/claude/main/build_gl36/tw/x86 || exit 1
lg() { zcat out_$1.log.gz; }
al() { lg $1 | grep -av "time\|ms\b\|^twin.sh\|split_"; }
for p in "p pB" "rx rxB" "r rB" "rc rcB"; do set -- $p; [ -f out_$2.log.gz ] || continue; echo "=== $1 against $2"
  python3 -I /home/claude/main/build_gl35/g1cmp35.py <(lg $1) <(lg $2)
  n=$(diff <(al $1) <(al $2) | grep -c '^[<>]'); echo "alllines (the reuse's and keeps' lines included; times out): lines differing $n"
  [ "$n" = 0 ] || diff <(al $1) <(al $2) | head -6 | cut -c1-200; done
