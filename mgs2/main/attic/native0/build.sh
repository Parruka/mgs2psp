#!/bin/sh
# build.sh <outdir> [extra cflags...]: the x86 twin from /home/claude/n0 (boot, runtime, ge copies); stage and IOP objects read from main/build/x86
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/n0; N=/home/claude/n0; M=/home/claude/main; G=$M/gen
MG=/mnt/project-files/mgs2; RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio; O=$M/build/x86
X=$1; shift; mkdir -p $X; cd $X || exit 1
CF="-O1 -g0 -w -I$N/runtime -I$N/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 $*"
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS $*"
cu() { o=$1; shift; s=$(date +%s); gcc "$@" -o $o > cc_$o.log 2>&1; echo "$o rc $? $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; }
cu gen.o $CF -DGEN_LITE "-DGEN=\"$G/wh/gen_elf.h\"" -c $N/boot/gen.c &
cu gen_dispatch.o $CF "-DSTAGES=\"$G/WP/stages.h\"" -c $N/boot/gen_dispatch.c &
cu boot_rt.o $CF -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $N/boot/boot_x86.c &
GEO=""
for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model $NAT_GE; do
  cu ge_$f.o $GF -c $N/ge/$f.c & GEO="$GEO ge_$f.o"; done
cu ge_gsup.o $GF -c $N/ge/gsup2.c &
cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT $* -c $N/boot/xlive_chk.c &
wait
U="gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
gcc -o boot boot_rt.o gen.o gen_dispatch.o $(for u in $U; do printf '%s/%s.o ' $O $u; done) $(for u in $IOPO; do printf '%s/%s ' $O $u; done) \
  xlive_chk.o $GEO ge_gsup.o -lm > link.log 2>&1
echo "link rc $?"; tail -5 link.log; grep -l "warning\|error" cc_*.log | head
