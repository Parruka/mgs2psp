#!/bin/sh
# rh44.sh (gl44): the READ-HASH twin for V44 (build_gl43/rh43.sh's method): every unit with -DGEN_DG_TRACE.  The units V44 does not touch are
# gl43's trace objects (build_gl43/rh/x86: gen_nat.o, gen_dispatch.o and the other stages; recompiling gen_s_r_tnk0.o and gen_dispatch.o from
# t44 gave the same bytes: only a #define came into runtime/host_time.h); w00a from gen44/wpl with -DNAT_IN_TRACE (so the natives, V44's
# stub and skip among them, run in a trace build, in both arms alike: gl43's read hash had them off); boot_rt from t44's boot_x86.c; the ge
# objects from build_gl44/tw.  Into build_gl44/rh/x86; nice 19.
set -e
M=/home/claude/main; MG=/mnt/project-files/mgs2; T=/home/claude/t44; X=$M/build_gl44/rh/x86; O=$M/build_gl40/x19/x86; G=$M/gen; GV=$T/gen41/gen_vu.h
H43=$M/build_gl43/rh/x86; RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; nice -n 19 gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE"
for o in gen_nat.o gen_dispatch.o gen_s_init.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_n_title.o gen_s_d00t.o gen_s_d01t.o gen_s_w01a.o; do cp $H43/$o .; done
for f in $M/build_gl44/tw/x86/ge_*.o $M/build_gl44/tw/x86/xlive_chk.o; do cp $f .; done
cu gen_s_w00a.o $CF -DNAT_IN_TRACE "-DSTAGE_GEN=\"$M/gen44/wpl/gen_s_w00a.h\"" -c $T/boot/gen_stage.c &
cu boot_rt.o $CF -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$GV\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c &
wait
nice -n 19 gcc -o boot boot_rt.o gen_nat.o gen_dispatch.o gen_s_init.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_n_title.o gen_s_d00t.o gen_s_d01t.o gen_s_w00a.o gen_s_w01a.o \
  $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_*.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log; md5sum *.o > objs.md5
