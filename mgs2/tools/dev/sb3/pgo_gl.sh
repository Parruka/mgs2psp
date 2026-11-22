#!/bin/sh
# pgo_gl.sh <step> [<work dir, default /home/claude/sbg>]: (sb3's copy: T = sb3's tree, gl9's lanes 1 + 2 and the trims of renderer_notes 11.28; the order step frees each trace
# before writing its reordered copy) a PGO profile for the picture code (task #103, renderer_notes 11.22): ge/*.c and
# psp/xl_psp.c have been plain -O2 since gl1 (never in a profile).  The base is gl7's build (/home/claude/gl7/d) with this tree's lane 1,
# as mkpsp.sh; boot_rt.o is profiled again too (lane 1 changed vif1_chain_fast, so its old profile no longer fits io_flush).  The game's
# own objects (gen.o, gen_dispatch.o, the overlays) keep gl7's profile: their code is gl7's.  Game data passes through it: personal use only.
#   gen    build g: boot_rt.o, xl_psp.o and the picture objects counting their branches (-fprofile-generate), main_psp.o with MGS2_PGO
#          (main_psp.c pgo_vb: counts zeroed at vb A, written to ms0:/pgo at vb B); the rest as gl7's
#   run    g in PPSSPP on the indoor route from boot, "pgo 15000 37790": the opening's two codec calls, the walk across the aft deck and the
#          two minutes inside, picture on; the counts go to <work>/pg (and the log: its hashes against the route's reference)
#   use    build u: those objects with -fprofile-use and the new counts, gl7's code order (layout files), relinked as mkpsp.sh
#   trace <0|1>   u in PspProf with an instruction trace of eight 5-step windows ("prof a b all": every fetch counts the same, since
#          the model and the PSP time whole steps): 0 = the deck's code (overlay w00a: the opening's two codec calls, Snake waiting, the
#          walk), 1 = inside (w01a); the last two windows of each are held out of the search (see order)
#   order <seed>  tools/layout_sim.c on both traces (one ELF order for the two overlays), 40,000 moves; lay/order_s<seed>.txt
#   d <seed>      build d = u relinked with that order (tools/dev/relink_order.sh), and layout_sim -new on its real addresses
# Then psp/gl3.sh with GL3=<work dir>: "emu hw" (the delivery's cfg, dl_cfg.txt from gl7) and "model" on d.
D0=/home/claude/gl7/d; T=/home/claude/sb3; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
S=$1; X=${2:-/home/claude/sbg}; GG=/home/claude/ms_home/.ppsspp/PSP/GAME; P=/home/claude/ms_home/.ppsspp/pgo
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
PGEN="-fprofile-generate -fprofile-update=single"
PUSE="-fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning"
GE="xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2"
XO="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o"
# the picture objects, xl_psp.o and boot_rt.o in the current directory with profile flags $1 (boot_rt's line is gl7's make.log line with
# its -fprofile-use flags replaced and this tree's runtime/ first)
objs() {
  for f in $GE; do psp-gcc $GF $1 -c $T/ge/$f.c -o ge_$f.o || return 1; done
  psp-gcc $GF $1 -DMGS2_XL -DMGS2_GL -DPOOL_KB=512 -I$SDK/include -I$T/runtime -I$MG/runtime -c $T/psp/xl_psp.c -o xl_psp.o || return 1
  L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" $D0/make.log | head -1 | sed "s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|-fprofile-use .*-fno-predictive-commoning|$1|")
  eval "$L" > boot_rt.out 2>&1
}
# the link outputs, so make links them again (the overlays are linked against the ELF's symbols)
unlink_out() {
  rm -f MGS2T.elf EBOOT.PBP PARAM.SFO ovl_init.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_n_title.bin ovl_d00t.bin ovl_d01t.bin ovl_w00a.bin ovl_w01a.bin \
    ovl_init.elf ovl_r_title.elf ovl_r_tnk0.elf ovl_n_title.elf ovl_d00t.elf ovl_d01t.elf ovl_w00a.elf ovl_w01a.elf
}
export XOBJS="$XO" MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576"
export XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user"
case $S in
gen)
  [ ! -e $X/g ] && mkdir -p $X && cp -a $D0 $X/g && cd $X/g || { echo "pgo_gl.sh gen: $X/g exists"; exit 1; }
  objs "$PGEN" || { echo "pgo_gl.sh gen: a compile failed"; exit 1; }
  rm -f main_psp.o; unlink_out
  OO=$(for o in *.o; do printf -- "-o %s " $o; done)
  RTXX="-Wno-error=coverage-mismatch -DVP_VFPU $RTX18" VP=/home/claude/psp/vp22 J=4 timeout 1800 sh $MG/tools/dev/mk_psp.sh $X/g /home/claude/v31/WPL \
    PGO=gen PGO_ELF= PGO_STAGE= LAYOUT=1 STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a" PSP_EBOOT_TITLE="MGS2 harness sb1 pgo" $OO
  tail -n 1 make.log; echo "compiles: $(grep -c 'psp-gcc.* -c ' make.log)"; ls -l EBOOT.PBP && md5sum EBOOT.PBP ;;
