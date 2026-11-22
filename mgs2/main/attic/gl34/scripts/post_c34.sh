#!/bin/sh
# post_c34.sh (gl34; post_b34.sh's steps after the deliver folder, which a container restart cut at 14:01): side by side the emulator
# runs of the package (e2: the delivered cfg; e3: the dense cfg) and the model's runs (model34.sh), then the model run of the
# not-drawn arm for the split (g34n4) beside the second build (g7_34.sh).
R=/home/claude/main/build_gl34; cd $R || exit 1
sh em34.sh e2 > $R/em_e2.out 2>&1 &
sh em34.sh e3 $R/cfg_emu/mgs2_gl34_dense.cfg > $R/em_e3.out 2>&1 &
sh model34.sh > $R/model34.out 2>&1
wait
cd $R/m && sh scripts/pgo.sh model $R/psp g34n4 $R/cfg_emu/mgs2_m34_n4.cfg $R/cfg/hashref_gl34.txt 29810:29890 29910:29990 31010:31090 31110:31190 > $R/pgo_g34n4.out 2>&1 &
cd $R && sh g7_34.sh > $R/g7.out 2>&1
wait
echo "post_c34.sh: done $(date -u +%H:%M:%S)"
