#!/bin/sh
# mk.sh: the gl39 x86 twin (main/build_gl39/tw39.sh's units and flags) from st0/t (a copy of t39 with the st0 hooks under -DNGE_TWIN)
# plus the cooker (st0/nge: nge_cook.c, nge_list.c) and its harness (nge_twin.c), in st0/bld.  gen.o: build19's, with P1214a0 and
# P12cd78 made weak and *_orig added at their addresses (objcopy).  Compiles one at a time, nice 10.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st0; T=$S/t; N=$S/nge; X=$S/bld
M=/home/claude/main; MG=/mnt/project-files/mgs2; O=$M/build19/x86; G=$M/gen
RT=$MG/third_party/ps2rc/ps2rc/iop/rt; AU=$MG/third_party/ps2rc/audio
mkdir -p $X; cd $X || exit 1
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; nice -n 10 gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
CF="-O1 -g0 -w -I$T/runtime -I$T/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS -DNGE_TWIN"
set -e
for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model xlme; do
  cu ge_$f.o $GF -c $T/ge/$f.c; done
cu ge_gsup.o $GF -c $T/ge/gsup2.c
for f in nge_cook nge_list; do cu $f.o $GF -std=gnu99 -I$N -c $N/$f.c; done
cu nge_twin.o $GF -std=gnu99 -I$N -I$T/ge -c $N/nge_twin.c
cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT -DNAT_HOOK -DNGE_TWIN -c $T/boot/xlive_chk.c
cu boot_rt.o $CF -DNAT_HOOK -DNGE_TWIN -DMGS2_XLIVE "-DMGS2_GEN_VU=\"$G/vp22/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$RT -I$AU -DGUEST_SPLIT_RT -c $T/boot/boot_x86.c
OC=; for f in P1214a0 P12cd78; do
  a=$(nm $O/gen.o | awk -v f=$f '$3 == f && $2 == "T" { print $1 }'); [ -n "$a" ] || { echo "mk.sh: no $f in $O/gen.o"; exit 1; }
  OC="$OC --add-symbol ${f}_orig=.text:0x$a,global,function --weaken-symbol $f"; done
objcopy $OC $O/gen.o gen_nat.o
echo "gen_nat.o: $(nm gen_nat.o | grep -E 'P1214a0|P12cd78' | tr '\n' ';')"
nice -n 10 gcc -o boot boot_rt.o gen_nat.o $(for u in $UNITS; do printf "$O/%s.o " $u; done) $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_xlive.o \
  ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o \
  ge_c1model.o ge_c2model.o ge_c3model.o ge_xlme.o nge_cook.o nge_list.o nge_twin.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log; grep -l "warning\|error" cc_*.log | head
