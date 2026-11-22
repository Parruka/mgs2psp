#!/bin/sh
# tw31.sh [folder] (gl31 = gl30 + native step 1; build_gl30n/tw30.sh on /home/claude/t31): the x86 twin of /home/claude/t31 in build_gl31/<folder, default tw>/x86, as build_gl29/tw29.sh
# builds gl29's (gl19's game, IOP and VU objects read in place from build_gl19/x86), with:
#  - boot_rt.o from t31/boot/boot_x86.c (scripts/x86.sh twin's line, -DNAT_HOOK): nat_stub.h (the stub, shared with the PSP's boot.c),
#    nat_x86.c (the twin walk's merge and checks), runtime/ee_io.h's hooks;
#  - gen.o: gl19's object with P1214a0 made weak and P1214a0_orig added at its address (objcopy; not compiled again): the wrapper in
#    nat_stub.h is the P1214a0 every caller reaches, as on the PSP;
#  - every ge/ object from t31/ge (x86.sh's GF line), ge_xlme.o too (the PSP's late walk, for MGS2_NAT_LW);
#  - xlive_chk.o from t31/boot/xlive_chk.c (build_gl29/m29's + gl30's sub: + step 1's natb lines and late-walk harness, merged by diff3), -DNAT_HOOK.
set -e
M=/home/claude/main; MG=/mnt/project-files/mgs2; T=/home/claude/t31; X=$M/build_gl31/${1:-tw}/x86; O=$M/build_gl19/x86; G=$M/gen
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
cu boot_rt.o $CF -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c &
for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model xlme; do
  cu ge_$f.o $GF -c $T/ge/$f.c & done; wait
cu ge_gsup.o $GF -c $T/ge/gsup2.c
cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT -DNAT_HOOK -c $T/boot/xlive_chk.c
a=$(nm $O/gen.o | awk '$3 == "P1214a0" && $2 == "T" { print $1 }'); [ -n "$a" ] || { echo "tw31.sh: no P1214a0 in $O/gen.o"; exit 1; }
objcopy --add-symbol P1214a0_orig=.text:0x$a,global,function --weaken-symbol P1214a0 $O/gen.o gen_nat.o
echo "gen_nat.o: $(nm gen_nat.o | grep 'P1214a0' | tr '\n' ';')"
gcc -o boot boot_rt.o gen_nat.o $(for u in $UNITS; do printf "$O/%s.o " $u; done) $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_xlive.o \
  ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o \
  ge_c1model.o ge_c2model.o ge_c3model.o ge_xlme.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log; grep -l "warning\|error" cc_*.log | head
