#!/bin/sh
# cmpb.sh <build a> <build b>: the two builds' route outputs (R<r>/H<r>.ref every 100 vblanks, H<r>.dense the per-frame window) side by side
for r in d w f; do
  a=$1/R$r/H$r; b=$2/R$r/H$r
  [ -f $a.done ] && [ -f $b.done ] || { echo "$r: not run in both"; continue; }
  awk 'NR == FNR {x[$1] = $0; next} ($1 in x) {n++; if (x[$1] == $0) e++; else if (!f) f = x[$1] " / " $0} END {printf "'$r': %d / %d checkpoints equal; first difference: %s\n", e, n, f ? f : "none"}' $a.ref $b.ref
  awk 'NR == FNR {x[$1] = $0; next} ($1 in x) {n++; if (x[$1] == $0) e++; else if (!f) f = $1} END {printf "   per-frame window: %d / %d equal; first difference: vb %s\n", e, n, f ? f : "none"}' $a.dense $b.dense
done
