#!/bin/sh
# xrun27.sh <build dir> [env...]: the three x86 routes (runh.sh d / w / f) in parallel from <build dir>/boot, then xcmp27.sh
B=$1; shift; [ -x $B/boot ] || { echo "no $B/boot"; exit 1; }
for r in d w f; do mkdir -p $B/R$r/memcard0; cp $B/boot $B/R$r/boot; ( sh /home/claude/lean/runh.sh $B/R$r H$r $r "$@" ) & done
wait
sh /home/claude/lean/xcmp27.sh $B
