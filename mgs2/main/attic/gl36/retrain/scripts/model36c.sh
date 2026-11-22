#!/bin/sh
# model36c.sh [psp dir] (gl36 retrain's scoring [C model]; model36.sh with the cull): PspProf runs of the final build (default
# build_gl36/psp), native, in place, fast and the drawn-areas keep in every turn, timers on (as delivered), each lever whole-run:
#   g36p  reuse 0 cull 0 (cfg_emu/mgs2_m36_p.cfg)    g36r  reuse 1 cull 0 (m36_r)    g36c  reuse 0 cull 1 (m36_c)    g36rc  both (m36_rc)
#   the four rain windows in one run each (waiting ME 29,810-29,890, waiting main 29,910-29,990, walk ME 31,010-31,090, walk main 31,110-31,190);
#   g36pc2 / g36rcc2  the steady deck in d01t (27,410-27,490 ME, 27,510-27,590 main), levers off / both on
#   g36pc1 / g36rcc1  the opening (16,010-16,090 ME, 16,110-16,190 main), levers off / both on
# three runs at a time; each game folder deleted after its run (literal path).
R=/home/claude/main/build_gl36; P=${1:-$R/psp}; cd $R/m || exit 1; H=$R/cfg/hashref_gl36.txt; C=$R/cfg_emu/mgs2_m36
W4="29810:29890 29910:29990 31010:31090 31110:31190"
mr() { t=$1; c=$2; shift 2; nice -n 10 sh scripts/pgo.sh model $P $t ${C}_$c.cfg $H "$@" > $R/pgo_$t.out 2>&1; rm -rf "$R/ms/$t"; }
mr g36p p $W4 & mr g36r r $W4 & mr g36c c $W4 & wait
mr g36rc rc $W4 & mr g36pc2 p 27410:27490 27510:27590 & mr g36pc1 p 16010:16090 16110:16190 & wait
mr g36rcc2 rc 27410:27490 27510:27590 & mr g36rcc1 rc 16010:16090 16110:16190 & wait
echo done
