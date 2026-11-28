#!/bin/sh
# gl45 batch 3: an x86 twin from a pair of headers.  usage: buildx.sh <tag> <gen_elf.h> <gen_s_w00a.h> [extra objects/flags for the link ...]
# -> build/x86/boot_<tag> (gen_<tag>.o, gen_s_w00a_<tag>.o; every other object is build/x86's as it is)
M=/home/claude/main; . $M/scripts/env.sh
t=$1; he=$2; hs=$3; shift 3
X=$B/x86; cd $X || exit 1
CF="-O1 -g0 -w -I$M/runtime -I$M/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 $XCF"
UNITS="gen_dispatch gen_s_init gen_s_r_title gen_s_r_tnk0 gen_s_n_title gen_s_d00t gen_s_d01t gen_s_w01a"
IOPO="iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o"
GE="xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o"
s=$(date +%s)
( gcc $CF -DGEN_LITE "-DGEN=\"$he\"" -c $M/boot/gen.c -o gen_$t.o > cc_gen_$t.log 2>&1 || echo "gen_$t FAILED" ) &
( gcc $CF "-DSTAGE_GEN=\"$hs\"" -c $M/boot/gen_stage.c -o gen_s_w00a_$t.o > cc_gen_s_w00a_$t.log 2>&1 || echo "gen_s_w00a_$t FAILED" ) &
wait
gcc -o boot_$t boot_rt.o gen_$t.o gen_s_w00a_$t.o $(for u in $UNITS; do printf '%s.o ' $u; done) $IOPO $GE "$@" -lm > link_$t.log 2>&1 || { echo "link_$t FAILED"; head link_$t.log; head -20 cc_gen_$t.log cc_gen_s_w00a_$t.log; exit 1; }
echo "boot_$t built $(( $(date +%s) - s )) s"
