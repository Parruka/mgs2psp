#!/bin/sh
# mk.sh: native step 1's x86 twin (build_gl39/tw39.sh's units and flags) from st1/t (t39 + step 1: ge/nge.h, boot/nge_stub.h, nge/,
# and the hooks in nat_stub.h, nat_x86.c, xlme.c, xconv.c, gelist.c, gelive.c, xlive_chk.c), in st1/bld.  gen.o: build19's with
# P1214a0 made weak and P1214a0_orig added at its address (objcopy), as st0.  One compile at a time, nice 19 (gl40 builds).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1; T=$S/t; X=$S/bld
M=/home/claude/main; MG=/mnt/project-files/mgs2; O=$M/build19/x86; G=$M/gen
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X || exit 1
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; nice -n 19 gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
set -e
for f in ${ONLY:-xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model xlme gsup chk rt}; do
  case $f in
  gsup) cu ge_gsup.o $GF -c $T/ge/gsup2.c ;;
  chk) cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT -DNAT_HOOK -c $T/boot/xlive_chk.c ;;
  rt) cu boot_rt.o $CF -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c ;;
  *) cu ge_$f.o $GF -c $T/ge/$f.c ;;
  esac; done
[ -f gen_nat.o ] || { a=$(nm $O/gen.o | awk '$3 == "P1214a0" && $2 == "T" { print $1 }'); [ -n "$a" ] || { echo "mk.sh: no P1214a0"; exit 1; }
  objcopy --add-symbol P1214a0_orig=.text:0x$a,global,function --weaken-symbol P1214a0 $O/gen.o gen_nat.o; }
echo "gen_nat.o: $(nm gen_nat.o | grep P1214a0 | tr '\n' ';')"
nice -n 19 gcc -o boot boot_rt.o gen_nat.o $(for u in $UNITS; do printf "$O/%s.o " $u; done) $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_xlive.o \
  ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o \
  ge_c1model.o ge_c2model.o ge_c3model.o ge_xlme.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log; grep -l "warning\|error" cc_*.log | head
