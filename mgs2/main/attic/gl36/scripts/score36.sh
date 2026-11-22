#!/bin/sh
# score36.sh (gl36's scoring [C model]; after model36.sh and the g36n4 run): the windows' totals a picture (mtot35.py = gl33's mtot33.py),
# the split of the native path by part (split36.py = split35.py + the reuse functions: drawn minus not drawn, and reuse minus plain) and the top functions (ftop.py).
# gl35's model files are links in build_gl36/model to gl35's own runs (g35p gl35 keep 0 = gl35's plain turns; g35k gl35 keeps 15).
# The lever = g36r - g36p (one ELF); the base change = g36p (keep 1 + the reset + the retrain) - g35p, and g36r - g35k (the arms that
# fall on the same vblanks in the delivered turns: reuse 0 on gl35's keep 0 turns, reuse 1 on gl35's keep 15 turns).
R=/home/claude/main/build_gl36; cd $R || exit 1; E=$R/psp/MGS2T.elf
W4="waiting deck, ME turn 29,810-29,890
waiting deck, main turn 29,910-29,990
rain walk, ME turn 31,010-31,090 (after two windows)
rain walk, main turn 31,110-31,190 (after three windows)"
{ echo "== the four rain windows, against gl36 no reuse (g36p): gl36 reuse (g36r: the lever)"
  echo "$W4" | python3 -I mtot35.py model g36p g36r
  echo "== the base change: gl35 keep 0 (g35p) against gl36 no reuse (g36p) and gl36 reuse (g36r); gl35 keeps 15 (g35k)"
  echo "$W4" | python3 -I mtot35.py model g35p g36p g36r g35k
  echo "== gl35 keeps 15 (g35k) against gl36 reuse (g36r)"
  echo "$W4" | python3 -I mtot35.py model g35k g36r; } > model_tot4.out 2>&1
{ echo "== the steady deck in d01t (no native groups there): gl36 no reuse (g36pc2) | gl36 reuse (g36rc2) | gl35 keep 0 (g35pc2) | gl35 keeps (g35kc2)"
  printf 'steady deck (d01t), ME turn 27,410-27,490\nsteady deck (d01t), main turn 27,510-27,590\n' | python3 -I mtot35.py model g36pc2 g36rc2 g35pc2 g35kc2
  echo "== the opening (before the native turns): gl36 no reuse (g36pc1) | gl36 reuse (g36rc1) | gl35 keep 0 (g35pc1) | gl35 keeps (g35kc1)"
  printf 'opening, ME turn 16,010-16,090\nopening, main turn 16,110-16,190\n' | python3 -I mtot35.py model g36pc1 g36rc1 g35pc1 g35kc1; } > model_totc.out 2>&1
[ -f model/g36n4 ] && { for a in "p no reuse" "r reuse"; do set -- $a; t=$1; shift
  python3 -I split36.py $E 40 609 11545 "rain wait, ME turn 29,810-29,890, gl36 $*: drawn (g36$t) - not drawn (g36n4)" model/g36n4 model/g36$t
  echo; python3 -I split36.py $E 40 609 11545 "rain wait, main turn 29,910-29,990, gl36 $*: drawn (g36$t.1) - not drawn (g36n4.1)" model/g36n4.1 model/g36$t.1; echo; done > model_split.out 2>&1
  { python3 -I split36.py $E 40 609 11545 "rain wait, ME turn 29,810-29,890: gl36 reuse (g36r) - gl36 no reuse (g36p) by part (the lever)" model/g36p model/g36r; echo
    python3 -I split36.py $E 40 609 11545 "rain wait, main turn 29,910-29,990: gl36 reuse (g36r.1) - gl36 no reuse (g36p.1) by part (the lever)" model/g36p.1 model/g36r.1; echo
    python3 -I split36.py $E 40 609 11545 "rain walk, ME turn 31,010-31,090: gl36 reuse (g36r.2) - gl36 no reuse (g36p.2) by part (the lever)" model/g36p.2 model/g36r.2; } > model_lever.out 2>&1; }
for t in g36p g36r; do python3 -I ftop.py $E model/$t 40 50 > model/ftop_$t.txt; done
cat model_tot4.out model_totc.out
