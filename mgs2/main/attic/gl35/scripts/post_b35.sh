#!/bin/sh
# post_b35.sh (gl35, after pgo_b35.sh): the deliver folder from the final build (build_gl35/psp), then side by side the emulator runs of
# the package (e2: the delivered cfg; e3: the dense cfg = the delivered cfg + digests over the whole run + a shot every 2 vblanks in the
# walk) and the model's runs (model35.sh), then the model run of the not-drawn arm for the split (g35n4), the second build (g7_35.sh).
R=/home/claude/main/build_gl35; cd $R || exit 1
grep -q 'pgo_b35.sh: done' $R/pgo_b35.out && [ -f $R/psp/EBOOT.PBP ] || { echo "post_b35.sh: no final build"; exit 1; }
[ -e $R/deliver ] && { echo "post_b35.sh: deliver exists"; exit 1; }
mkdir -p $R/deliver && cp $R/psp/EBOOT.PBP $R/psp/ovl_d00t.bin $R/psp/ovl_d01t.bin $R/psp/ovl_init.bin $R/psp/ovl_n_title.bin $R/psp/ovl_r_title.bin \
  $R/psp/ovl_r_tnk0.bin $R/psp/ovl_w00a.bin $R/psp/ovl_w01a.bin $R/deliver/ && cp $R/cfg/mgs2_gl35.cfg $R/deliver/mgs2.cfg \
  && cp $R/cfg/hashref_gl35.txt $R/deliver/hashref.txt && cp /home/claude/main/build_gl33/deliver/pad.txt $R/deliver/pad.txt || exit 1
( cd $R/deliver && md5sum EBOOT.PBP ovl_*.bin mgs2.cfg hashref.txt pad.txt )
sed -e '/^MGS2_GL_DIG=/d' -e '/^MGS2_GL_SHOT_RANGE=/d' $R/cfg/mgs2_gl35.cfg > $R/cfg_emu/mgs2_gl35_dense.cfg
{ echo "# dense emulator run (gl35 G3): digests over the whole run and a shot every 2 vblanks in the walk"; echo "MGS2_GL_DIG=0:31400"; echo "MGS2_GL_SHOT_RANGE=30500:31400:2"; } >> $R/cfg_emu/mgs2_gl35_dense.cfg
sh em35.sh e2 > $R/em_e2.out 2>&1 &
sh em35.sh e3 $R/cfg_emu/mgs2_gl35_dense.cfg > $R/em_e3.out 2>&1 &
sh model35.sh > $R/model35.out 2>&1
wait
cd $R/m && sh scripts/pgo.sh model $R/psp g35n4 $R/cfg_emu/mgs2_m35_n4.cfg $R/cfg/hashref_gl35.txt 29810:29890 29910:29990 31010:31090 31110:31190 > $R/pgo_g35n4.out 2>&1 &
cd $R && sh g7_35.sh > $R/g7.out 2>&1
wait
echo "post_b35.sh: done $(date -u +%H:%M:%S)"
