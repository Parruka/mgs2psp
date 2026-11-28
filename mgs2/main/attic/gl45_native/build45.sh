#!/bin/sh
# gl45 study: the twin with the hooked ELF unit (gen/w45/gen_elf.h): boot45 (plain), boot45c (-DN45_CHECK), boot45t (-DN45_TICKS).
# usage: build45.sh [plain] [check] [ticks]   (default: all three); the other objects are build/x86's as they are
M=/home/claude/main; . $M/scripts/env.sh
X=$B/x86; cd $X || exit 1
CF="-O1 -g0 -w -I$M/runtime -I$M/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w00a gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
GE="xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o"
one() {  # <suffix> <extra flags>
  s=$(date +%s)
  gcc $CF -DGEN_LITE $2 "-DGEN=\"$G/w45/gen_elf.h\"" -c $M/boot/gen.c -o gen45$1.o > cc_gen45$1.log 2>&1 || { echo "gen45$1 FAILED"; head -30 cc_gen45$1.log; return 1; }
  gcc -o boot45$1 boot_rt.o gen45$1.o $(for u in $UNITS; do printf '%s.o ' $u; done) $IOPO $GE -lm > link45$1.log 2>&1 || { echo "link45$1 FAILED"; head link45$1.log; return 1; }
  echo "boot45$1 built $(( $(date +%s) - s )) s"
}
[ $# = 0 ] && set -- plain check ticks
for w in "$@"; do case $w in plain) one "" "" & ;; check) one c -DN45_CHECK & ;; ticks) one t -DN45_TICKS & ;; esac; done
wait
