#!/bin/sh
# sbg0 (task #167, H3R): mkt_h3r.sh: the read-hash proof build of H3R in /home/claude/sbg0/xt5 (the unit with H3R as well); as mkt_cs.sh (task #165, G0C, xt4): as mkt_h3.sh (xt's objects, boot_rt.o from this
# tree's boot.c with GEN_DG_TRACE: g_rh_off) with the G0C copy of v31's w00a header (/home/claude/sbg0/wpl: CS_RH around charaspread's sets,
# message 3 H3-wrapped) and the native code (-DNAT_IN_TRACE: nat_cs_sets), -g1 (its line table maps host offsets to w00a's guest addresses).
MG=/mnt/project-files/mgs2; W=/home/claude/sbg0/wpl; T=/home/claude/sbg0; VP=/home/claude/psp/vp22; O=$T/xt5
mkdir -p $O && cd $O || exit 1
s=$(date +%s); : > cc.status
for o in gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w01a iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do
  ln -sf $T/xt/$o.o $o.o; done
[ -f boot_rt.o ] && [ boot_rt.o -nt $T/boot/boot.c ] && echo "rt kept" >> cc.status || ( nice gcc -O1 -g0 -w -I$T/runtime -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE \
    "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT \
    -c $T/boot/boot.c -o boot_rt.o > cc_rt.log 2>&1; echo "rt $? $(( $(date +%s) - s )) s" >> cc.status ) &
( nice gcc -O1 -g1 -pipe -w -I$T/nat -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE -DNAT_IN_TRACE \
    "-DSTAGE_GEN=\"$W/gen_s_w00a.h\"" -c $MG/boot/gen_stage.c -o gen_s_w00a.o > cc_w00a.log 2>&1; echo "w00a $? $(( $(date +%s) - s )) s" >> cc.status ) &
wait
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o \
  gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o -lm > link.log 2>&1
echo "link $? $(( $(date +%s) - s )) s" >> cc.status; cat cc.status
