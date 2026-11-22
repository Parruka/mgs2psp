#!/bin/sh
# xbuild_kz.sh <header dir> <out dir> <boot_rt object> (dev, lean_plan 1b: JSIF now from boot/gen_pre.h; the given runtime object): an x86 build (v26/x86/build.sh's recipe) with gen.o and the nine stage objects from <header dir>;
# boot_rt.o, gen_dispatch.o (stages.h = v26's) and the IOP objects are v26's x86 ones.  Then: sh /home/claude/lean/runh.sh <out>/R<r> H<r> <r>
MG=/mnt/project-files/mgs2; W=$1; O=$2; RT=$3; X=/home/claude/v26/x86
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
mkdir -p $O && cd $O || exit 1
cmp -s $W/stages.h $X/../W/stages.h || { echo "stages.h differs from v26's"; exit 1; }
s=$(date +%s); : > cc.status
for o in iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o gen_dispatch.o; do cp $X/$o .; done; cp $RT boot_rt.o
mkdir -p memcard0
cst() { gcc $CF "-DSTAGE_GEN=\"$W/gen_s_$1.h\"" -c $MG/boot/gen_stage.c -o gen_s_$1.o > cc_$1.log 2>&1; echo "$1 $? $(( $(date +%s) - s )) s" >> cc.status; }
( gcc $CF -DGEN_LITE "-DGEN=\"$W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( cst w00a; cst r_title; cst r_tnk0 ) &
( cst d01t; cst init ) &
( cst d00t; cst n_title; cst w01a ) &
wait
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o -lm > link.log 2>&1; echo "link $? $(( $(date +%s) - s )) s" >> cc.status
grep -v " 0 " cc.status | grep -v "^link 0" ; echo done >> cc.status
