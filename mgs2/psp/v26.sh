#!/bin/sh
# v26.sh <step> [<work dir, default /home/claude/v26>]: the timing build MGS2T_v26 (2026-10-23; psp_harness.md "Build v26").  New against
# v24: a stage's calls through a register go through a sorted table and a 256-entry cache (tools/stage_tab.py; the harness's OPT_STAGE_TAB
# bit, runtime/host_time.h) instead of the PGO-ordered switch (speed_notes §43); free play's functions are in the program itself (v25f's
# translation of the ELF, w00a and d01t); profile and code order come from three routes: Otacon's deck tutorial (deck), the crew deck
# (w01a) and v25f's free play on the deck (free).  psp/main_psp.c as it was for v26 is kept in psp/v26/main_psp.c (its PGO part registers
# the instrumented overlay at vblank A, as the free route loads w00a, d01t, then w00a again).
# Steps, in order (each after the one before; the emulator runs are long):
#   hdr    the headers WP: tools/stage_tab.py over v25f's gen_s_w00a.h / gen_s_d01t.h and v24's (vglz) other stages; v25f's gen_elf.h
#   ga     PGO=gen with the main ELF instrumented (PGO_ELF=1), the overlays plain; v24's build flags (genf.txt)
#   pga    profile runs of ga on deck, w01a, free (tools/dev/pgo_run.sh; libgcov merges the three in ms0:/pgo) -> pga_ms0
#   gb     ga's ELF objects rebuilt plain, w00a instrumented (tools/dev/pgo_stagegen.sh)
#   gc     gb with ga's plain gen_s_w00a.o, w01a instrumented
#   pgbc   profile runs: gb on deck and free -> pgb_ms0, gc on w01a -> pgc_ms0 (counts zeroed at each route's first timed vblank)
#   c      PGO=use with those profiles and v24's code order (tools/dev/rebuild_use.sh)
#   trace  instruction traces of c, 8 windows of 5 steps per route (tools/dev/trace_run.sh IT=1) -> lay/
#   order  tools/layout_sim.c over deck + free (one trace, w00a) and w01a, two seeds (lay/order_s<seed>.txt)
#   d      c relinked in the kept order (SEED, default 1; tools/dev/relink_order.sh), checked with layout_sim -new
#   emu    d in PPSSPP on the three routes (exact check: every hash against the x86 references)
# Container inputs (ephemeral): v24/d (genf.txt, layouts), v25f/W25, vglz/W, psp/vp22, the packs psp5 and v25f/pak, the pads.
S=$1; V=${2:-/home/claude/v26}; MG=/mnt/project-files/mgs2; T=$MG/tools
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a"
cd $V || exit 1
case $S in
hdr)
  mkdir -p W WP
  ln -sf /home/claude/v25f/W25/gen_elf.h W/gen_elf.h; ln -sf /home/claude/vgz/W/stages.h W/stages.h
  for st in w00a d01t; do python3 $T/stage_tab.py /home/claude/v25f/W25/gen_s_$st.h W/gen_s_$st.h || exit 1; done
  for st in d00t init n_title r_title r_tnk0 w01a; do python3 $T/stage_tab.py /home/claude/vglz/W/gen_s_$st.h WP/gen_s_$st.h || exit 1; done
  ln -sf /home/claude/v25f/W25/gen_elf.h WP/gen_elf.h; ln -sf /home/claude/vglz/W/stages.h WP/stages.h; ln -sf /home/claude/vglz/W/cov_u.txt WP/cov_u.txt
  for st in w00a d01t; do ln -sf $V/W/gen_s_$st.h WP/gen_s_$st.h; done ;;
ga)
  mkdir -p ga && cp /home/claude/v24/d/genf.txt ga/
  RTXX="-Wno-error=coverage-mismatch -DVP_VFPU" VP=/home/claude/psp/vp22 J=4 sh $T/dev/mk_psp.sh $V/ga $V/WP PGO=gen PGO_ELF=1 PGO_STAGE= LAYOUT=1 \
    STAGES="$STAGES" PSP_EBOOT_TITLE="MGS2 timing harness v26 pgo elf" > ga_build.out 2>&1 ;;
pga)
  sh $T/dev/pgo_run.sh $V/ga $V/pga deck w01a free > pga.out 2>&1
  mkdir -p pga_ms0 && mv /home/claude/ms_home/.ppsspp/pgo/boot_rt.gcda /home/claude/ms_home/.ppsspp/pgo/gen.gcda /home/claude/ms_home/.ppsspp/pgo/gen_dispatch.gcda pga_ms0/ ;;
gb)
  mkdir -p keep && cp -a ga/gen_s_w00a.o ga/gen_s_w01a.o keep/
  W=$V/WP sh $T/dev/pgo_stagegen.sh $V/ga $V/gb w00a gen.o gen_dispatch.o boot_rt.o main_psp.o gen_s_w00a.o > gb_build.out 2>&1 ;;
gc)
  rm -rf gcpre && cp -a gb gcpre && cp -a keep/gen_s_w00a.o gcpre/gen_s_w00a.o
  W=$V/WP sh $T/dev/pgo_stagegen.sh $V/gcpre $V/gc w01a main_psp.o gen_s_w01a.o > gc_build.out 2>&1 ;;
