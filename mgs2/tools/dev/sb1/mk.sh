#!/bin/sh
# mk.sh <out dir> [ge flags]: the x86 twin (as codec68/build_xlive_ge.sh, GU=lean, v31's game objects) from this private tree:
# runtime/ee_io.h ahead of the shared runtime, boot/xlive_chk.c and ge/ from here.  Stage B lane 1 work (renderer_notes 11.20, task #100).
O=$1; shift; T=/home/claude/sb1; R=/home/claude/v31/x86; VP=/home/claude/psp/vp22; MG=/mnt/project-files/mgs2
CF="-O1 -g0 -w -I$T/runtime -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
mkdir -p $O && cd $O || exit 1
for o in gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w00a gen_s_w01a iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do
  ln -sf $R/$o.o $o.o; done
if [ ! -f boot_rt.o ] || [ $T/runtime/ee_io.h -nt boot_rt.o ]; then
gcc $CF -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT -c $MG/boot/boot.c -o boot_rt.o || exit 1; fi
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS $*"
for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist; do gcc $GF -c $T/ge/$f.c -o ge_$f.o || exit 1; done
gcc $GF -c $T/ge/gsup2.c -o ge_gsup.o || exit 1
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -c $T/boot/xlive_chk.c -o xlive_chk.o || exit 1
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o -lm || exit 1
echo built $O
