#!/bin/sh
# score36c.sh (gl36 retrain [C model], after model36c.sh): the totals a picture (mtot35.py) and the pair means (pairmean36.py):
#  the reuse lever from runs with the cull equal (r - p with cull 0, rc - c with cull 1), the cull lever from runs with the reuse equal
#  (c - p with reuse 0, rc - r with reuse 1); the retrain against gl35 (g36p, the levers off, against g35p = gl35 keep 0; the deck and
#  the opening against g35pc2 / g35pc1), each scene by its pair's mean (ME turn + main turn); the noise from the deck and opening pairs
#  with the levers off and on.
R=/home/claude/main/build_gl36; cd $R || exit 1
W4="waiting deck, ME turn 29,810-29,890
waiting deck, main turn 29,910-29,990
rain walk, ME turn 31,010-31,090
rain walk, main turn 31,110-31,190"
{ echo "== the rain windows: g36p (levers off) | g36r (reuse) | g36c (cull) | g36rc (both) | g35p (gl35 keep 0) | g35k (gl35 keeps 15)"
  echo "$W4" | python3 -I mtot35.py model g36p g36r g36c g36rc g35p g35k
  echo "== the deck and the opening: g36 levers off | levers on | gl35 keep 0 | gl35 keeps 15"
  printf 'steady deck (d01t), ME turn 27,410-27,490\nsteady deck (d01t), main turn 27,510-27,590\n' | python3 -I mtot35.py model g36pc2 g36rcc2 g35pc2 g35kc2
  printf 'opening, ME turn 16,010-16,090\nopening, main turn 16,110-16,190\n' | python3 -I mtot35.py model g36pc1 g36rcc1 g35pc1 g35kc1
  echo "== pair means [C model], ms a step (ME turn and the main turn after it, which alternate)"
  echo "-- the reuse lever, cull equal:"; python3 -I pairmean36.py model g36p:,.1 g36r:,.1 g36p:.2,.3 g36r:.2,.3 g36c:,.1 g36rc:,.1 g36c:.2,.3 g36rc:.2,.3
  echo "-- the cull lever, reuse equal:"; python3 -I pairmean36.py model g36p:,.1 g36c:,.1 g36p:.2,.3 g36c:.2,.3 g36r:,.1 g36rc:,.1 g36r:.2,.3 g36rc:.2,.3
  echo "-- the retrain against gl35 (levers off against gl35 keep 0): rain wait, rain walk, steady deck, opening:"
  python3 -I pairmean36.py model g35p:,.1 g36p:,.1 g35p:.2,.3 g36p:.2,.3 g35pc2:,.1 g36pc2:,.1 g35pc1:,.1 g36pc1:,.1
  echo "-- the delivered arms against gl35's (both levers on against gl35 keeps 15; rain):"; python3 -I pairmean36.py model g35k:,.1 g36rc:,.1 g35k:.2,.3 g36rc:.2,.3
  echo "-- noise reference: the deck and the opening, levers on against off (same ELF):"; python3 -I pairmean36.py model g36pc2:,.1 g36rcc2:,.1 g36pc1:,.1 g36rcc1:,.1
  echo "-- gl36 before the retrain (pre_cull/model, the first trained build) against the retrain, levers off:"
  for t in g36p g36pc2 g36pc1; do for s in "" .1 .2 .3; do [ -f pre_cull/model/$t$s ] && ln -sf $R/pre_cull/model/$t$s model/old_$t$s; done; done
  python3 -I pairmean36.py model old_g36p:,.1 g36p:,.1 old_g36p:.2,.3 g36p:.2,.3 old_g36pc2:,.1 g36pc2:,.1 old_g36pc1:,.1 g36pc1:,.1
} > score36c.out 2>&1
for t in g36p g36rc; do python3 -I ftop.py psp/MGS2T.elf model/$t 40 50 > model/ftop_$t.txt; done
cat score36c.out
