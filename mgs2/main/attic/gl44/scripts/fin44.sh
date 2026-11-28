#!/bin/sh
# fin44.sh [psp dir, default psp] [tag, default f] (gl44: build_gl43/fin43.sh with gl44's package (cfg/mgs2_gl44.cfg, cfg/hashref_gl44.txt),
# cfg variants (tools/mkemu44.py: h0 has B and V44 off), the twin's s44 arm, k0cmp44, turnchk44, g4_44, mkpc44 and block A's gates):
#   <tag>2  the package as delivered (MGS2_BBOX=1);  <tag>3  with the XP check in every plan (cfg_emu/mgs2_gl44_xpchk.cfg);
#   <tag>h1 / <tag>h0  THE HEAP GATE's pair: digests in every frame from 24,000 (cfg_emu/mgs2_gl44_h1.cfg) with the package's turns, and
#           the same with MGS2_NGE_CYCLE all 0 and every OPT2 word ff9ff9ff (h0: nothing native, no B, no V44: REF=hashref_gl38);
# then the digests by kind (tools/k0cmp44.py): <tag>2 and <tag>3 against the twin's s44 arm, <tag>h1 against <tag>h0; each run's turns
# against tools/sched44.py (turnchk44); the heap lines and totals; the static gate (g4_44.sh); the reader made from <tag>2's log
# (rd/rd44pc_<tag>.py, run on <tag>2); rdbbox.py on <tag>2's bbox.txt; H1's three starts on the package (rot44.sh, E=emu_rot_<tag>).
# One emulator run at a time; each game folder is deleted after its run (literal paths); logs stay in emu_<tag>*.
R=/home/claude/main/build_gl44; cd $R || exit 1; P=${1:-psp}; F=${2:-f}
K=$R/pk_${F}44; [ -e $K ] && { echo "fin44.sh: $K exists"; exit 1; }
mkdir -p $K && cp $R/$P/EBOOT.PBP $R/$P/ovl_*.bin $K/ && cp $R/cfg/mgs2_gl44.cfg $K/mgs2.cfg && cp $R/cfg/hashref_gl44.txt $K/hashref.txt \
  && cp /home/claude/main/build_gl38/pk_f38/pad.txt $K/pad.txt || exit 1
( cd $K && md5sum * ) > $K.md5; cat $K.md5
del() { [ -d "/home/claude/main/build_gl44/ms/em_$1/.ppsspp" ] && case $1 in
  f2) rm -rf "/home/claude/main/build_gl44/ms/em_f2" ;; f3) rm -rf "/home/claude/main/build_gl44/ms/em_f3" ;;
  fh1) rm -rf "/home/claude/main/build_gl44/ms/em_fh1" ;; fh0) rm -rf "/home/claude/main/build_gl44/ms/em_fh0" ;;
  g2) rm -rf "/home/claude/main/build_gl44/ms/em_g2" ;; g3) rm -rf "/home/claude/main/build_gl44/ms/em_g3" ;;
  gh1) rm -rf "/home/claude/main/build_gl44/ms/em_gh1" ;; gh0) rm -rf "/home/claude/main/build_gl44/ms/em_gh0" ;; esac; }
PKG=$K sh em44.sh ${F}2 > $R/em_${F}2.out 2>&1; del ${F}2
PKG=$K sh em44.sh ${F}3 $R/cfg_emu/mgs2_gl44_xpchk.cfg > $R/em_${F}3.out 2>&1; del ${F}3
PKG=$K sh em44.sh ${F}h1 $R/cfg_emu/mgs2_gl44_h1.cfg > $R/em_${F}h1.out 2>&1; del ${F}h1
REF=/home/claude/main/build_gl41/cfg/hashref_gl38.txt PKG=$K sh em44.sh ${F}h0 $R/cfg_emu/mgs2_gl44_h0.cfg > $R/em_${F}h0.out 2>&1; del ${F}h0
ls $R/ms
for t in ${F}2 ${F}3 ${F}h1 ${F}h0; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200; done
for t in ${F}2 ${F}3; do echo "== $t's digests by kind against the twin's s44 arm"; python3 -I tools/k0cmp44.py $R/tw/x86/out_s44.log.gz $R/emu_$t/log.txt; done
echo "== THE HEAP GATE's digests: ${F}h1 (native, B and V44 turns) against ${F}h0 (MGS2_NGE_CYCLE all 0, B and V44 off), by kind"; python3 -I tools/k0cmp44.py $R/emu_${F}h0/log.txt $R/emu_${F}h1/log.txt 0 1073741824 B
for t in ${F}2 ${F}3 ${F}h1; do echo "== $t's turns against sched44"; python3 -I tools/turnchk44.py $R/emu_$t/log.txt | tail -n 2; done
echo "== totals and heap"; for t in ${F}2 ${F}3 ${F}h1 ${F}h0; do echo "-- $t"; grep -a "^gl40 XP totals\|^F1 totals\|^bound totals\|^natcull: totals\|^nge totals\|^nge people totals\|^b43\|^nge converter\|headroom\|malloc can still\|^nat: PROBLEMS\|NAT PROBLEMS\|^memory (gl41)\|^picture heap (gl41)\|me picture loop (gl41)\|me picture loop: no thread\|textures refused\|me picture lines\|growth late\|the carve\|^gl44 " $R/emu_$t/log.txt | cut -c1-600; done
sh g4_44.sh $R/$P > $R/g4_44_$F.out 2>&1; cat $R/g4_44_$F.out
python3 -I rd/mkpc44.py $R/emu_${F}2/log.txt $R/rd/rd44pc_$F.py && python3 -I rd/rd44pc_$F.py $R/emu_${F}2 > $R/rd/rd44pc_${F}2.out 2>&1; grep -n '^== \(END\|HASHES\|DIGESTS\|CONTENT\|SCHEDULE\|HEAP\)' $R/rd/rd44pc_${F}2.out | cut -c1-300
echo "== rdbbox.py on ${F}2's bbox.txt"; python3 -I $R/rd/rdbbox.py $R/emu_${F}2 > $R/rdbbox_${F}2.out 2>&1; echo "rdbbox rc $?" >> $R/rdbbox_${F}2.out; cat $R/rdbbox_${F}2.out | cut -c1-300
echo "== H1 three starts on the package"; [ -e $R/emu_rot_$F ] && echo "fin44.sh: emu_rot_$F exists" || { PKG=$K E=$R/emu_rot_$F sh rot44.sh > $R/rot44_$F.out 2>&1; cat $R/rot44_$F.out | cut -c1-400; }
echo "fin44.sh: done $(date -u +%H:%M:%S)"
