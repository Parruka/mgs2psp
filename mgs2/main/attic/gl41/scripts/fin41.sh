#!/bin/sh
# fin41.sh [psp dir, default psp] [tag, default f] (gl41: build_gl40/fin40.sh with gl41's gates): the package pk_<tag>41 (<psp dir>'s
# EBOOT and 8 overlays, cfg/mgs2_gl41.cfg, cfg/hashref_gl38.txt, build_gl38/pk_f38/pad.txt) on the emulator:
#   <tag>2  the package as delivered;  <tag>3  with the XP check in every plan (cfg_emu/mgs2_gl41_xpchk.cfg)   (side by side), then
#   <tag>h1 / <tag>h0  THE HEAP GATE's pair: digests in every frame from 24,000 (cfg_emu/mgs2_gl41_h1.cfg) with the package's native
#           turns, and the same with MGS2_NGE_CYCLE all 0 (h0)   (side by side);
# then the digests by kind: <tag>2 and <tag>3 against the twin's same-schedule arm (tw2 sch), <tag>h1 against <tag>h0 (every non-N frame's
# list/frame equal, every frame's content equal); the heap lines and totals; the static gate (g4_41.sh); the reader made from <tag>2's log.
# Each game folder is deleted after its run (literal paths); logs stay in emu_<tag>*.
R=/home/claude/main/build_gl41; cd $R || exit 1; P=${1:-psp}; F=${2:-f}; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g41
K=$R/pk_${F}41; [ -e $K ] && { echo "fin41.sh: $K exists"; exit 1; }
mkdir -p $S $K && cp $R/$P/EBOOT.PBP $R/$P/ovl_*.bin $K/ && cp $R/cfg/mgs2_gl41.cfg $K/mgs2.cfg && cp $R/cfg/hashref_gl38.txt $K/hashref.txt \
  && cp /home/claude/main/build_gl38/pk_f38/pad.txt $K/pad.txt || exit 1
( cd $K && md5sum * ) > $K.md5; cat $K.md5
del() { case $1 in
  f2) rm -rf "/home/claude/main/build_gl41/ms/em_f2" ;; f3) rm -rf "/home/claude/main/build_gl41/ms/em_f3" ;;
  fh1) rm -rf "/home/claude/main/build_gl41/ms/em_fh1" ;; fh0) rm -rf "/home/claude/main/build_gl41/ms/em_fh0" ;;
  g2) rm -rf "/home/claude/main/build_gl41/ms/em_g2" ;; g3) rm -rf "/home/claude/main/build_gl41/ms/em_g3" ;;
  gh1) rm -rf "/home/claude/main/build_gl41/ms/em_gh1" ;; gh0) rm -rf "/home/claude/main/build_gl41/ms/em_gh0" ;; esac; }
( PKG=$K nice -n 5 sh em41.sh ${F}2 > $R/em_${F}2.out 2>&1 ) &
( PKG=$K nice -n 5 sh em41.sh ${F}3 $R/cfg_emu/mgs2_gl41_xpchk.cfg > $R/em_${F}3.out 2>&1 ) &
wait; del ${F}2; del ${F}3
( PKG=$K nice -n 5 sh em41.sh ${F}h1 $R/cfg_emu/mgs2_gl41_h1.cfg > $R/em_${F}h1.out 2>&1 ) &
( PKG=$K nice -n 5 sh em41.sh ${F}h0 $R/cfg_emu/mgs2_gl41_h0.cfg > $R/em_${F}h0.out 2>&1 ) &
wait; del ${F}h1; del ${F}h0
ls $R/ms
for t in ${F}2 ${F}3 ${F}h1 ${F}h0; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200; done
zcat $R/tw2/x86/out_sch.log.gz > $S/sch.log
for t in ${F}2 ${F}3; do echo "== $t's digests by kind against the twin's same-schedule arm (tw2 sch)"; python3 -I tools/k0cmp41.py $S/sch.log $R/emu_$t/log.txt; done
echo "== THE HEAP GATE's digests: ${F}h1 (native turns) against ${F}h0 (MGS2_NGE_CYCLE all 0), by kind"; python3 -I tools/k0cmp41.py $R/emu_${F}h0/log.txt $R/emu_${F}h1/log.txt
echo "== totals and heap"; for t in ${F}2 ${F}3 ${F}h1 ${F}h0; do echo "-- $t"; grep -a "^gl40 XP totals\|^F1 totals\|^bound totals\|^natcull: totals\|^nge totals\|^nge converter\|headroom\|malloc can still\|^nat: PROBLEMS\|NAT PROBLEMS\|^memory (gl41)\|^picture heap (gl41)\|me picture loop (gl41)\|me picture loop: no thread\|textures refused\|me picture lines" $R/emu_$t/log.txt | cut -c1-600; done
rm -f $S/sch.log
sh g4_41.sh $R/$P > $R/g4_41_$F.out 2>&1; cat $R/g4_41_$F.out
python3 -I rd/mkpc41.py $R/emu_${F}2/log.txt $R/rd/rd41pc_$F.py && python3 -I rd/rd41pc_$F.py $R/emu_${F}2 > $R/rd/rd41pc_${F}2.out 2>&1; grep -n '^== \(END\|HASHES\|DIGESTS\|CONTENT\|SCHEDULE\)' $R/rd/rd41pc_${F}2.out | cut -c1-300
echo "fin41.sh: done $(date -u +%H:%M:%S)"
