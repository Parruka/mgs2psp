#!/bin/sh
# mk.sh: me12 (task #142, the ME hand-off design) x86 tracker build in /home/claude/me12/xd: me10/me11's late-run harness with the census
# (boot/xlive_chk.c) + MGS2_XLIVE_WT, the writer tracker.  The game code (v31's headers, /home/claude/v31/WP, as tools/dev/x86_ref.sh builds
# them) with GEN_DG_TRACE: every store reports itself (rt_dgt_wr / rt_dgt_wrh, defined in xlive_chk.c; no native replacements, no idioms);
# the runtime (sb9's boot.c) with WT_TRACK and runtime/ee_io.h from here: its scratchpad DMA copies report too (rt_dgt_copy); the GE side
# (sb9/x1's objects) unchanged.  The game's state must be the same as the normal build's (checked with the hash lines).
MG=/mnt/project-files/mgs2; W=/home/claude/v31/WP; R=/home/claude/v31/x86; T=/home/claude/sb9; VP=/home/claude/psp/vp22; M=/home/claude/me12
O=$M/xd; mkdir -p $O/g && cd $O || exit 1
CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_DG_TRACE"
s=$(date +%s); : > cc.status
cst() { nice gcc $CF "-DSTAGE_GEN=\"$W/gen_s_$1.h\"" -c $MG/boot/gen_stage.c -o g/gen_s_$1.o > g/cc_$1.log 2>&1; echo "$1 $? $(( $(date +%s) - s )) s" >> cc.status; }
( nice gcc $CF -DGEN_LITE "-DGEN=\"$W/gen_elf.h\"" -c $MG/boot/gen.c -o g/gen.o > g/cc_gen.log 2>&1; echo "gen $? $(( $(date +%s) - s )) s" >> cc.status ) &
( nice gcc $CF "-DSTAGES=\"$W/stages.h\"" -c $MG/boot/gen_dispatch.c -o g/gen_dispatch.o > g/cc_disp.log 2>&1; echo "dispatch $? $(( $(date +%s) - s )) s" >> cc.status
  nice gcc -O1 -g0 -w -I$M/runtime -I$T/runtime -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DWT_TRACK -DMGS2_XLIVE \
    "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT \
    -c $T/boot/boot.c -o g/boot_rt.o > g/cc_rt.log 2>&1; echo "rt $? $(( $(date +%s) - s )) s" >> cc.status ) &
( for u in d00t d01t init; do cst $u; done ) &
( for u in n_title r_title r_tnk0 w00a w01a; do cst $u; done ) &
wait
for o in iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do ln -sf $R/$o.o $o.o; done
for f in xlive xlate gsfront gsup xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin; do ln -sf $T/x1/ge_$f.o ge_$f.o; done
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -I$T/boot -c $M/boot/xlive_chk.c -o xlive_chk.o > cc_chk.log 2>&1; echo "chk $? $(( $(date +%s) - s )) s" >> cc.status
gcc -o boot g/boot_rt.o g/gen.o g/gen_dispatch.o g/gen_s_d00t.o g/gen_s_d01t.o g/gen_s_init.o g/gen_s_n_title.o g/gen_s_r_title.o g/gen_s_r_tnk0.o \
  g/gen_s_w00a.o g/gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o \
  ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o -lm > link.log 2>&1
echo "link $? $(( $(date +%s) - s )) s" >> cc.status; cat cc.status