pgbc)
  sh $T/dev/pgo_run.sh $V/gb $V/pgb deck free; mkdir -p pgb_ms0; mv /home/claude/ms_home/.ppsspp/pgo/*.gcda pgb_ms0/
  sh $T/dev/pgo_run.sh $V/gc $V/pgc w01a; mkdir -p pgc_ms0; mv /home/claude/ms_home/.ppsspp/pgo/*.gcda pgc_ms0/ ;;
c)
  rm -rf c && cp -a ga c && cd c && rm -f gen.o gen_dispatch.o boot_rt.o main_psp.o gen_s_w00a.o gen_s_w01a.o *.gcda
  cp ../pga_ms0/gen.gcda ../pga_ms0/gen_dispatch.gcda ../pga_ms0/boot_rt.gcda ../pgb_ms0/gen_s_w00a.gcda ../pgc_ms0/gen_s_w01a.gcda .
  cp /home/claude/v24/d/layout_elf.ld /home/claude/v24/d/layout_w00a.ld /home/claude/v24/d/layout_w01a.ld .
  cd $V && W=$V/WP VP=/home/claude/psp/vp22 TITLE="MGS2 timing harness v26" sh $T/dev/rebuild_use.sh $V/c > c_build.out 2>&1 ;;
trace)
  P="prof 24001 24006\nprof 24151 24156\nprof 24301 24306\nprof 24451 24456\nprof 24601 24606\nprof 24751 24756\nprof 24901 24906\nprof 25051 25056"
  PF="prof 30801 30806\nprof 31301 31306\nprof 32201 32206\nprof 32951 32956\nprof 33401 33406\nprof 33851 33856\nprof 34801 34806\nprof 35801 35806"
  IT=1 VB=25060 sh $T/dev/trace_run.sh $V/c t26d deck $V/lay "$P" 8 &
  IT=1 VB=25060 sh $T/dev/trace_run.sh $V/c t26w w01a $V/lay "$P" 8 &
  IT=1 VB=35810 MAXS=5000 sh $T/dev/trace_run.sh $V/c t26f free $V/lay "$PF" 8 &
  wait ;;
order)
  cd lay || exit 1
  python3 $T/layout_funcs.py ../c/MGS2T.elf ../c/ovl_w00a.elf ../c w00a > funcs_deck.txt
  python3 $T/layout_funcs.py ../c/MGS2T.elf ../c/ovl_w01a.elf ../c w01a > funcs_w01a.txt
  # one w00a trace: deck windows 1-7, free 1-7, deck 8, free 8 (-test 2 holds out the last of each route)
  python3 -c "
import numpy as np
def wins(p):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == 9; return [a[m[k]:m[k + 1]] for k in range(8)]
d, f = wins('t26d.it'), wins('t26f.it')
np.concatenate(d[:7] + f[:7] + [d[7], f[7]]).tofile('t_df.it')"
  B1=$(printf %08x $((0x$(psp-nm ../c/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))   # the overlay code after its header
  for s in 1 2; do
    /home/claude/v17/layout_sim t_df.it funcs_deck.txt -and t26w.it funcs_w01a.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -opt 40000 -j 2 \
      -seed $s -out order_s$s.txt > ls_s$s.out 2>&1 &
  done
  wait ;;
d)
  rm -rf d && cp -a c d
  W=$V/WP VP=/home/claude/psp/vp22 TITLE="MGS2 timing harness v26" sh $T/dev/relink_order.sh $V/lay/order_s${SEED:-1}.txt $V/d > d_build.out 2>&1
  cd lay && python3 $T/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w00a.elf ../d w00a > funcs_deck_d.txt && python3 $T/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w01a.elf ../d w01a > funcs_w01a_d.txt
  # the relinked addresses must give the search's own counts, route by route (-new takes one trace)
  B1=$(printf %08x $((0x$(psp-nm ../d/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  /home/claude/v17/layout_sim t_df.it funcs_deck.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -new funcs_deck_d.txt > new_dd.log 2>&1
  /home/claude/v17/layout_sim t26w.it funcs_w01a.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -new funcs_w01a_d.txt > new_dw.log 2>&1 ;;
emu)   # d in PPSSPP (JIT, PPSSPP_VFPU_RN=1) on the three routes at once, as tools/dev/exact_run.sh: deck and w01a with v24's cfgs to vb 25195
       # against the v21 x86 references, free with the delivery's cfg to vb 36495 against v25f's; logs and steps in emu/
  GG=/home/claude/ms_home/.ppsspp/PSP/GAME; mkdir -p emu; export PPSSPP_VFPU_RN=1
  F=$(psp-nm d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  for r in deck w01a free; do
    G=$GG/MGS2V26_$r; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/split.* $G/battery.txt
    cp d/EBOOT.PBP d/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $G/
    if [ $r = free ]; then cp /home/claude/v25f/dl/pad.txt $G/; ln -sf /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak
      cp dl_draft/mgs2.cfg $G/mgs2.cfg; echo 'prof 36495 36496' >> $G/mgs2.cfg; cp $MG/prof/hashref_x86_free_v25f.txt $G/hashref.txt
    else cp $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak
      cp $MG/prof/emu_v24/mgs2_$r.cfg $G/mgs2.cfg; echo 'prof 25195 25196' >> $G/mgs2.cfg; cp $MG/prof/hashref_x86_${r}_v21.txt $G/hashref.txt; fi
    (sh $T/dev/prof_run.sh $G 0x$F $V/emu/x_$r ${MAXS:-9000} > emu/run_$r.out 2>&1; cp $G/log.txt emu/log_$r.txt; cp $G/steps.csv emu/steps_$r.csv
     ls $G/split.* > emu/splitfiles_$r.txt 2>/dev/null; echo done > emu/done_$r) &
  done
  wait ;;
*) echo "v26.sh: step hdr | ga | pga | gb | gc | pgbc | c | trace | order | d | emu"; exit 1 ;;
esac
