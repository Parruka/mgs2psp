#!/bin/sh
# fin38.sh (gl38, the final build's emulator gates): the package pk_f38 (build_gl38/psp's EBOOT and 8 overlays, cfg/mgs2_gl38.cfg,
# cfg/hashref_gl38.txt, pad.txt) on the emulator with its own cfg (f2) and with F1's check cfg (f3: cfg_emu/mgs2_gl38_f1chk.cfg), side
# by side; then the digests by kind against the twin's same-schedule arm (sch2) and the plain arm (g37d), the pictures against gl36's
# emulator pictures (build_gl36/emu_e2/pic), the static gate (g4_38.sh) and the reader made from f2's log and run on it.
R=/home/claude/main/build_gl38; cd $R || exit 1; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad
[ -e $R/pk_f38 ] && { echo "fin38.sh: pk_f38 exists"; exit 1; }
mkdir -p $R/pk_f38 && cp $R/psp/EBOOT.PBP $R/psp/ovl_*.bin $R/pk_f38/ && cp $R/cfg/mgs2_gl38.cfg $R/pk_f38/mgs2.cfg && cp $R/cfg/hashref_gl38.txt $R/pk_f38/hashref.txt \
  && cp /home/claude/main/build_gl37/pk_n37/pad.txt $R/pk_f38/pad.txt || exit 1
( cd $R/pk_f38 && md5sum * ) > $R/pk_f38.md5
( PKG=$R/pk_f38 nice -n 10 sh em38.sh f2 > $R/em_f2.out 2>&1; rm -rf "/home/claude/main/build_gl38/ms/em_f2" ) &
( PKG=$R/pk_f38 nice -n 10 sh em38.sh f3 $R/cfg_emu/mgs2_gl38_f1chk.cfg > $R/em_f3.out 2>&1; rm -rf "/home/claude/main/build_gl38/ms/em_f3" ) &
wait
for t in f2 f3; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200; done
[ -f $S/sch2.log ] || zcat $R/tw/x86/out_sch2.log.gz > $S/sch2.log; [ -f $S/g37d.log ] || zcat $R/tw/x86/out_g37d.log.gz > $S/g37d.log
echo "== f2's digests by kind against the twin's same-schedule arm (sch2)"; python3 -I tools/k0cmp38.py $S/sch2.log $R/emu_f2/log.txt
echo "== f2's digests by kind against the twin's plain arm (g37d, gl37's keys)"; python3 -I tools/k0cmp38.py $S/g37d.log $R/emu_f2/log.txt
echo "== f2 against the no-PGO build's run n4 (the same cfg), digests by kind"; python3 -I tools/k0cmp38.py $R/emu_n4/log.txt $R/emu_f2/log.txt
echo "== f2's pictures against gl36's emulator pictures (e2)"; python3 -I tools/piccmp38.py $R/emu_f2/pic /home/claude/main/build_gl36/emu_e2/pic > $R/piccmp_f2.out 2>&1; tail -n 2 $R/piccmp_f2.out
echo "== totals"; for t in f2 f3; do grep -a "^F1 totals\|^bound totals\|^reuse totals\|^natcull: totals\|headroom\|malloc can still\|^nat: PROBLEMS\|NAT PROBLEMS" $R/emu_$t/log.txt | cut -c1-600; done
sh g4_38.sh $R/psp > $R/g4_38.out 2>&1; cat $R/g4_38.out
python3 -I rd/mkpc38.py $R/emu_f2/log.txt $R/rd/rd38pc.py && python3 -I rd/rd38pc.py $R/emu_f2 > $R/rd/rd38pc_f2.out 2>&1; sed -n 16,22p $R/rd/rd38pc_f2.out | cut -c1-300
echo "fin38.sh: done $(date -u +%H:%M:%S)"
