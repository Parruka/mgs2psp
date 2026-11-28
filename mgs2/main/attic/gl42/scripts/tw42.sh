#!/bin/sh
# tw42.sh [folder] (gl42: build_gl41/tw41.sh with T=t42 (gl41's final source + native step 2), build_gl42, and gen.o's P12e8b8 (the people's
# builder) and P12e790 (its template's set-up) made weak with <name>_orig added at their addresses, beside P1214a0 (boot/np6_stub.h's
# wrappers are the ones every caller reaches, as on the PSP); one compile at a time, nice 19 (gl41 has the CPU first).
# gl41's notes: the x86 twin of the tree in build_gl42/<folder, default tw>/x86, the gl19 x86 units read from build_gl40/x19.
set -e
M=/home/claude/main; MG=/mnt/project-files/mgs2; T=/home/claude/t42; X=$M/build_gl42/${1:-tw}/x86; O=$M/build_gl40/x19/x86; G=$M/gen; GV=$T/gen41/gen_vu.h
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; nice -n 19 gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
for f in ${ONLY:-xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model xlme gsup chk rt}; do
  case $f in
  gsup) cu ge_gsup.o $GF -c $T/ge/gsup2.c ;;
  chk) cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT -DNAT_HOOK -c $T/boot/xlive_chk.c ;;
  rt) cu boot_rt.o $CF -DNAT_HOOK -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$GV\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c ;;
  *) cu ge_$f.o $GF -c $T/ge/$f.c ;;
  esac; done
# the three builders behind their wrappers (boot/nat_stub.h, boot/np6_stub.h)
OC=; for f in P1214a0 P12e8b8 P12e790; do
  a=$(nm $O/gen.o | awk -v f=$f '$3 == f && $2 == "T" { print $1 }'); [ -n "$a" ] || { echo "tw42.sh: no $f in $O/gen.o"; exit 1; }
  OC="$OC --add-symbol ${f}_orig=.text:0x$a,global,function --weaken-symbol $f"; done
objcopy $OC $O/gen.o gen_nat.o
echo "gen_nat.o: $(nm gen_nat.o | grep 'P1214a0\|P12e8b8\|P12e790' | tr '\n' ';')"
nice -n 19 gcc -o boot boot_rt.o gen_nat.o $(for u in $UNITS; do printf "$O/%s.o " $u; done) $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_xlive.o \
  ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o \
  ge_c1model.o ge_c2model.o ge_c3model.o ge_xlme.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log; grep -l "warning\|error" cc_*.log | head
