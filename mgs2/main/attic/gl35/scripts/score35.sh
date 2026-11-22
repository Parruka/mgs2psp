#!/bin/sh
# score35.sh (gl35's scoring [C model]; after model35.sh and the g35n4 run): the windows' totals a picture (mtot35.py = gl33's mtot33.py),
# the split of the native path by part (split35.py: drawn minus not drawn, no keeps and keeps) and the top functions (ftop.py).
# Model files of gl33 and gl34 are links in build_gl35/model to their own runs (g33i gl33 in place; g34f gl34 fast in every turn = gl34's
# delivered fast turns' code path; g34p gl34 plain).
R=/home/claude/main/build_gl35; cd $R || exit 1; E=$R/psp/MGS2T.elf
W4="waiting deck, ME turn 29,810-29,890
waiting deck, main turn 29,910-29,990
rain walk, ME turn 31,010-31,090 (after two windows)
rain walk, main turn 31,110-31,190 (after three windows)"
{ echo "== the four rain windows, against gl35 no keeps (g35p): gl35 keeps 15 (g35k: the lever), keeps 7 (g35k7: the native groups' own look-ups only)"
  echo "$W4" | python3 -I mtot35.py model g35p g35k g35k7
  echo "== the retrain: gl35 no keeps (g35p) against gl34 fast (g34f, the same path on gl34's ELF) and gl34 plain (g34p), gl33 in place (g33i)"
  echo "$W4" | python3 -I mtot35.py model g34f g35p g34p g33i
  echo "== all against gl33 in place (g33i): gl34 fast, gl34 plain, gl35 no keeps, gl35 keeps"
  echo "$W4" | python3 -I mtot35.py model g33i g34f g34p g35p g35k; } > model_tot4.out 2>&1
{ echo "== the steady deck in d01t (no native groups there): gl34 fast (g34fc2) | gl35 no keeps (g35pc2) | gl35 keeps (g35kc2) | gl34 plain (g34pc2) | gl33 in place (g33ic2)"
  printf 'steady deck (d01t), ME turn 27,410-27,490\nsteady deck (d01t), main turn 27,510-27,590\n' | python3 -I mtot35.py model g34fc2 g35pc2 g35kc2 g34pc2 g33ic2
  echo "== the opening (before the native turns): gl34 fast (g34fc1) | gl35 no keeps (g35pc1) | gl35 keeps (g35kc1) | gl34 plain (g34pc1) | gl33 (g33pc1)"
  printf 'opening, ME turn 16,010-16,090\nopening, main turn 16,110-16,190\n' | python3 -I mtot35.py model g34fc1 g35pc1 g35kc1 g34pc1 g33pc1; } > model_totc.out 2>&1
[ -f model/g35n4 ] && { for a in "p no keeps" "k keeps 15"; do set -- $a; t=$1; shift
  python3 -I split35.py $E 40 609 11545 "rain wait, ME turn 29,810-29,890, gl35 $*: drawn (g35$t) - not drawn (g35n4)" model/g35n4 model/g35$t
  echo; python3 -I split35.py $E 40 609 11545 "rain wait, main turn 29,910-29,990, gl35 $*: drawn (g35$t.1) - not drawn (g35n4.1)" model/g35n4.1 model/g35$t.1; echo; done > model_split.out 2>&1
  { python3 -I split35.py $E 40 609 11545 "rain wait, ME turn 29,810-29,890: gl35 keeps (g35k) - gl35 no keeps (g35p) by part (the lever)" model/g35p model/g35k; echo
    python3 -I split35.py $E 40 609 11545 "rain wait, main turn 29,910-29,990: gl35 keeps (g35k.1) - gl35 no keeps (g35p.1) by part (the lever)" model/g35p.1 model/g35k.1; echo
    python3 -I split35.py $E 40 609 11545 "rain walk, ME turn 31,010-31,090: gl35 keeps (g35k.2) - gl35 no keeps (g35p.2) by part (the lever)" model/g35p.2 model/g35k.2; } > model_lever.out 2>&1; }
for t in g35p g35k; do python3 -I ftop.py $E model/$t 40 50 > model/ftop_$t.txt; done
cat model_tot4.out model_totc.out
