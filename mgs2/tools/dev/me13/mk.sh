#!/bin/sh
# mk.sh [NORT=1 keeps boot_rt.o]: me13 (task #147, the ME hand-off's build order step 1a) x86 build in /home/claude/me13/xd: sb14's twin (its GE objects, x1) with
# the game code built with GEN_DG_TRACE (me12's objects: every store reports itself, rt_dgt_wr / rt_dgt_wrh), sb14's runtime (boot.c) built
# with WT_TRACK and ME_GUARD_RT and me13's runtime headers (ee_io.h: the scratchpad DMA to RAM; sif.h, iop_host.h: the IOP-to-EE copies and
# the disc reads report before they write, rt_me_guard), and boot/xlive_chk.c = sb14's + me12.diff + tools/patch_me13.py (MGS2_XLIVE_DL,
# MGS2_XLIVE_G0; me11 b's MGS2_XLIVE_SNAPP, MGS2_XLIVE_SNAPR).  Then g0.txt: the G0 functions' code ranges in this binary (nm -S).
MG=/mnt/project-files/mgs2; R=/home/claude/v31/x86; T=/home/claude/sb14; VP=/home/claude/psp/vp22; M=/home/claude/me13; G=/home/claude/me12/xd/g
O=$M/xd; mkdir -p $O && cd $O || exit 1
s=$(date +%s); : > cc.status
for o in gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w00a gen_s_w01a; do ln -sf $G/$o.o $o.o; done
for o in iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do ln -sf $R/$o.o $o.o; done
for f in xlive xlate gsfront gsup xldig texconv texcache gelive gebatch xconv vu1conv gelist fxthin c1model c2model c3model; do ln -sf $T/x1/ge_$f.o ge_$f.o; done
[ -n "$NORT" ] && [ -f boot_rt.o ] && echo "rt kept (NORT)" >> cc.status || nice gcc -O1 -g0 -w -I$M/runtime -I$T/runtime -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0 -DWT_TRACK -DME_GUARD_RT -DMGS2_XLIVE \
  "-DMGS2_GEN_VU=\"$VP/gen_vu.h\"" -DVP_FAST -DVP_VFPU -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$MG/third_party/ps2rc/audio -DGUEST_SPLIT_RT \
  -c $T/boot/boot.c -o boot_rt.o > cc_rt.log 2>&1 && echo "rt $? $(( $(date +%s) - s )) s" >> cc.status
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -c $M/boot/xlive_chk.c -o xlive_chk.o > cc_chk.log 2>&1; echo "chk $? $(( $(date +%s) - s )) s" >> cc.status
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o \
  iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o \
  ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o -lm > link.log 2>&1
echo "link $? $(( $(date +%s) - s )) s" >> cc.status
# the G0 functions (me_handoff_design.md section 2): offset from __executable_start, size, name
nm -S boot > nm.txt
python3 -I -c '
import sys
G0 = "S_d00t_P299fb8 P11de00 S_d01t_P2a1200 S_w00a_P31c0d0 S_w00a_P3a4138 S_w00a_P11e0a8 P16d1ac S_w01a_P30c8b8 S_w01a_P322198 S_w01a_P30d6f8 S_w01a_P350f40 S_w01a_P3926e0".split()
sym = {}
for l in open("nm.txt"):
    f = l.split()
    if len(f) == 4: sym[f[3]] = (int(f[0], 16), int(f[1], 16))
    elif len(f) == 3: sym[f[2]] = (int(f[0], 16), 0)
b = sym["__executable_start"][0]
for g in G0: a, n = sym[g]; print("%x %x %s" % (a - b, n, g))
' > g0.txt
echo "g0 $(wc -l < g0.txt) functions" >> cc.status; cat cc.status
