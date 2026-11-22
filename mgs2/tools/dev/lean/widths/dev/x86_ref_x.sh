#!/bin/sh
# x86_ref.sh <header dir> <out dir>: the x86 reference build of a header set from tools/mk_headers.sh (the build the PSP harness's hashes
# are checked against): boot/Makefile.probe's flags (GEN_MM_FAST GUEST_SPR0), gen.c with GEN_LITE, gen_dispatch.c with the set's stages.h,
# and the runtime unit from boot/boot.c with the AOT VU0 programs and their VFPU forms (VP_FAST VP_VFPU) as in v26's x86 check
# (psp/v26.sh; v26/x86/build.sh).  Inputs (container, ephemeral): VP (default /home/claude/psp/vp22, its gen_vu.h) and IOPX (default
# /home/claude/v23/xg3: the IOP objects, unchanged since v23).  The runtime is the project's own (runtime/kernel.h: the cleared handler
# start of lean_plan.md 1b), so the hashes are those of v27 on (prof/hashes_x86_*_v27.txt).  Then, per route (d: the deck tutorial,
# w: the crew deck, f: free play):  sh tools/dev/lean/runh.sh <out dir>/R<r> H<r> <r>
MG=/mnt/project-files/mgs2; W=$1; O=$2; VP=${VP:-/home/claude/psp/vp22}; X=${IOPX:-/home/claude/v23/xg3}
CF="$XCF -O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
[ -n "$W" ] && [ -n "$O" ] && [ -f $W/gen_elf.h ] && [ -f $W/stages.h ] || { echo "usage: x86_ref.sh <header dir> <out dir>"; exit 1; }
W=$(cd $W && pwd); mkdir -p $O/memcard0 && cd $O || exit 1
s=$(date +%s); : > cc.status
for o in iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o; do cp $X/$o . || exit 1; done
ST=$(grep -o 'gen_s_[a-z0-9_]*\.h' $W/stages.h | sort -u | sed 's/^gen_s_//; s/\.h$//')
cst() { gcc $CF "-DSTAGE_GEN=\"$W/gen_s_$1.h\"" -c $MG/boot/gen_stage.c -o gen_s_$1.o > cc_$1.log 2>&1; echo "$1 $? $(( $(date +%s) - s )) s" >> cc.status; }
( gcc $CF -DGEN_LITE "-DGEN=\"$W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( gcc $CF "-DSTAGES=\"$W/stages.h\"" -c $MG/boot/gen_dispatch.c -o gen_dispatch.o > cc_disp.log 2>&1; echo "dispatch $? $(( $(date +%s) - s )) s" >> cc.status
  gcc $CF "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT \
    -c $MG/boot/boot.c -o boot_rt.o > cc_rt.log 2>&1; echo "rt $? $(( $(date +%s) - s )) s" >> cc.status ) &
i=0; g0=""; g1=""; g2=""
for u in $ST; do case $((i % 3)) in 0) g0="$g0 $u" ;; 1) g1="$g1 $u" ;; *) g2="$g2 $u" ;; esac; i=$((i + 1)); done
( for u in $g0; do cst $u; done ) &
( for u in $g1; do cst $u; done ) &
( for u in $g2; do cst $u; done ) &
wait
gcc -o boot boot_rt.o gen.o gen_dispatch.o $(for u in $ST; do printf 'gen_s_%s.o ' $u; done) iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o \
  jsifman.o cdbios.o spu2_lite.o -lm > link.log 2>&1; echo "link $? $(( $(date +%s) - s )) s" >> cc.status
grep -v " 0 " cc.status; echo done >> cc.status
