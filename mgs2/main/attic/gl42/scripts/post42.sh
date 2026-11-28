#!/bin/sh
# post42.sh [arms] (gl42): the refcmp that runs42a.sh's arms nchk, lw7 and pgo could not run (their REF was a relative path, and
# tw_run42.sh runs twin.sh in the twin's folder): each log against cfg/hashref_gl42.txt by its absolute path, writing tw/x86/hashref_<arm>.txt.
R=/home/claude/main/build_gl42; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g42; X=$R/tw/x86
for t in ${*:-nchk lw7 pgo}; do zcat $X/out_$t.log.gz > $S/out_$t.log || exit 1
  echo "== $t"; nice -n 19 python3 -I /home/claude/main/tools/refcmp.py $R/cfg/hashref_gl42.txt $S/out_$t.log $X/hashref_$t.txt; echo "refcmp rc $?"
  cmp -s $X/hashref_$t.txt $R/cfg/hashref_gl42.txt && echo "hashref_$t.txt equals hashref_gl42.txt byte for byte"
  rm -f "$S/out_$t.log"; done
