#!/bin/sh
# x86_gi.sh <header dir> <out dir>: tools/dev/x86_ref.sh's x86 build with guest-instruction counting (boot.c GEN_GI_COUNT, MGS2_GI):
# every generated header rewritten by tools/dev/gi_count.py into <out dir>/W, compiled with -DGEN_GI_COUNT -include gi.h.
MG=/mnt/project-files/mgs2; W=$1; O=$2; VP=${VP:-/home/claude/psp/vp22}; X=${IOPX:-/home/claude/v23/xg3}; GH=${GH:-$MG/tools/dev/lean/gi.h}
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_GI_COUNT"
[ -n "$W" ] && [ -n "$O" ] && [ -f $W/gen_elf.h ] && [ -f $W/stages.h ] || { echo "usage: x86_gi.sh <header dir> <out dir>"; exit 1; }
W=$(cd $W && pwd); mkdir -p $O/memcard0 $O/W && cd $O || exit 1
cp $GH gi.h; GI="-include $O/gi.h"
s=$(date +%s); : > cc.status
for o in iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o; do cp $X/$o . || exit 1; done
ST=$(grep -o 'gen_s_[a-z0-9_]*\.h' $W/stages.h | sort -u | sed 's/^gen_s_//; s/\.h$//')
cp $W/stages.h W/
for h in gen_elf $(for u in $ST; do printf 'gen_s_%s ' $u; done); do python3 -I $MG/tools/dev/gi_count.py $W/$h.h W/$h.h 2>> gi_count.log || exit 1; done
echo "headers $(( $(date +%s) - s )) s" >> cc.status
cst() { gcc $CF $GI "-DSTAGE_GEN=\"$O/W/gen_s_$1.h\"" -c $MG/boot/gen_stage.c -o gen_s_$1.o > cc_$1.log 2>&1; echo "$1 $? $(( $(date +%s) - s )) s" >> cc.status; }
( gcc $CF $GI -DGEN_LITE "-DGEN=\"$O/W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( gcc $CF "-DSTAGES=\"$O/W/stages.h\"" -c $MG/boot/gen_dispatch.c -o gen_dispatch.o > cc_disp.log 2>&1; echo "dispatch $? $(( $(date +%s) - s )) s" >> cc.status
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
