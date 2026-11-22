#!/bin/sh
# build_xlive.sh <out dir> <x86 reference dir> [vp dir]: the x86 check build of the live translator (renderer_notes 11.10 step 6).
# The reference dir holds an x86 build of the same version (e.g. v26's, made with its build.sh: gen*.o, iop*.o, jsifman.o, cdbios.o,
# spu2_lite.o); this links those with boot_rt.o from boot/boot.c built with -DMGS2_XLIVE (the reference's flags otherwise), boot/xlive_chk.c
# and ge/ (xlive, xlate with XL_CENSUS, gsfront, gsup with GU_SENT, xldig, texconv, texcache; gelive, gebatch, xconv, vu1conv: the live
# renderer's frame, MGS2_XLIVE_GB; gelist: its GE list).  Without MGS2_XLIVE in the environment the hooks do nothing.
# vp dir: where gen_vu.h of that version is (default /home/claude/psp/vp22).
# GU=lean in the environment links ge/gsup2.c (the lean texture map) instead of ge/gsup.c; GU=dual links both side by side behind
# boot/gudual.c (gsup.c renamed gr_, gsup2.c gl_ by ge/gu_ren.h), which compares them (renderer_notes 11.12, task #47).
O=$1; R=$2; VP=${3:-/home/claude/psp/vp22}
MG=/mnt/project-files/mgs2
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
mkdir -p $O && cd $O || exit 1
for o in gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w00a gen_s_w01a iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do
  [ -f $R/$o.o ] || { echo "no $R/$o.o"; exit 1; }; ln -sf $R/$o.o $o.o; done
gcc $CF -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT -c $MG/boot/boot.c -o boot_rt.o || exit 1
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist; do gcc $GF -c $MG/ge/$f.c -o ge_$f.o || exit 1; done
case "$GU" in
  lean) gcc $GF -c $MG/ge/gsup2.c -o ge_gsup.o || exit 1; GUO=ge_gsup.o ;;
  dual) gcc $GF -DGU_RP=gr_ -DGU_RT=Gr -include $MG/ge/gu_ren.h -c $MG/ge/gsup.c -o ge_gsup_r.o || exit 1
        gcc $GF -DGU_RP=gl_ -DGU_RT=Gl -include $MG/ge/gu_ren.h -c $MG/ge/gsup2.c -o ge_gsup_l.o || exit 1
        gcc $GF -c $MG/boot/gudual.c -o gudual.o || exit 1; GUO="ge_gsup_r.o ge_gsup_l.o gudual.o" ;;
  *) gcc $GF -c $MG/ge/gsup.c -o ge_gsup.o || exit 1; GUO=ge_gsup.o ;;
esac
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -c $MG/boot/xlive_chk.c -o xlive_chk.o || exit 1
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o $GUO ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o -lm || exit 1
echo built
