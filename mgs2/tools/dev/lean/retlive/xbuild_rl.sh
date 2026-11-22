#!/bin/sh
# xbuild_rl.sh <header dir> <out dir> <boot_rt object> (lean_plan 1c, dev): v26's x86 build (v26/x86's objects, the given runtime object)
# with the header dir's gen.o and stage objects, plus cg_wrap.o, the guard against a call through a register reaching a closed function
# (closed lists from <header dir>/ret_*.json, mk_cg.py), linked with --wrap.  JSIF and RC_RETLIVE=poison's RETPOISON come from boot/gen_pre.h.
# Needs headers made before regcache.py dropped the closed functions' dispatch cases (the project form, which makes this guard redundant).
MG=/mnt/project-files/mgs2; W=$1; O=$2; RT=$3; X=/home/claude/v26/x86; L=${L:-$(cd $(dirname $0) && pwd)}
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
PI=""   # (JSIF and RETPOISON now come from the project's boot/gen_pre.h)
mkdir -p $O && cd $O || exit 1
cmp -s $W/stages.h $X/../W/stages.h || { echo "stages.h differs from v26's"; exit 1; }
s=$(date +%s); : > cc.status
for o in iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o gen_dispatch.o; do cp $X/$o .; done; cp $RT boot_rt.o
mkdir -p memcard0
python3 $L/mk_cg.py $W $O/cg_closed.h > cg.log 2>&1 || { echo "mk_cg failed"; exit 1; }
gcc $CF "-DSTAGES=\"$X/../W/stages.h\"" "-DCG_CLOSED=\"$O/cg_closed.h\"" -c $L/cg_wrap.c -o cg_wrap.o > cc_cg.log 2>&1 || { echo "cg_wrap failed"; cat cc_cg.log; exit 1; }
cst() { gcc $CF $PI "-DSTAGE_GEN=\"$W/gen_s_$1.h\"" -c $MG/boot/gen_stage.c -o gen_s_$1.o > cc_$1.log 2>&1; echo "$1 $? $(( $(date +%s) - s )) s" >> cc.status; }
( gcc $CF $PI -DGEN_LITE "-DGEN=\"$W/gen_elf.h\"" -c $MG/boot/gen.c -o gen.o > cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( cst w00a; cst r_title; cst r_tnk0 ) &
( cst d01t; cst init ) &
( cst d00t; cst n_title; cst w01a ) &
wait
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o cg_wrap.o -Wl,--wrap=gen_jalr,--wrap=gen_guest_call -lm > link.log 2>&1; echo "link $? $(( $(date +%s) - s )) s" >> cc.status
grep -v " 0 " cc.status | grep -v "^link 0" ; echo done >> cc.status
