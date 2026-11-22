#!/bin/sh
# xl1.sh <step> [<args>]: MGS2T_xl1, the live packet translator's PSP test build (2026-10-26; renderer_notes
# 11.12 "The plan for the PSP", step 1 on the PSP; psp_harness.md "Build xl1"; task #45).  Base: /home/claude/h2/d (v30's build d with
# state hash 2: PGO=use, v30's code order; ephemeral).  New against it:
#   boot_rt.o with -DMGS2_XLIVE: io_dma_start hands each VIF1 / GIF transfer to xlive_chk_dma before its exact walk (runtime/ee_io.h);
#   main_psp.o with -DMGS2_XL: the region HOST_XL timed apart (window lines, summary, steps.csv column xl_us; "prof A B xl" in mgs2.cfg
#     counts only inside it in PspProf) and the translator's totals at the end (xl_psp_report);
#   psp/xl_psp.c with ge/xlive.c (spans), ge/xlate.c (lazy UNPACK), ge/gsfront.c and ge/gsup.c (content keys on), linked after the rest.
# g_opt's OPT_XL (1024, on by default) switches the translator: MGS2_OPT=fffffbff runs the build without it; MGS2_OPT_CYCLE=<vb>:<n>:
# ffffffff,fffffbff takes turns.  The translator reads guest memory only, so every state hash must equal the state hash 2 references.
# Steps:
#   d      the build: tools/dev/rebuild_use.sh on a copy of h2/d with boot_rt.o and main_psp.o dropped (XOBJS, MAINX from the environment)
#   emu    [<run>...]: PPSSPP (JIT), runs side by side, against prof/hashes_x86_{in,deck}_h2.txt and their hashref: "in" the indoor
#          route (v30's delivered cfg) and "deck" the deck route, the translator on from boot (the default: in deck incyc); "incyc" the
#          indoor route with MGS2_OPT_CYCLE=31800:100:ffffffff,fffffbff; "inoff" the indoor route with the translator off (MGS2_OPT=fffffbff)
#   dump   <tag> <build dir> <route: in | deck> <vb> [<cfg line>...]: a run of that build whose only reference hash is a wrong one at <vb>,
#          so it writes EE RAM there (split.<vb> in its game folder): a RAM image to compare a split with
#   model  <tag> <A> <B> <prof mode: xl | all> [<route: in | deck>] [<cfg line>...]: PspProf over steps A..B-1 (one window per run, each
#          from boot), dumps in <work dir>/model
S=$1; X=${XL1:-/home/claude/xl1}; MG=/mnt/project-files/mgs2; T=$MG/tools   # work dir: $XL1 or /home/claude/xl1
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
GG=/home/claude/ms_home/.ppsspp/PSP/GAME; V=/home/claude/v30
# the game folder <dir> for route <in | deck> with build <build dir>'s files, the route's hashref and <cfg lines> appended to the route's
# cfg (its own variable names: the callers' G and r stay as they are)
setup_game() {
  sg_g=$1; sg_r=$2; sg_b=$3; shift 3
  mkdir -p $sg_g/memcard0; rm -f $sg_g/log.txt $sg_g/steps.csv $sg_g/split.* $sg_g/battery.txt
  cp $sg_b/EBOOT.PBP $sg_b/ovl_*.bin $sg_g/; cp $GG/MGS2V5/SLES_503.83 $sg_g/
  if [ $sg_r = in ]; then cp $MG/play/pad_w01af.txt $sg_g/pad.txt; ln -sf $V/pak/mgs2disc.pak $sg_g/mgs2disc.pak; cp $V/dl_draft/mgs2.cfg $sg_g/mgs2.cfg
  else cp $GG/MGS2V5/pad.txt $sg_g/; ln -sf /home/claude/psp5/mgs2disc.pak $sg_g/mgs2disc.pak; cp $MG/prof/emu_v24/mgs2_$sg_r.cfg $sg_g/mgs2.cfg; fi
  cp $MG/prof/hashref_x86_${sg_r}_h2.txt $sg_g/hashref.txt
  for l in "$@"; do echo "$l" >> $sg_g/mgs2.cfg; done
}
case $S in
d)
  rm -rf $X/d && mkdir -p $X && cp -a /home/claude/h2/d $X/d || exit 1
  XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o" MAINX=-DMGS2_XL RTX18=-DMGS2_XLIVE W=$V/WP VP=/home/claude/psp/vp22 \
    TITLE="MGS2 harness xl1 translator" sh $T/dev/rebuild_use.sh $X/d boot_rt.o main_psp.o > $X/d_build.out 2>&1
  cd $X/d && psp-nm MGS2T.elf | grep -c ' xl_\| xlv_\| gu_\| gf_' >> $X/d_build.out ;;
emu)
  export PPSSPP_VFPU_RN=1; mkdir -p $X/emu; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}'); shift; [ $# -gt 0 ] || set -- in deck incyc
  for r in "$@"; do
    G=$GG/MGS2XL1_$r; rm -f $X/emu/done_$r
    case $r in in) setup_game $G in $X/d 'prof 37795 37796' ;; deck) setup_game $G deck $X/d 'prof 25195 25196' ;;
                incyc) setup_game $G in $X/d 'MGS2_OPT_CYCLE=31800:100:ffffffff,fffffbff' 'prof 37795 37796' ;;
                inoff) setup_game $G in $X/d 'MGS2_OPT=fffffbff' 'prof 37795 37796' ;; *) echo "xl1.sh emu: no run $r"; continue ;; esac
    (sh $T/dev/prof_run.sh $G 0x$F $X/emu/x_$r ${MAXS:-9000} > $X/emu/run_$r.out 2>&1; cp $G/log.txt $X/emu/log_$r.txt; cp $G/steps.csv $X/emu/steps_$r.csv
     ls $G/split.* > $X/emu/splitfiles_$r.txt 2>/dev/null; echo done > $X/emu/done_$r) &
  done
  wait ;;
dump)
  t=$2; B=$3; r=$4; vb=$5; shift 5
  export PPSSPP_VFPU_RN=1; mkdir -p $X/dump; F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  G=$GG/MGS2XL1D_$t; setup_game $G $r $B "prof $((vb - 5)) $((vb - 4))" "$@"; echo "$vb 0000000000000000 0" > $G/hashref.txt
  sh $T/dev/prof_run.sh $G 0x$F $X/dump/x_$t ${MAXS:-9000} > $X/dump/run_$t.out 2>&1; cp $G/log.txt $X/dump/log_$t.txt; ls -la $G/split.* > $X/dump/split_$t.txt 2>&1
  touch $X/dump/done_$t ;;
model)
  t=$2; a=$3; b=$4; m=$5; r=${6:-in}; if [ $# -ge 6 ]; then shift 6; else shift $#; fi
  mkdir -p $X/model; export PPSSPP_VFPU_RN=1; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  G=$GG/MGS2XL1M_$t; setup_game $G $r $X/d "vblanks $b" "MGS2_HASH_EVERY=500" "prof $a $b $m" "$@"
  sed -i '/^MGS2_HASH_EVERY=500,31800:100$/d; /^vblanks 37800$/d' $G/mgs2.cfg
  sh $T/dev/prof_run.sh $G 0x$F $X/model/$t 4000 $X/model/$t.calls > $X/model/run_$t.out 2>&1
  cp $G/log.txt $X/model/log_$t.txt; cp $G/mgs2.cfg $X/model/cfg_$t.txt; touch $X/model/done_$t ;;
*) echo "xl1.sh: step d | emu | dump | model"; exit 1 ;;
esac
