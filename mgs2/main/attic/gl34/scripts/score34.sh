#!/bin/sh
# score34.sh (gl34's scoring [C model]; after model34.sh and the g34n4 run): the windows' totals a picture (mtot34.py = gl33's
# mtot33.py), the split of the native path by part (split34.py: drawn minus not drawn, plain and fast) and the top functions (ftop.py).
R=/home/claude/main/build_gl34; cd $R || exit 1; E=$R/psp/MGS2T.elf
W4="waiting deck, ME turn 29,810-29,890
waiting deck, main turn 29,910-29,990
rain walk, ME turn 31,010-31,090 (after two windows)
rain walk, main turn 31,110-31,190 (after three windows)"
{ echo "== the four rain windows: gl33 in place (g33i, gl33's ELF) | gl34 plain, timers off (g34p0) | gl34 plain (g34p) | gl34 fast, timers off (g34f0) | gl34 fast (g34f); against gl33 in place"
  echo "$W4" | python3 -I mtot34.py model g33i g34p0 g34p g34f0 g34f
  echo "== the same, against gl34 plain (the lever: g34f - g34p; the timers: g34p - g34p0, g34f - g34f0)"
  echo "$W4" | python3 -I mtot34.py model g34p g34p0 g34f g34f0; } > model_tot4.out 2>&1
{ echo "== the steady deck in d01t (no native groups there): gl33 in place (g33ic2) | gl34 plain (g34pc2) | gl34 fast (g34fc2)"
  printf 'steady deck (d01t), ME turn 27,410-27,490\nsteady deck (d01t), main turn 27,510-27,590\n' | python3 -I mtot34.py model g33ic2 g34pc2 g34fc2
  echo "== the opening (before the native turns): gl33 (g33pc1) | gl34 plain (g34pc1) | gl34 fast (g34fc1)"
  printf 'opening, ME turn 16,010-16,090\nopening, main turn 16,110-16,190\n' | python3 -I mtot34.py model g33pc1 g34pc1 g34fc1; } > model_totc.out 2>&1
[ -f model/g34n4 ] && { for a in "p plain" "f fast"; do set -- $a
  python3 -I split34.py $E 40 609 11545 "rain wait, ME turn 29,810-29,890, gl34 $2: drawn (g34$1) - not drawn (g34n4)" model/g34n4 model/g34$1
  echo; python3 -I split34.py $E 40 609 11545 "rain wait, main turn 29,910-29,990, gl34 $2: drawn (g34$1.1) - not drawn (g34n4.1)" model/g34n4.1 model/g34$1.1; echo; done > model_split.out 2>&1
  { python3 -I split34.py $E 40 609 11545 "rain wait, ME turn 29,810-29,890: gl34 fast (g34f) - gl34 plain (g34p) by part (the lever)" model/g34p model/g34f; echo
    python3 -I split34.py $E 40 609 11545 "rain wait, main turn 29,910-29,990: gl34 fast (g34f.1) - gl34 plain (g34p.1) by part (the lever)" model/g34p.1 model/g34f.1; } > model_lever.out 2>&1; }
for t in g34p g34f; do python3 -I ftop.py $E model/$t 40 50 > model/ftop_$t.txt; done
cat model_tot4.out model_totc.out
