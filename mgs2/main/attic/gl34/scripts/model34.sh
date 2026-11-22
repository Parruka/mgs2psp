#!/bin/sh
# model34.sh [psp dir] (gl34's scoring, [C model]): PspProf runs of the final build (default build_gl34/psp), native and in place in
# every turn, with the fast front end off in the whole run (p: cfg_emu/mgs2_m34_p.cfg = gl33's in-place arm's keys + the split's
# timers) and on in the whole run (f: mgs2_m34_f.cfg), and both again with the timers off (p0, f0: MGS2_NAT_SPLIT absent):
#   g34p, g34f, g34p0, g34f0   the four rain windows in one run (waiting ME 29,810-29,890, waiting main 29,910-29,990, rain walk ME
#                              31,010-31,090, rain walk main 31,110-31,190), as gl33's g33p / g33i
#   g34pc2, g34fc2             the steady deck in d01t (27,410-27,490 an ME turn, 27,510-27,590 a main turn), as g33ic2
#   g34pc1, g34fc1             the opening (16,010-16,090 ME, 16,110-16,190 main; before the native turns: the lever cannot act)
# three runs at a time.
R=/home/claude/main/build_gl34; P=${1:-$R/psp}; cd $R/m || exit 1; H=$R/cfg/hashref_gl34.txt; C=$R/cfg_emu/mgs2_m34
W4="29810:29890 29910:29990 31010:31090 31110:31190"
sh scripts/pgo.sh model $P g34p ${C}_p.cfg $H $W4 > $R/pgo_g34p.out 2>&1 &
sh scripts/pgo.sh model $P g34f ${C}_f.cfg $H $W4 > $R/pgo_g34f.out 2>&1 &
sh scripts/pgo.sh model $P g34p0 ${C}_p0.cfg $H $W4 > $R/pgo_g34p0.out 2>&1 &
wait
sh scripts/pgo.sh model $P g34f0 ${C}_f0.cfg $H $W4 > $R/pgo_g34f0.out 2>&1 &
sh scripts/pgo.sh model $P g34pc2 ${C}_p.cfg $H 27410:27490 27510:27590 > $R/pgo_g34pc2.out 2>&1 &
sh scripts/pgo.sh model $P g34fc2 ${C}_f.cfg $H 27410:27490 27510:27590 > $R/pgo_g34fc2.out 2>&1 &
wait
sh scripts/pgo.sh model $P g34pc1 ${C}_p.cfg $H 16010:16090 16110:16190 > $R/pgo_g34pc1.out 2>&1 &
sh scripts/pgo.sh model $P g34fc1 ${C}_f.cfg $H 16010:16090 16110:16190 > $R/pgo_g34fc1.out 2>&1 &
wait
echo done
