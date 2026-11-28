#!/bin/sh
# fin40.sh [psp dir, default psp] [tag, default f] (gl40, the final build's emulator gates; as build_gl39/fin39.sh): the package pk_<tag>40
# (<psp dir>'s EBOOT and 8 overlays, cfg/mgs2_gl40.cfg, cfg/hashref_gl38.txt, pad.txt) on the emulator with its own cfg (<tag>2) and
# with the XP check in every plan (<tag>3: cfg_emu/mgs2_gl40_xpchk.cfg), side by side; then the digests by kind against the twin's
# same-schedule arm (tw sch) for both runs, the static gate (g4_40.sh) and the reader made from <tag>2's log and run on it.
R=/home/claude/main/build_gl40; cd $R || exit 1; P=${1:-psp}; F=${2:-f}; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g40
K=$R/pk_${F}40; [ -e $K ] && { echo "fin40.sh: $K exists"; exit 1; }
mkdir -p $S $K && cp $R/$P/EBOOT.PBP $R/$P/ovl_*.bin $K/ && cp $R/cfg/mgs2_gl40.cfg $K/mgs2.cfg && cp $R/cfg/hashref_gl38.txt $K/hashref.txt \
  && cp /home/claude/main/build_gl38/pk_f38/pad.txt $K/pad.txt || exit 1
( cd $K && md5sum * ) > $K.md5
( PKG=$K nice -n 10 sh em40.sh ${F}2 > $R/em_${F}2.out 2>&1 ) &
( PKG=$K nice -n 10 sh em40.sh ${F}3 $R/cfg_emu/mgs2_gl40_xpchk.cfg > $R/em_${F}3.out 2>&1 ) &
wait
for t in ${F}2 ${F}3; do case $t in f2) rm -rf "/home/claude/main/build_gl40/ms/em_f2" ;; f3) rm -rf "/home/claude/main/build_gl40/ms/em_f3" ;;
  g2) rm -rf "/home/claude/main/build_gl40/ms/em_g2" ;; g3) rm -rf "/home/claude/main/build_gl40/ms/em_g3" ;; esac; done
for t in ${F}2 ${F}3; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200; done
zcat $R/tw/x86/out_sch.log.gz > $S/sch.log
for t in ${F}2 ${F}3; do echo "== $t's digests by kind against the twin's same-schedule arm (tw sch)"; python3 -I tools/k0cmp40.py $S/sch.log $R/emu_$t/log.txt; done
[ -f $R/emu_n1/log.txt ] && { echo "== ${F}2 against the no-PGO build's run n1 (the same package cfg), digests by kind"; python3 -I tools/k0cmp40.py $R/emu_n1/log.txt $R/emu_${F}2/log.txt; }
echo "== totals"; for t in ${F}2 ${F}3; do grep -a "^gl40 XP totals\|^F1 totals\|^bound totals\|^reuse totals\|^natcull: totals\|headroom\|malloc can still\|^nat: PROBLEMS\|NAT PROBLEMS\|the break's highest\|me picture lines" $R/emu_$t/log.txt | cut -c1-700; done
sh g4_40.sh $R/$P > $R/g4_40_$F.out 2>&1; cat $R/g4_40_$F.out
python3 -I rd/mkpc40.py $R/emu_${F}2/log.txt $R/rd/rd40pc_$F.py && python3 -I rd/rd40pc_$F.py $R/emu_${F}2 > $R/rd/rd40pc_${F}2.out 2>&1; grep -n '^== \(END\|HASHES\|DIGESTS\|SCHEDULE\)' $R/rd/rd40pc_${F}2.out | cut -c1-300
echo "fin40.sh: done $(date -u +%H:%M:%S)"
