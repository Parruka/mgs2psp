#!/bin/sh
# ecmp.sh <build dir> [ref dir]: per-vblank hashes (rune.sh) of each route against the reference build's (default /home/claude/lean/xP)
B=$1; P=${2:-/home/claude/lean/xP}
for r in d w f; do
  [ -f $B/E$r/E$r.done ] && [ -f $P/E$r/E$r.done ] || { echo "$r: not done"; continue; }
  n=$(wc -l < $P/E$r/E$r.all); m=$(wc -l < $B/E$r/E$r.all)
  d=$(diff $B/E$r/E$r.all $P/E$r/E$r.all | grep -c '^<'); f=$(diff $B/E$r/E$r.all $P/E$r/E$r.all | grep '^<' | head -1)
  echo "$r: $m / $n vblanks, $d differ; first: ${f:-none}"
done
