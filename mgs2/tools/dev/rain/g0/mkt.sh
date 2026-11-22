#!/bin/sh
# sbg0 (task #165): mkt.sh: the read-hash proof build of G0H in /home/claude/sbg0/xt: every game unit (v31's headers, WPL) with GEN_DG_TRACE
# (every load and store reports itself: boot.c MGS2_READ_HASH), w00a with the private nat/ and NAT_IN_TRACE (line_sph's native form and G0
# kept, untraced), the runtime from this tree's boot.c with GEN_DG_TRACE and no live picture (the game's state does not depend on it).
MG=/mnt/project-files/mgs2; W=/home/claude/v31/WPL; R=/home/claude/v31/x86; T=/home/claude/sbg0; VP=/home/claude/psp/vp22; O=$T/xt
mkdir -p $O && cd $O || exit 1
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE"
s=$(date +%s); : > cc.status
cst() { nice gcc $CF "-DSTAGE_GEN=\"$W/gen_s_$1.h\"" -c $MG/boot/gen_stage.c -o gen_s_$1.o > cc_$1.log 2>&1; echo "$1 $? $(( $(date +%s) - s )) s" >> cc.status; }
( nice gcc $CF -DGEN_LITE "-DGEN=\"$W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( nice gcc $CF "-DSTAGES=\"$W/stages.h\"" -c $MG/boot/gen_dispatch.c -o gen_dispatch.o > cc_disp.log 2>&1; echo "dispatch $? $(( $(date +%s) - s )) s" >> cc.status
  nice gcc -O1 -g0 -w -I$T/runtime -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE \
    "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT \
    -c $T/boot/boot.c -o boot_rt.o > cc_rt.log 2>&1; echo "rt $? $(( $(date +%s) - s )) s" >> cc.status ) &
( for u in d00t d01t init; do cst $u; done ) &
( for u in n_title r_title r_tnk0 w01a; do cst $u; done ) &
( nice gcc -O1 -g0 -w -I$T/nat -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE -DNAT_IN_TRACE \
    "-DSTAGE_GEN=\"$W/gen_s_w00a.h\"" -c $MG/boot/gen_stage.c -o gen_s_w00a.o > cc_w00a.log 2>&1; echo "w00a $? $(( $(date +%s) - s )) s" >> cc.status ) &
wait
for o in iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do ln -sf $R/$o.o $o.o; done
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o \
  gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o -lm > link.log 2>&1
echo "link $? $(( $(date +%s) - s )) s" >> cc.status; cat cc.status
