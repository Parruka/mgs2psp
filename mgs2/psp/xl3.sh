#!/bin/sh
# xl3.sh <step> [<args>]: MGS2T_xl3, xl2 (psp/xl2.sh) with the one walk (2026-10-26; renderer_notes 11.12, task #47): runtime/ee_io.h's
# vif1_chain_fast hands the translator the runs of words it walks (XL_ONE) and the transfer's end (psp/xl_psp.c xlive_one_end), so a VIF1
# transfer is walked once; GIF transfers keep the second walk.  mgs2.cfg MGS2_XL_ONE=0 turns it off (xl2's second walk for every transfer),
# so one build times both.  The translator's time is then inside the region HOST_GS: "prof A B xg" counts the translator and the GS walk
# together (psp/main_psp.c), for both modes and with the translator off.  Base, hooks, switch (OPT_XL) and steps as xl2.sh's; the work dir
# is $XL3 or /home/claude/xl3, the game folders MGS2XL3*.  The translator reads guest memory only, so every state hash must equal the state
# hash 2 references, as xl2's.
S=$1; X=${XL3:-/home/claude/xl3}; MG=/mnt/project-files/mgs2; T=$MG/tools   # work dir: $XL3 or /home/claude/xl3
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
  XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o" MAINX=-DMGS2_XL RTX18=-DMGS2_XLIVE W=$V/WP VP=/home/claude/psp/vp22 \
    TITLE="MGS2 harness xl3 one walk" sh $T/dev/rebuild_use.sh $X/d boot_rt.o main_psp.o > $X/d_build.out 2>&1
  cd $X/d && psp-nm MGS2T.elf | grep -c ' xl_\| xlv_\| gu_\| gf_' >> $X/d_build.out ;;
emu)
  export PPSSPP_VFPU_RN=1; mkdir -p $X/emu; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}'); shift; [ $# -gt 0 ] || set -- in deck incyc
  for r in "$@"; do
    G=$GG/MGS2XL3_$r; rm -f $X/emu/done_$r
    case $r in in) setup_game $G in $X/d 'prof 37795 37796' ;; deck) setup_game $G deck $X/d 'prof 25195 25196' ;;
                incyc) setup_game $G in $X/d 'MGS2_OPT_CYCLE=31800:100:ffffffff,fffffbff' 'prof 37795 37796' ;;
                inoff) setup_game $G in $X/d 'MGS2_OPT=fffffbff' 'prof 37795 37796' ;;
                in2) setup_game $G in $X/d 'MGS2_XL_ONE=0' 'prof 37795 37796' ;; *) echo "xl3.sh emu: no run $r"; continue ;; esac
    (sh $T/dev/prof_run.sh $G 0x$F $X/emu/x_$r ${MAXS:-9000} > $X/emu/run_$r.out 2>&1; cp $G/log.txt $X/emu/log_$r.txt; cp $G/steps.csv $X/emu/steps_$r.csv
     ls $G/split.* > $X/emu/splitfiles_$r.txt 2>/dev/null; echo done > $X/emu/done_$r) &
  done
  wait ;;
dump)
  t=$2; B=$3; r=$4; vb=$5; shift 5
  export PPSSPP_VFPU_RN=1; mkdir -p $X/dump; F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  G=$GG/MGS2XL3D_$t; setup_game $G $r $B "prof $((vb - 5)) $((vb - 4))" "$@"; echo "$vb 0000000000000000 0" > $G/hashref.txt
  sh $T/dev/prof_run.sh $G 0x$F $X/dump/x_$t ${MAXS:-9000} > $X/dump/run_$t.out 2>&1; cp $G/log.txt $X/dump/log_$t.txt; ls -la $G/split.* > $X/dump/split_$t.txt 2>&1
  touch $X/dump/done_$t ;;
model)
  t=$2; a=$3; b=$4; m=$5; r=${6:-in}; if [ $# -ge 6 ]; then shift 6; else shift $#; fi
  mkdir -p $X/model; export PPSSPP_VFPU_RN=1; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  G=$GG/MGS2XL3M_$t; setup_game $G $r $X/d "vblanks $b" "MGS2_HASH_EVERY=500" "prof $a $b $m" "$@"
  sed -i '/^MGS2_HASH_EVERY=500,31800:100$/d; /^vblanks 37800$/d' $G/mgs2.cfg
  sh $T/dev/prof_run.sh $G 0x$F $X/model/$t 4000 $X/model/$t.calls > $X/model/run_$t.out 2>&1
  cp $G/log.txt $X/model/log_$t.txt; cp $G/mgs2.cfg $X/model/cfg_$t.txt; touch $X/model/done_$t ;;
*) echo "xl3.sh: step d | emu | dump | model"; exit 1 ;;
esac
