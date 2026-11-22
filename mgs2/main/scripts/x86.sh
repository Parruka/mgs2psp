#!/bin/sh
# x86.sh <obj | dump | twin>: the x86 builds in $B/x86 (game data inside: container only).
#  obj   (gl19: gen.o from gen/wh, WP's gen_elf.h with tools/patch_half.py's two lines: MGS2_HALF)
#        the game's units as tools/dev/x86_ref.sh compiles them (boot/Makefile.probe's CF with GEN_MM_FAST GUEST_SPR0; gen.c -DGEN_LITE;
#        the units from gen/WP, w00a from gen/wpl with boot/'s native_w00a.c, as rain/g0/cc_w00a.sh) and the IOP objects (boot.c's
#        documented line: -O1 -w -I runtime -I $RT -fwrapv -fno-strict-aliasing; spu2_lite as Makefile.probe: -O2 -g0 -w)
#  dump  boot_dump: the runtime with -DVU_PROFILE and no compiled VU0 programs (every micro program interpreted; boot_x86.c vu_prof saves
#        each image met as <dir>/<hash>.bin with MGS2_VU_PROF=<dir>:<vb>: the v21-v22 census's way), the input of step vu (recomp/vupipe.py).
#        (vu0.h's VU_DUMP is not used: today's ee_io.h VIF0 MPG paths do not mark its image dirty, so it saves the first image only)
#  twin  boot: the x86 twin of gl18me as rain/g0/mk.sh builds it (sb17's twin with the G0 tree's runtime): boot/boot_x86.c with
#        -DMGS2_XLIVE, the AOT VU0 programs gen/vp22/gen_vu.h (-DVP_FAST -DVP_VFPU), ge/ with -DGU_SENT -DXL_CENSUS, boot/xlive_chk.c
. "$(dirname "$0")/env.sh"
X=$B/x86; mkdir -p $X; cd $X || exit 1
CF="-O1 -g0 -w -I$M/runtime -I$M/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
UNITS="gen gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() {   # <object> <flags...>: one compile, its log and status
  o=$1; shift; s=$(date +%s)
  gcc "$@" -o $o > cc_$o.log 2>&1; echo "$o rc $? $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log
}
case $1 in
obj)
  cu gen.o $CF -DGEN_LITE "-DGEN=\"$G/wh/gen_elf.h\"" -c $M/boot/gen.c &   # gl19: WP's with MGS2_HALF's two lines (hdr.sh, patch_half.py)
  cu gen_s_w00a.o $CF "-DSTAGE_GEN=\"$G/wpl/gen_s_w00a.h\"" -c $M/boot/gen_stage.c &
  cu gen_s_w01a.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_w01a.h\"" -c $M/boot/gen_stage.c &
  wait
  ( for u in d00t init r_tnk0; do cu gen_s_$u.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_$u.h\"" -c $M/boot/gen_stage.c; done ) &
  ( for u in d01t n_title; do cu gen_s_$u.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_$u.h\"" -c $M/boot/gen_stage.c; done ) &
  ( cu gen_s_r_title.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_r_title.h\"" -c $M/boot/gen_stage.c
    cu gen_dispatch.o $CF "-DSTAGES=\"$G/WP/stages.h\"" -c $M/boot/gen_dispatch.c
    IF="-O1 -g0 -w -I$M/runtime -I$RT -fwrapv -fno-strict-aliasing"
    for f in iop_rt iop_rt_libs iop_rt_ucontext; do cu $f.o $IF -c $RT/$f.c; done
    for f in jsifman cdbios iop_main; do cu $f.o $IF -c $IG/$f.c; done
    cu spu2_lite.o -O2 -g0 -w -c $AU/spu2_lite.c ) &
  wait ;;
dump)
  cu boot_rt_dump.o $CF -DVU_PROFILE -I$RT -I$AU -DGUEST_SPLIT_RT -c $M/boot/boot_x86.c
  gcc -o boot_dump boot_rt_dump.o $(for u in $UNITS; do printf '%s.o ' $u; done) $IOPO -lm > link_dump.log 2>&1; echo "boot_dump link rc $?" ;;
twin)
  [ -f $G/vp22/gen_vu.h ] || { echo "x86.sh twin: no $G/vp22/gen_vu.h (step vu)"; exit 1; }
  cu boot_rt.o $CF -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $M/boot/boot_x86.c
  GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
  for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model; do
    cu ge_$f.o $GF -c $M/ge/$f.c & done; wait
  cu ge_gsup.o $GF -c $M/ge/gsup2.c
  cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT -c $M/boot/xlive_chk.c
  gcc -o boot boot_rt.o $(for u in $UNITS; do printf '%s.o ' $u; done) $IOPO xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o \
    ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o -lm > link.log 2>&1
  echo "boot link rc $?"; tail -n 20 link.log ;;
*) echo "x86.sh obj | dump | twin"; exit 1 ;;
esac
