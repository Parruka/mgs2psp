#!/bin/sh
# build.sh: the play probe (x86): full ELF + full w00a units (h1size/full_l, v23 header pipeline, no prune) with coverage (GEN_COVER) and
# guest instruction counts (GEN_GI_COUNT, tools/dev/gi_count.py); boot.c with the fork server (play/rt); other units from m7/x
MG=/mnt/project-files/mgs2; P=/home/claude/play; X=$P/x
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
GF="-DGEN_COVER -DGEN_GI_COUNT -include /home/claude/gi_x86/gi.h"
cd $X || exit 1
s=$(date +%s)
[ -f $P/W/gen_elf.h ] || python3 $MG/tools/dev/gi_count.py /home/claude/h1size/full_l/gen_elf.h $P/W/gen_elf.h
[ -f $P/W/gen_s_w00a.h ] || python3 $MG/tools/dev/gi_count.py /home/claude/h1size/full_l/gen_s_w00a.h $P/W/gen_s_w00a.h
echo "gi headers $(( $(date +%s) - s )) s"
( gcc $CF $GF "-DGEN=\"$P/W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( gcc $CF $GF "-DSTAGE_GEN=\"$P/W/gen_s_w00a.h\"" -c $MG/boot/gen_stage.c -o gen_s_w00a.o > cc_w00a.log 2>&1; echo "w00a $? $(( $(date +%s) - s )) s" >> cc.status ) &
gcc $CF "-DSTAGES=\"$P/W/stages.h\"" -c $MG/boot/gen_dispatch.c -o gen_dispatch.o > cc_disp.log 2>&1; echo "dispatch $?" >> cc.status
gcc $CF -I$P/rt "-DMGS2_GEN_VU=\"/home/claude/psp/vp16n/gen_vu.h\"" -DVP_FAST -DVP_VFPU -DVPV_STATS -DGEN_GI_COUNT -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT -c $P/rt/boot.c -o boot_rt.o > cc_rt.log 2>&1; echo "rt $?" >> cc.status
wait
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o -lm > link.log 2>&1; echo "link $? $(( $(date +%s) - s )) s" >> cc.status
