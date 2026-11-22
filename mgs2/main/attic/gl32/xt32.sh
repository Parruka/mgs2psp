#!/bin/sh
# xt32.sh (gl31 G2; build_gl30n/xt30.sh on /home/claude/t32): the read-hash twin of /home/claude/t32 in build_gl32/xt32: the runtime from t31/boot/boot_x86.c with
# -DGEN_DG_TRACE -DDG_LITE_DYN (every guest load goes to the tracer: MGS2_READ_HASH, MGS2_READ_LOG) and the native stub (nat_stub.h), the
# game's units as the design's proof build made them (scratchpad native/xt/mkt.sh: main's tree with GEN_DG_TRACE; gen.o with GEN_LITE +
# DG_LITE_DYN; read in place), gen.o's P1214a0 made weak as in tw32.sh, the ge objects of build_gl32/tw/x86 (the picture is not on in
# these runs: the stub's choices do not depend on it).
set -e
M=/home/claude/main; MG=/mnt/project-files/mgs2; T=/home/claude/t32; X=$M/build_gl32/xt32; G=$M/gen; W=$M/build_gl32/tw/x86
XT=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/native/xt; O=$M/build/x86
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE"
{ echo "cmd: gcc $CF ..."; gcc $CF -DDG_LITE_DYN -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT \
    -c $T/boot/boot_x86.c -o boot_rt.o; } > cc_boot_rt.log 2>&1; echo "boot_rt.o rc $?"
OC=; for f in P1214a0 P129128 P12cf28 P12cd78; do   # gl32: and the vertex cache's three event functions (boot/nat_stub.h)
  a=$(nm $XT/gen.o | awk -v f=$f '$3 == f && $2 == "T" { print $1 }'); [ -n "$a" ] || { echo "xt32.sh: no $f in $XT/gen.o"; exit 1; }
  OC="$OC --add-symbol ${f}_orig=.text:0x$a,global,function --weaken-symbol $f"; done
objcopy $OC $XT/gen.o gen_nat.o
echo "gen_nat.o: $(nm gen_nat.o | grep 'P1214a0\|P129128\|P12cf28\|P12cd78' | tr '\n' ';')"
gcc -o boot boot_rt.o gen_nat.o $XT/gen_dispatch.o $XT/gen_s_d00t.o $XT/gen_s_d01t.o $XT/gen_s_init.o $XT/gen_s_n_title.o $XT/gen_s_r_title.o \
  $XT/gen_s_r_tnk0.o $XT/gen_s_w00a.o $XT/gen_s_w01a.o $O/iop_main.o $O/iop_rt.o $O/iop_rt_libs.o $O/iop_rt_ucontext.o $O/jsifman.o $O/cdbios.o \
  $O/spu2_lite.o $W/xlive_chk.o $W/ge_xlive.o $W/ge_xlate.o $W/ge_gsfront.o $W/ge_gsup.o $W/ge_xldig.o $W/ge_texconv.o $W/ge_texcache.o $W/ge_gelive.o \
  $W/ge_gebatch.o $W/ge_xconv.o $W/ge_vu1conv.o $W/ge_gelist.o $W/ge_fxthin.o $W/ge_c1model.o $W/ge_c2model.o $W/ge_c3model.o $W/ge_xlme.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log
