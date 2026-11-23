#!/bin/sh
# tw41.sh <folder> <tree> <gen_vu.h> (gl41s): the x86 twin as build_gl39/tw39.sh (tw2 = gl39's), only boot_rt.o compiled again (from
# <tree>/boot/boot_x86.c with <tree>/runtime and the given gen_vu.h); every other object as gl39's twin linked it: build_gl39/tw2/x86's
# ge_*.o, xlive_chk.o and gen_nat.o, and gl19's units / IOP objects (scratch x19 = build_gl40/x19/x86's copies).
set -e
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s; MG=/mnt/project-files/mgs2
X=$S/$1/x86; T=$2; GV=$3; O=$S/x19; W=/home/claude/main/build_gl39/tw2/x86
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
GE="ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o ge_xlme.o"
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
if [ "$T" != keep ]; then s=$(date +%s)
  { echo "cmd: gcc $CF -DNAT_HOOK -DMGS2_XLIVE -DMGS2_GEN_VU=\"$GV\" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c -o boot_rt.o"
    gcc $CF -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$GV\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c -o boot_rt.o; } > cc_boot_rt.o.log 2>&1
  echo "boot_rt.o rc $? $(( $(date +%s) - s )) s" >> cc_boot_rt.o.log; tail -n 1 cc_boot_rt.o.log; fi
gcc -o boot boot_rt.o $W/gen_nat.o $(for u in $UNITS; do printf "$O/%s.o " $u; done) $(for u in $IOPO; do printf "$O/%s " $u; done) $W/xlive_chk.o \
  $(for g in $GE; do printf "$W/%s " $g; done) -lm > link.log 2>&1
echo "boot link rc $?"; md5sum boot
