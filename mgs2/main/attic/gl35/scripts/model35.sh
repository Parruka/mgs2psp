#!/bin/sh
# model35.sh [psp dir] (gl35's scoring, [C model]): PspProf runs of the final build (default build_gl35/psp), native, in place and the
# fast front end in every turn (as the delivered cfg), with the keeps off in the whole run (p: cfg_emu/mgs2_m35_p.cfg) and on in the
# whole run (k: mgs2_m35_k.cfg, MGS2_NAT_KEEP=15; k7: mgs2_m35_k7.cfg, MGS2_NAT_KEEP=7, the native groups' own look-ups only), timers on
# (as delivered; gl34 measured them at <= 0.5 ms a picture in the model):
#   g35p, g35k, g35k7    the four rain windows in one run (waiting ME 29,810-29,890, waiting main 29,910-29,990, rain walk ME
#                        31,010-31,090, rain walk main 31,110-31,190), as gl34's g34p / g34f
#   g35pc2, g35kc2       the steady deck in d01t (27,410-27,490 an ME turn, 27,510-27,590 a main turn)
#   g35pc1, g35kc1       the opening (16,010-16,090 ME, 16,110-16,190 main; before the native turns)
# three runs at a time.
R=/home/claude/main/build_gl35; P=${1:-$R/psp}; cd $R/m || exit 1; H=$R/cfg/hashref_gl35.txt; C=$R/cfg_emu/mgs2_m35
W4="29810:29890 29910:29990 31010:31090 31110:31190"
sh scripts/pgo.sh model $P g35p ${C}_p.cfg $H $W4 > $R/pgo_g35p.out 2>&1 &
sh scripts/pgo.sh model $P g35k ${C}_k.cfg $H $W4 > $R/pgo_g35k.out 2>&1 &
sh scripts/pgo.sh model $P g35k7 ${C}_k7.cfg $H $W4 > $R/pgo_g35k7.out 2>&1 &
wait
sh scripts/pgo.sh model $P g35pc2 ${C}_p.cfg $H 27410:27490 27510:27590 > $R/pgo_g35pc2.out 2>&1 &
sh scripts/pgo.sh model $P g35kc2 ${C}_k.cfg $H 27410:27490 27510:27590 > $R/pgo_g35kc2.out 2>&1 &
sh scripts/pgo.sh model $P g35pc1 ${C}_p.cfg $H 16010:16090 16110:16190 > $R/pgo_g35pc1.out 2>&1 &
wait
sh scripts/pgo.sh model $P g35kc1 ${C}_k.cfg $H 16010:16090 16110:16190 > $R/pgo_g35kc1.out 2>&1 &
wait
echo done