run)
  [ -z "$(ls $P)" ] || { echo "pgo_gl.sh run: $P is not empty"; exit 1; }
  export PPSSPP_VFPU_RN=1; G=$GG/MGS2SBG_in; mkdir -p $G/memcard0 $X/pg
  rm -f $G/log.txt $G/steps.csv $G/battery.txt $G/crash.txt
  cp $X/g/EBOOT.PBP $X/g/ovl_*.bin $G/; cp /home/claude/mgs2/SLES_503.83 $G/
  cp $MG/play/pad_w01af.txt $G/pad.txt; ln -sf /home/claude/v30/pak/mgs2disc.pak $G/mgs2disc.pak
  cp /home/claude/v30/dl_draft/mgs2.cfg $G/mgs2.cfg; cp $MG/prof/hashref_x86_in_h2.txt $G/hashref.txt
  printf 'MGS2_GL_TEXKB=2688\npgo 15000 37790\n' >> $G/mgs2.cfg
  F=$(psp-nm $X/g/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  sh $MG/tools/dev/prof_run.sh $G 0x$F $X/pg/prof_in.txt ${MAXS:-5000} $G/steps.csv > $X/pg/run_in.out 2>&1
  cp $G/log.txt $X/pg/log_in.txt; cp $G/mgs2.cfg $X/pg/cfg_in.txt; mkdir -p $X/pg/gcda; mv $P/*.gcda $X/pg/gcda/; ls -l $X/pg/gcda ;;
use)
  [ ! -e $X/u ] && cp -a $D0 $X/u && cd $X/u || { echo "pgo_gl.sh use: $X/u exists"; exit 1; }
  cp $X/pg/gcda/*.gcda . || exit 1
  objs "$PUSE" || { echo "pgo_gl.sh use: a compile failed"; exit 1; }
  W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 TITLE="MGS2 harness sb1 pgo" sh $MG/tools/dev/rebuild_use.sh $X/u > relink.out 2>&1
  tail -n 3 relink.out; ls -l EBOOT.PBP && md5sum EBOOT.PBP ;;
trace)
  k=$3; case $k in 0) W="16001 17501 26501 27801 29601 30101 30701 31101" ;; 1) W="31901 32651 33401 34151 34901 35651 36401 37151" ;;
                   *) echo "pgo_gl.sh trace: 0 or 1"; exit 1 ;; esac
  export PPSSPP_VFPU_RN=1; G=$GG/MGS2SBG_t$k; mkdir -p $G/memcard0 $X/lay
  rm -f $G/log.txt $G/steps.csv $G/battery.txt $G/crash.txt
  cp $X/u/EBOOT.PBP $X/u/ovl_*.bin $G/; cp /home/claude/mgs2/SLES_503.83 $G/
  cp $MG/play/pad_w01af.txt $G/pad.txt; ln -sf /home/claude/v30/pak/mgs2disc.pak $G/mgs2disc.pak
  cp /home/claude/v30/dl_draft/mgs2.cfg $G/mgs2.cfg; cp $MG/prof/hashref_x86_in_h2.txt $G/hashref.txt
  sed -i '/^MGS2_HASH_EVERY=/d; /^vblanks /d' $G/mgs2.cfg; e=0
  { echo 'MGS2_GL_TEXKB=2688'; echo 'MGS2_HASH_EVERY=500'; for a in $W; do echo "prof $a $((a + 5)) all"; e=$((a + 10)); done; echo "vblanks $e"; } >> $G/mgs2.cfg
  F=$(psp-nm $X/u/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  PSPPROF_ITRACE=$X/lay/t$k.it PSPPROF_ITRACE_ALL=1 PSPPROF_ITRACE_MARK=1 \
    sh $MG/tools/dev/prof_run.sh $G 0x$F $X/lay/t$k ${MAXS:-5000} $X/lay/t$k.7.calls > $X/lay/run_t$k.out 2>&1
  cp $G/log.txt $X/lay/log_t$k.txt; cp $G/mgs2.cfg $X/lay/cfg_t$k.txt; echo done > $X/lay/done_t$k ;;
order)
  sd=$3; [ -x $X/layout_sim ] || gcc -O2 -pthread -o $X/layout_sim $MG/tools/layout_sim.c -lm || exit 1
  cd $X/lay || exit 1
  [ -f funcs_deck.txt ] || python3 $MG/tools/layout_funcs.py ../u/MGS2T.elf ../u/ovl_w00a.elf ../u w00a > funcs_deck.txt 2> funcs_deck.err
  [ -f funcs_w01a.txt ] || python3 $MG/tools/layout_funcs.py ../u/MGS2T.elf ../u/ovl_w01a.elf ../u w01a > funcs_w01a.txt 2> funcs_w01a.err
  # each trace's windows in a mixed order, the two held out last: deck (call 1, call 2, waiting, walk; held out call 2 and waiting),
  # inside (held out 35,651 and 36,401: the corner and the alert's start).  The codec calls run in other overlays (call 1 in d00t,
  # call 2 in d01t: t0's log), whose code is not laid out: their fetches in the overlay arena move up 16 MB (a multiple of the I-cache's
  # 8 KB way, so the same cache sets), out of every listed function, so layout_sim keeps them where they are instead of reading them as
  # w00a's functions
  A0=$(psp-nm ../u/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' ../u/make.log | head -1 | cut -d= -f2)
  [ -f t0m.it ] || python3 -c "
import numpy as np, os
def wins(p):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == 9, (p, len(m)); return [a[m[k]:m[k + 1]] for k in range(8)]
import os
d = wins('t0.it'); os.remove('t0.it'); lo, hi = 0x$A0, 0x$A0 + $OS   # (in memory now: the disk allowance holds one trace copy)
for k in (0, 1, 2, 3):
    x = d[k].copy(); s = (x[:, 0] >= lo) & (x[:, 0] < hi); x[s, 0] += 0x1000000; d[k] = x
    print('window', k, 'arena fetch runs moved:', int(s.sum()), 'of', len(x))
np.concatenate([d[k] for k in (0, 2, 4, 6, 1, 7, 3, 5)]).tofile('t0m.it'); del d
w = wins('t1.it'); os.remove('t1.it'); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1m.it'); del w"
  B0=$(psp-nm ../u/MGS2T.elf | awk '$3 == "_ftext" { print $1 }')
  B1=$(printf %08x $((0x$(psp-nm ../u/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  echo "base0 $B0 base1 $B1" > bases.txt
  $X/layout_sim t0m.it funcs_deck.txt -and t1m.it funcs_w01a.txt -wl 1 -wd 1 -test 2 -base0 $B0 -base1 $B1 -opt 40000 -j 2 \
    -seed $sd -out order_s$sd.txt > ls_s$sd.out 2>&1 ;;
d)
  sd=$3; [ ! -e $X/d ] && cp -a $X/u $X/d || { echo "pgo_gl.sh d: $X/d exists"; exit 1; }
  W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 TITLE="MGS2 harness sb1 pgo layout" sh $MG/tools/dev/relink_order.sh $X/lay/order_s$sd.txt $X/d > $X/d_build.out 2>&1
  cd $X/lay && python3 $MG/tools/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w00a.elf ../d w00a > funcs_deck_d.txt 2>/dev/null \
    && python3 $MG/tools/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w01a.elf ../d w01a > funcs_w01a_d.txt 2>/dev/null
  B0=$(psp-nm ../d/MGS2T.elf | awk '$3 == "_ftext" { print $1 }')
  B1=$(printf %08x $((0x$(psp-nm ../d/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  $X/layout_sim t0m.it funcs_deck.txt -wl 1 -wd 1 -test 2 -base0 $B0 -base1 $B1 -new funcs_deck_d.txt > new_d0.log 2>&1
  $X/layout_sim t1m.it funcs_w01a.txt -wl 1 -wd 1 -test 2 -base0 $B0 -base1 $B1 -new funcs_w01a_d.txt > new_d1.log 2>&1
  tail -n 4 $X/d_build.out; tail -n 4 new_d0.log new_d1.log; md5sum $X/d/EBOOT.PBP ;;
*) echo "pgo_gl.sh: step gen | run | use | trace <0|1> | order <seed> | d <seed>"; exit 1 ;;
esac
