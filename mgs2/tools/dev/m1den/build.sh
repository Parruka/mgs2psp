#!/bin/sh
# build.sh <dir>: the v21 x86 reference (xlt) with every generated unit recompiled from /home/claude/m1den/W (lwc1 / mtc1 sites) and
# guest.h from /home/claude/m1den/rt (M1DEN), plus m1den.o
D=$1; cd $D || exit 1
MG=/mnt/project-files/mgs2; W=${M1W:-/home/claude/m1den/W}; RT=${M1RT:-/home/claude/m1den/rt}   # v30: M1W, M1RT, M1C (m1den.c)
CF="-O1 -g0 -w -I$RT -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DM1DEN $EXTRA"
( gcc $CF -DGEN_LITE "-DGEN=\"$W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $?" >> cc.status ) &
for s in w00a w01a d00t; do ( gcc $CF "-DSTAGE_GEN=\"$W/gen_s_$s.h\"" -c $MG/boot/gen_stage.c -o gen_s_$s.o > cc_$s.log 2>&1; echo "$s $?" >> cc.status ) & done
wait
for s in d01t n_title init r_title r_tnk0; do ( gcc $CF "-DSTAGE_GEN=\"$W/gen_s_$s.h\"" -c $MG/boot/gen_stage.c -o gen_s_$s.o > cc_$s.log 2>&1; echo "$s $?" >> cc.status ) & done
gcc -O1 -g0 -c ${M1C:-/home/claude/m1den/m1den.c} -o m1den.o; echo "m1den $?" >> cc.status
wait
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o m1den.o -lm > link.log 2>&1; echo "link $?" >> cc.status
echo built > built
