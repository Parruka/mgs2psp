#!/bin/sh
# rh43.sh (gl43): the READ-HASH twin (speed_notes s34's method): every unit compiled with -DGEN_DG_TRACE (every guest load outside the
# draw pipeline hashed per vblank with MGS2_READ_HASH=1), as tw43.sh otherwise: gen.o from gen/wh (P1214a0, P12e8b8, P12e790 weakened
# behind their wrappers), w00a from gen43/wpl (B; its floor and fall acts' loads left out of the hash in both modes, native_b43.h),
# the other stages from gen/WP, the IOP objects from build_gl40/x19.  The native forms of native_w00a.c are off in a trace build (NAT_ON),
# in both arms alike.  Into build_gl43/rh/x86; nice 19.
set -e
M=/home/claude/main; MG=/mnt/project-files/mgs2; T=/home/claude/t43; X=$M/build_gl43/rh/x86; O=$M/build_gl40/x19/x86; G=$M/gen; GV=$T/gen41/gen_vu.h
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; nice -n 19 gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE"
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
cu gen.o $CF -DGEN_LITE "-DGEN=\"$G/wh/gen_elf.h\"" -c $T/boot/gen.c &
cu gen_s_w00a.o $CF "-DSTAGE_GEN=\"$M/gen43/wpl/gen_s_w00a.h\"" -c $T/boot/gen_stage.c &
cu gen_s_w01a.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_w01a.h\"" -c $T/boot/gen_stage.c &
wait
( for u in d00t init r_tnk0; do cu gen_s_$u.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_$u.h\"" -c $T/boot/gen_stage.c; done ) &
( for u in d01t n_title; do cu gen_s_$u.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_$u.h\"" -c $T/boot/gen_stage.c; done ) &
( cu gen_s_r_title.o $CF "-DSTAGE_GEN=\"$G/WP/gen_s_r_title.h\"" -c $T/boot/gen_stage.c
  cu gen_dispatch.o $CF "-DSTAGES=\"$G/WP/stages.h\"" -c $T/boot/gen_dispatch.c
  cu boot_rt.o $CF -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$GV\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c ) &
wait
for f in ../../tw/x86/ge_*.o ../../tw/x86/xlive_chk.o; do cp $f .; done
OC=; for f in P1214a0 P12e8b8 P12e790; do
  a=$(nm gen.o | awk -v f=$f '$3 == f && $2 == "T" { print $1 }'); [ -n "$a" ] || { echo "rh43.sh: no $f in gen.o"; exit 1; }
  OC="$OC --add-symbol ${f}_orig=.text:0x$a,global,function --weaken-symbol $f"; done
objcopy $OC gen.o gen_nat.o
nice -n 19 gcc -o boot boot_rt.o gen_nat.o gen_dispatch.o gen_s_init.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_n_title.o gen_s_d00t.o gen_s_d01t.o gen_s_w00a.o gen_s_w01a.o \
  $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_*.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log
