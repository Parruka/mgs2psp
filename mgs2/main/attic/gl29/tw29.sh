#!/bin/sh
# tw29.sh (gl29, 2026-11-20): the x86 twin of gl29 in build_gl29/x86, as scripts/x86.sh twin builds it, with gl19's game, IOP and
# runtime objects as they are (build_gl19/x86: boot_rt.o, gen*.o, the IOP objects; read in place), every ge/ object compiled from
# /home/claude/t29/ge with x86.sh's GF line, and xlive_chk.o from the main copy build_gl29/m29/boot/xlive_chk.c (MGS2_GL_DZ).
# TW_T, TW_X, TW_C: another tree, build folder and xlive_chk.c (the gl28 twin for E1's control: t28, x86_t28, main/boot/xlive_chk.c).
set -e
M=/home/claude/main; X=${TW_X:-$M/build_gl29/x86}; O=$M/build_gl19/x86; T=${TW_T:-/home/claude/t29}; C=${TW_C:-$M/build_gl29/m29/boot/xlive_chk.c}
mkdir -p $X
cd $X
UNITS="gen gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
cu() { o=$1; shift; s=$(date +%s); { echo "cmd: gcc $*"; gcc "$@" -o $o; } > cc_$o.log 2>&1; r=$?; echo "$o rc $r $(( $(date +%s) - s )) s" >> cc_$o.log; tail -n 1 cc_$o.log; return $r; }
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
for f in xlive xlate gsfront xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model; do
  cu ge_$f.o $GF -c $T/ge/$f.c & done; wait
cu ge_gsup.o $GF -c $T/ge/gsup2.c
cu xlive_chk.o -O2 -g0 -Wall -Wextra -DGU_SENT -c $C
gcc -o boot $O/boot_rt.o $(for u in $UNITS; do printf "$O/%s.o " $u; done) $(for u in $IOPO; do printf "$O/%s " $u; done) xlive_chk.o ge_xlive.o \
  ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o \
  ge_c1model.o ge_c2model.o ge_c3model.o -lm > link.log 2>&1
echo "boot link rc $?"; tail -n 20 link.log
