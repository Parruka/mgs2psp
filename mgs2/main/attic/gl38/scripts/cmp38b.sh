#!/bin/bash
# cmp38b.sh (gl38, after runs38b.sh): frames (build_gl35/g1cmp35.py: list and frame digests, counts, dz text, content, natb / natz / natq / nats lines) and the
# other lines (filtered as build_gl37/cmp37a.sh) between gl38's twin arms and gl37's twin log of the same keys (rrC).  The schedule's arm against the
# plain arm by the kind of each frame's turn (a cycle of 138 turns: the schedule's digits, then 0).
cd /home/claude/main/build_gl38/tw/x86 || exit 1
lg() { case $1 in /*) if [ -f $1.log ]; then cat $1.log; else zcat $1.log.gz; fi ;; *) if [ -f out_$1.log ]; then cat out_$1.log; else zcat out_$1.log.gz; fi ;; esac; }
al() { lg $1 | grep -av "^natst:\|^natkeep\|^natreuse\|^natcull\|^natf1\|^bound:\|^nat: cull study\|^nat: gl3[5-8]\|^nrdbg\|time\|ms\b\|^twin.sh\|split_"; }
cmp1() { echo "=== $1 against $2 ${3:+(cycle)}"; python3 -I /home/claude/main/build_gl35/g1cmp35.py <(lg $1) <(lg $2) $3
  n=$(diff <(al $1) <(al $2) | grep -c '^[<>]'); echo "alllines filtered: lines differing $n"
  [ "$n" = 0 ] || diff <(al $1) <(al $2) | head -6 | cut -c1-200; }
G=/home/claude/main/build_gl37/tw/x86
C=$(python3 -I -c "
import sys; sys.path.insert(0, '/home/claude/main/build_gl38/tools'); import sched38
print(','.join(list(sched38.bound) + ['0'] * (138 - len(sched38.bound))))")
for a in "$@"; do case $a in
  g37*) cmp1 $G/out_rrC $a ;;
  sch*) cmp1 ${BASE:-g37d} $a $C ;;
  *) cmp1 ${BASE:-g37d} $a ;; esac; done
