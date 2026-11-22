#!/bin/sh
# post_c35.sh (gl35; post_b35.sh's steps after the deliver folder, which a container restart cut at 14:01): side by side the emulator
# runs of the package (e2: the delivered cfg; e3: the dense cfg) and the model's runs (model35.sh), then the model run of the
# not-drawn arm for the split (g35n4) beside the second build (g7_35.sh).
R=/home/claude/main/build_gl35; cd $R || exit 1
sh em35.sh e2 > $R/em_e2.out 2>&1 &
sh em35.sh e3 $R/cfg_emu/mgs2_gl35_dense.cfg > $R/em_e3.out 2>&1 &
sh model35.sh > $R/model35.out 2>&1
wait
cd $R/m && sh scripts/pgo.sh model $R/psp g35n4 $R/cfg_emu/mgs2_m35_n4.cfg $R/cfg/hashref_gl35.txt 29810:29890 29910:29990 31010:31090 31110:31190 > $R/pgo_g35n4.out 2>&1 &
cd $R && sh g7_35.sh > $R/g7.out 2>&1
wait
echo "post_c35.sh: done $(date -u +%H:%M:%S)"
