#!/bin/sh
# v28.sh <step> [<work dir, default /home/claude/v28>]: the timing build MGS2T_v28 (2026-10-25; psp_harness.md "Build v28").  New against
# v26: the leaner generator's steps 1 and 2 (recomp/lean_plan.md): v27's headers (1b handlers start with cleared registers, 1c returns
# store only caller-readable registers, 1e cross-unit sets) with regcache.py's RC_CONDCLEAN fix and the proved register widths
# (RC_WPW = recomp/widths/<WPW>.json.gz, recomp/widths_step2.md), which also mend v27's quadword copies (free play from vb 33,531).
# Profile and code order as v26's (three routes), then the Media Engine sound mix, crash catcher 2 and battery log on top as v26me4
# (psp/v28me.sh on build d).  Steps, in order:
#   hdr    the headers WP from the disc files: tools/mk_headers.sh with v27's options + RC_CONDCLEAN=1 + RC_WPW
#   ga pga gb gc pgbc c trace order d    as psp/v26.sh (the same scripts, routes and windows)
#   emu    d in PPSSPP on the three routes: deck and w01a against prof/hashref_x86_{deck,w01a}_v27.txt, free against REF_FREE
# Container inputs (ephemeral): v24/d (genf.txt, layouts), psp/vp22, the packs psp5 and v25f/pak, the pads, the disc files for mk_headers.
S=$1; V=${2:-/home/claude/v28}; MG=/mnt/project-files/mgs2; T=$MG/tools
WPW=${WPW:-wpd_all6}; REF_FREE=${REF_FREE:-$MG/prof/hashref_x86_free_v28.txt}; TITLE="MGS2 timing harness v28"
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a"
mkdir -p $V; cd $V || exit 1
case $S in
hdr)
  gzip -dc $MG/recomp/widths/$WPW.json.gz > $V/$WPW.json || exit 1
  RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_CONDCLEAN=1 RC_WPW=$V/$WPW.json J=${J:-3} \
    sh $T/mk_headers.sh $V/WP > hdr.out 2>&1 ;;
ga)
  mkdir -p ga && cp /home/claude/v24/d/genf.txt ga/
  RTXX="-Wno-error=coverage-mismatch -DVP_VFPU" VP=/home/claude/psp/vp22 J=4 sh $T/dev/mk_psp.sh $V/ga $V/WP PGO=gen PGO_ELF=1 PGO_STAGE= LAYOUT=1 \
    STAGES="$STAGES" PSP_EBOOT_TITLE="$TITLE pgo elf" > ga_build.out 2>&1 ;;
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
  cd $V && W=$V/WP VP=/home/claude/psp/vp22 TITLE="$TITLE" sh $T/dev/rebuild_use.sh $V/c > c_build.out 2>&1 ;;
trace)
  P="prof 24001 24006\nprof 24151 24156\nprof 24301 24306\nprof 24451 24456\nprof 24601 24606\nprof 24751 24756\nprof 24901 24906\nprof 25051 25056"
  PF="prof 30801 30806\nprof 31301 31306\nprof 32201 32206\nprof 32951 32956\nprof 33401 33406\nprof 33851 33856\nprof 34801 34806\nprof 35801 35806"
  IT=1 VB=25060 sh $T/dev/trace_run.sh $V/c t28d deck $V/lay "$P" 8 &
  IT=1 VB=25060 sh $T/dev/trace_run.sh $V/c t28w w01a $V/lay "$P" 8 &
  IT=1 VB=35810 MAXS=5000 sh $T/dev/trace_run.sh $V/c t28f free $V/lay "$PF" 8 &
  wait ;;
order)
  [ -x $V/layout_sim ] || gcc -O2 -pthread -o $V/layout_sim $T/layout_sim.c -lm || exit 1
  cd lay || exit 1
  python3 $T/layout_funcs.py ../c/MGS2T.elf ../c/ovl_w00a.elf ../c w00a > funcs_deck.txt
  python3 $T/layout_funcs.py ../c/MGS2T.elf ../c/ovl_w01a.elf ../c w01a > funcs_w01a.txt
  python3 -c "
import numpy as np
def wins(p):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == 9; return [a[m[k]:m[k + 1]] for k in range(8)]
d, f = wins('t28d.it'), wins('t28f.it')
np.concatenate(d[:7] + f[:7] + [d[7], f[7]]).tofile('t_df.it')"
  B1=$(printf %08x $((0x$(psp-nm ../c/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  for s in 1 2; do
    $V/layout_sim t_df.it funcs_deck.txt -and t28w.it funcs_w01a.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -opt 40000 -j 2 \
      -seed $s -out order_s$s.txt > ls_s$s.out 2>&1 &
  done
  wait ;;
d)
  rm -rf d && cp -a c d
  W=$V/WP VP=/home/claude/psp/vp22 TITLE="$TITLE" sh $T/dev/relink_order.sh $V/lay/order_s${SEED:-1}.txt $V/d > d_build.out 2>&1
  cd lay && python3 $T/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w00a.elf ../d w00a > funcs_deck_d.txt && python3 $T/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w01a.elf ../d w01a > funcs_w01a_d.txt
  B1=$(printf %08x $((0x$(psp-nm ../d/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  $V/layout_sim t_df.it funcs_deck.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -new funcs_deck_d.txt > new_dd.log 2>&1
  $V/layout_sim t28w.it funcs_w01a.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -new funcs_w01a_d.txt > new_dw.log 2>&1 ;;
emu)
  GG=/home/claude/ms_home/.ppsspp/PSP/GAME; mkdir -p emu; export PPSSPP_VFPU_RN=1
  F=$(psp-nm d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  for r in deck w01a free; do
    G=$GG/MGS2V28_$r; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/split.* $G/battery.txt
    cp d/EBOOT.PBP d/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $G/
    if [ $r = free ]; then cp /home/claude/v25f/dl/pad.txt $G/; ln -sf /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak
      cp /home/claude/v26/dl_draft/mgs2.cfg $G/mgs2.cfg; echo 'prof 36495 36496' >> $G/mgs2.cfg; cp $REF_FREE $G/hashref.txt
    else cp $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak
      cp $MG/prof/emu_v24/mgs2_$r.cfg $G/mgs2.cfg; echo 'prof 25195 25196' >> $G/mgs2.cfg; cp $MG/prof/hashref_x86_${r}_v27.txt $G/hashref.txt; fi
    (sh $T/dev/prof_run.sh $G 0x$F $V/emu/x_$r ${MAXS:-9000} > emu/run_$r.out 2>&1; cp $G/log.txt emu/log_$r.txt; cp $G/steps.csv emu/steps_$r.csv
     ls $G/split.* > emu/splitfiles_$r.txt 2>/dev/null; echo done > emu/done_$r) &
  done
  wait ;;
*) echo "v28.sh: step hdr | ga | pga | gb | gc | pgbc | c | trace | order | d | emu"; exit 1 ;;
esac
