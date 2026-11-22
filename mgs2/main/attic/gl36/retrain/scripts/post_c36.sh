#!/bin/sh
# post_c36.sh (gl36 retrain, after retrain/rt_b.sh): the deliver folder from the final build (build_gl36/psp) with the retrain's cfg
# (cfg/mgs2_gl36.cfg: + MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0), then the emulator runs of the package side by side, two at a time:
#   e2 the delivered cfg; e3 the dense cfg (+ digests over the whole run, a shot every 2 vblanks in the walk);
#   then the second build (g7_36.sh: reproducible) beside e5 (cfg_emu/mgs2_gl36_chk.cfg: reuse and cull in every turn with both check
#   modes, MGS2_NAT_RCHK=1 and MGS2_NAT_CULLCHK=1 on the ME objects) and e6 (cfg_emu/mgs2_gl36_off.cfg: both levers off).
# Each game folder is deleted after its run (literal path).
R=/home/claude/main/build_gl36; cd $R || exit 1
grep -q 'rt_b.sh: done' $R/rt_b.out && [ -f $R/psp/EBOOT.PBP ] || { echo "post_c36.sh: no final build"; exit 1; }
[ -e $R/deliver ] && { echo "post_c36.sh: deliver exists"; exit 1; }
mkdir -p $R/deliver && cp $R/psp/EBOOT.PBP $R/psp/ovl_d00t.bin $R/psp/ovl_d01t.bin $R/psp/ovl_init.bin $R/psp/ovl_n_title.bin $R/psp/ovl_r_title.bin \
  $R/psp/ovl_r_tnk0.bin $R/psp/ovl_w00a.bin $R/psp/ovl_w01a.bin $R/deliver/ && cp $R/cfg/mgs2_gl36.cfg $R/deliver/mgs2.cfg \
  && cp $R/cfg/hashref_gl36.txt $R/deliver/hashref.txt && cp /home/claude/main/build_gl35/deliver/pad.txt $R/deliver/pad.txt || exit 1
( cd $R/deliver && md5sum EBOOT.PBP ovl_*.bin mgs2.cfg hashref.txt pad.txt )
sed -e '/^MGS2_GL_DIG=/d' -e '/^MGS2_GL_SHOT_RANGE=/d' $R/cfg/mgs2_gl36.cfg > $R/cfg_emu/mgs2_gl36_dense.cfg
{ echo "# dense emulator run (gl35 G3): digests over the whole run and a shot every 2 vblanks in the walk"; echo "MGS2_GL_DIG=0:31400"; echo "MGS2_GL_SHOT_RANGE=30500:31400:2"; } >> $R/cfg_emu/mgs2_gl36_dense.cfg
for t in e2 e3 e5 e6; do [ -e $R/emu_$t ] && mv $R/emu_$t $R/pre_cull/emu_$t; done
( nice -n 10 sh em36.sh e2 > $R/em_e2.out 2>&1; rm -rf "$R/ms/em_e2" ) &
( nice -n 10 sh em36.sh e3 $R/cfg_emu/mgs2_gl36_dense.cfg > $R/em_e3.out 2>&1; rm -rf "$R/ms/em_e3" ) &
wait
( nice -n 10 sh em36.sh e5 $R/cfg_emu/mgs2_gl36_chk.cfg > $R/em_e5.out 2>&1; rm -rf "$R/ms/em_e5"
  nice -n 10 sh em36.sh e6 $R/cfg_emu/mgs2_gl36_off.cfg > $R/em_e6.out 2>&1; rm -rf "$R/ms/em_e6" ) &
for d in u2 psp2; do [ -e $R/$d ] && mv $R/$d $R/pre_cull/$d; done
nice -n 10 sh g7_36.sh > $R/g7.out 2>&1
wait
echo "post_c36.sh: done $(date -u +%H:%M:%S)"
