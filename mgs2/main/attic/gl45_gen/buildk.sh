#!/bin/sh
# gl45 batch 3: the x86 twin with the PSP's clock form (-DK_COUNTDOWN: the 32-bit countdown k_left of runtime/ee_io.h), so that lever (c)'s
# PSP form runs on the twin: kb/x86/boot_k (the units as scripts/x86.sh obj + twin compile them, plus -DK_COUNTDOWN; the IOP and GE
# objects are build/x86's) and kb/x86/boot_kabc (the two hot units from the x_abc headers with -DGEN_LEV).  Results: out/buildk.txt
M=/home/claude/main; . $M/scripts/env.sh
X=/home/claude/g45/kb/x86; mkdir -p $X; cd $X || exit 1; O=/home/claude/g45/out/buildk.txt; : > $O; GX=$M/gen/g45
CF="-O1 -g0 -w -I$M/runtime -I$M/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DK_COUNTDOWN"
cu() { o=$1; shift; s=$(date +%s); gcc "$@" -o $o > cc_$o.log 2>&1; echo "$o rc $? $(( $(date +%s) - s )) s" >> $O; }
( cu gen.o $CF -DGEN_LITE "-DGEN=\"$G/wh/gen_elf.h\"" -c $M/boot/gen.c
  cu gen_s_w01a.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_w01a.h\"" -c $M/boot/gen_stage.c
  for u in d00t init r_tnk0 d01t; do cu gen_s_$u.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_$u.h\"" -c $M/boot/gen_stage.c; done
  cu gen_abc.o $CF -DGEN_LEV -DGEN_LITE "-DGEN=\"$GX/x_abc/gen_elf.h\"" -c $M/boot/gen.c ) &
( cu gen_s_w00a.o $CF "-DSTAGE_GEN=\"$G/wpl/gen_s_w00a.h\"" -c $M/boot/gen_stage.c
  for u in n_title r_title; do cu gen_s_$u.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_$u.h\"" -c $M/boot/gen_stage.c; done
  cu gen_dispatch.o $CF "-DSTAGES=\"$G/WP/stages.h\"" -c $M/boot/gen_dispatch.c
  cu boot_rt.o $CF -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $M/boot/boot_x86.c
  cu gen_s_w00a_abc.o $CF -DGEN_LEV "-DSTAGE_GEN=\"$GX/x_abc/gen_s_w00a.h\"" -c $M/boot/gen_stage.c ) &
wait
P=$M/build/x86
UN="gen_dispatch.o gen_s_init.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_n_title.o gen_s_d00t.o gen_s_d01t.o gen_s_w01a.o"
IOPO="$P/iop_main.o $P/iop_rt.o $P/iop_rt_libs.o $P/iop_rt_ucontext.o $P/jsifman.o $P/cdbios.o $P/spu2_lite.o"
GE=$(for f in xlive_chk ge_xlive ge_xlate ge_gsfront ge_gsup ge_xldig ge_texconv ge_texcache ge_gelive ge_gebatch ge_xconv ge_vu1conv ge_gelist ge_fxthin ge_c1model ge_c2model ge_c3model; do printf '%s ' $P/$f.o; done)
gcc -o $P/boot_k boot_rt.o gen.o gen_s_w00a.o $UN $IOPO $GE -lm > link_k.log 2>&1; echo "boot_k link rc $?" >> $O
gcc -o $P/boot_kabc boot_rt.o gen_abc.o gen_s_w00a_abc.o $UN $IOPO $GE -lm > link_kabc.log 2>&1; echo "boot_kabc link rc $?" >> $O
echo "buildk done" >> $O
