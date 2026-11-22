#!/bin/sh
# model33.sh [psp dir] (gl33's scoring, [C model]): PspProf runs of the final build (default build_gl33/psp), native in every turn,
# with the in-place rule off in the whole run (p: cfg_emu/mgs2_m33_p.cfg) and on in the whole run (i: cfg_emu/mgs2_m33_i.cfg):
#   g33p, g33i    the four windows of gl32's scoring in one run (waiting ME 29,810-29,890, waiting main 29,910-29,990, rain ME
#                 31,010-31,090, rain main 31,110-31,190), as build_gl32/model/m32n3 ran gl31's ELF
#   g33pr, g33ir  the two rain windows in a run of their own (no window before them), as build_gl33/model/m31r
#   g33pc2, g33ic2  the steady deck in d01t (27,410-27,490 an ME turn, 27,510-27,590 a main turn), as m31c2
#   g33pc1        the opening (16,010-16,090 ME, 16,110-16,190 main; before the native turns: the rule cannot act), as m31c1
# three runs at a time.
R=/home/claude/main/build_gl33; P=${1:-$R/psp}; cd $R/m || exit 1; H=$R/cfg/hashref_gl33.txt; Cp=$R/cfg_emu/mgs2_m33_p.cfg; Ci=$R/cfg_emu/mgs2_m33_i.cfg
W4="29810:29890 29910:29990 31010:31090 31110:31190"
sh scripts/pgo.sh model $P g33p $Cp $H $W4 > $R/pgo_g33p.out 2>&1 &
sh scripts/pgo.sh model $P g33i $Ci $H $W4 > $R/pgo_g33i.out 2>&1 &
sh scripts/pgo.sh model $P g33pc1 $Cp $H 16010:16090 16110:16190 > $R/pgo_g33pc1.out 2>&1 &
wait
sh scripts/pgo.sh model $P g33pr $Cp $H 31010:31090 31110:31190 > $R/pgo_g33pr.out 2>&1 &
sh scripts/pgo.sh model $P g33ir $Ci $H 31010:31090 31110:31190 > $R/pgo_g33ir.out 2>&1 &
sh scripts/pgo.sh model $P g33pc2 $Cp $H 27410:27490 27510:27590 > $R/pgo_g33pc2.out 2>&1 &
wait
sh scripts/pgo.sh model $P g33ic2 $Ci $H 27410:27490 27510:27590 > $R/pgo_g33ic2.out 2>&1
echo done
