#!/bin/sh
# model36.sh [psp dir] (gl36's scoring, [C model]; gl35's model35.sh with the reuse in place of the keeps): PspProf runs of the final build
# (default build_gl36/psp), native, in place, fast and the drawn-areas keep in every turn (as the delivered cfg), with the reuse off in the
# whole run (p: cfg_emu/mgs2_m36_p.cfg) and on in the whole run (r: mgs2_m36_r.cfg, MGS2_NAT_REUSE=1), timers on (as delivered):
#   g36p, g36r       the four rain windows in one run (waiting ME 29,810-29,890, waiting main 29,910-29,990, rain walk ME
#                    31,010-31,090, rain walk main 31,110-31,190), as gl35's g35p / g35k
#   g36pc2, g36rc2   the steady deck in d01t (27,410-27,490 an ME turn, 27,510-27,590 a main turn)
#   g36pc1, g36rc1   the opening (16,010-16,090 ME, 16,110-16,190 main; before the native turns)
# three runs at a time.
R=/home/claude/main/build_gl36; P=${1:-$R/psp}; cd $R/m || exit 1; H=$R/cfg/hashref_gl36.txt; C=$R/cfg_emu/mgs2_m36
W4="29810:29890 29910:29990 31010:31090 31110:31190"
sh scripts/pgo.sh model $P g36p ${C}_p.cfg $H $W4 > $R/pgo_g36p.out 2>&1 &
sh scripts/pgo.sh model $P g36r ${C}_r.cfg $H $W4 > $R/pgo_g36r.out 2>&1 &
sh scripts/pgo.sh model $P g36pc2 ${C}_p.cfg $H 27410:27490 27510:27590 > $R/pgo_g36pc2.out 2>&1 &
wait
sh scripts/pgo.sh model $P g36rc2 ${C}_r.cfg $H 27410:27490 27510:27590 > $R/pgo_g36rc2.out 2>&1 &
sh scripts/pgo.sh model $P g36pc1 ${C}_p.cfg $H 16010:16090 16110:16190 > $R/pgo_g36pc1.out 2>&1 &
sh scripts/pgo.sh model $P g36rc1 ${C}_r.cfg $H 16010:16090 16110:16190 > $R/pgo_g36rc1.out 2>&1 &
wait
echo done
