#!/bin/sh
# xcmp.sh <out dir>: the three routes' hash lines (vb ee iop vfb, every 100) against the references; d / w = v21's, f = v25f's
P=/mnt/project-files/mgs2/prof
for r in d w f; do
  case $r in d) R=$P/hashes_x86_deck_v21.txt;; w) R=$P/hashes_x86_w01a_v21.txt;; f) R=$P/hashes_x86_free_v25f.txt;; esac
  F=$1/R$r/H$r.ref; [ -f $1/R$r/H$r.done ] || { echo "$r: not done"; continue; }
  n=$(wc -l < $F); fd=$(diff $F $R | grep '^<' | head -1)
  bad=$(grep -a 'missing\|error\|unhandled\|not found' $1/R$r/run_H$r.log | grep -vc 'ROMVER not found')
  echo "$r: $n lines, $(( n - $(diff $F $R | grep -c '^<') )) / $(wc -l < $R) equal to $(basename $R); first difference: ${fd:-none}; $(cat $1/R$r/rc_H$r.txt); other missing/error lines: $bad"
done
