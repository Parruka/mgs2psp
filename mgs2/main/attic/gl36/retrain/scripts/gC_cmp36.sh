#!/bin/bash
# gC_cmp36.sh (gl36 retrain, after gC_runs36.sh; the tree with the cull, t36_files_C.md5): hashes (each arm's hashref against c9ddb593),
# frames (gl35's g1cmp35.py) and every other line ('alllines', filtered as g1_cmp36.sh plus the cull's own lines; and unfiltered but the
# times), pairs: gl36's G1 pairs with C tags; the cull's pairs rxC-dxC (cy8 by the cull cycle and by the reuse cycle), rxC-onC, rxC-chkC,
# l3offC-l3chkC, cf0offC-cf0chkC, rxC-cf0offC, rxC-bothC (reuse and cull every turn, both checks); the pre-cull t36 (out_rxB, fix B) against
# rxC; gl35's dx against rxC.  Then the natcull totals by arm (the check arms' DIFFERING) and the reuse totals.
cd /home/claude/main/build_gl36/tw/x86 || exit 1
lg() { case $1 in /*) if [ -f $1.log ]; then cat $1.log; else zcat $1.log.gz; fi ;; *) if [ -f out_$1.log ]; then cat out_$1.log; else zcat out_$1.log.gz; fi ;; esac; }
al() { lg $1 | grep -av "^natst:\|^natkeep\|^natreuse\|^natcull\|^nat: cull study\|^nat: gl35\|^nat: gl36\|^nrdbg\|time\|ms\b\|^twin.sh\|split_"; }
au() { lg $1 | grep -av "time\|ms\b"; }
echo "=== hashes (g1r_*.out)"; for f in ../../g1r_*C.out; do echo "$(basename $f): $(grep -a 'reference lines\|equals\|differ' $f | tr '\n' ' ' | cut -c1-230)"; done
cmp1() { echo "=== $1 against $2 ${3:+(cycle $3)}"; python3 -I /home/claude/main/build_gl35/g1cmp35.py <(lg $1) <(lg $2) $3
  [ -n "$4" ] && return
  n=$(diff <(al $1) <(al $2) | grep -c '^[<>]'); echo "alllines filtered (reuse/keep/cull own lines, natst, times out): lines differing $n"
  [ "$n" = 0 ] || diff <(al $1) <(al $2) | head -8 | cut -c1-200
  n=$(diff <(au $1) <(au $2) | grep -c '^[<>]'); echo "alllines unfiltered (times out): lines differing $n"
  [ "$n" = 0 ] || diff <(au $1) <(au $2) | grep '^[<>]' | head -12 | cut -c1-200; }
cmp1 pC rC; cmp1 pC rcC; cmp1 pC rk15C; cmp1 pC rnfC; cmp1 pC needrC; cmp1 needpC needrC; cmp1 lw2pC lw2rC
cmp1 pxC rxC 1,1,0,0; cmp1 pxC rx2C 0,0,1,1; cmp1 /home/claude/main/build_gl35/tw/x86/out_dx rxC 1,1,0,0
cmp1 rxC dxC 1,1,1,1,0,0,0,0; cmp1 rxC dxC 1,1,0,0 x; cmp1 rxC onC; cmp1 rxC chkC; cmp1 l3offC l3chkC; cmp1 cf0offC cf0chkC; cmp1 rxC cf0offC; cmp1 rxC bothC
cmp1 rxB rxC
echo "=== natcull totals by arm"; for t in rxC dxC onC chkC bothC l3offC l3chkC cf0offC cf0chkC; do echo "$t: $(lg $t | grep -a '^natcull: totals' | cut -c1-600)"; done
echo "=== reuse totals by arm"; for t in pC rC rcC rk15C rnfC rxC rx2C pxC dxC bothC lw2pC lw2rC needpC needrC; do echo "$t: $(lg $t | grep -a '^natreuse: totals' | cut -c18-300)"; done
