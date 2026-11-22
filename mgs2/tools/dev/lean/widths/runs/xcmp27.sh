#!/bin/sh
# xcmp27.sh <out dir>: the three x86 routes' hash lines against the v27 references: <tag>.ref (vb ee iop vfb, every 100) against
# prof/hashes_x86_<route>_v27.txt and <tag>.dense (vb ee vfb [vuh]) against prof/hashref_x86_<route>_v27.txt, all fields
P=/mnt/project-files/mgs2/prof
for r in d w f; do
  case $r in d) n_=deck;; w) n_=w01a;; f) n_=free;; esac
  R=$P/hashes_x86_${n_}_v27.txt; H=$P/hashref_x86_${n_}_v27.txt
  F=$1/R$r/H$r.ref; [ -f $1/R$r/H$r.done ] || { echo "$r: not done"; continue; }
  n=$(wc -l < $F); fd=$(diff $F $R | grep '^<' | head -1)
  dd=$(diff $1/R$r/H$r.dense $H | grep -c '^<')
  bad=$(grep -a 'missing\|error\|unhandled\|not found' $1/R$r/run_H$r.log | grep -vc 'ROMVER not found')
  echo "$r: $n lines, $(( n - $(diff $F $R | grep -c '^<') )) / $(wc -l < $R) equal to $(basename $R); first difference: ${fd:-none}; dense lines differing from $(basename $H): $dd of $(wc -l < $H); $(cat $1/R$r/rc_H$r.txt); other missing/error lines: $bad"
done
