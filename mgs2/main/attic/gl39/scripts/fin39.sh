#!/bin/sh
# fin39.sh (gl39, the final build's emulator gates): the package pk_f39 (build_gl39/psp's EBOOT and 8 overlays, cfg/mgs2_gl39.cfg,
# cfg/hashref_gl38.txt, pad.txt) on the emulator with its own cfg (f2) and with F1's check cfg (f3: cfg_emu/mgs2_gl39_f1chk.cfg), side
# by side; then the digests by kind against the twin's same-schedule arm (tw2 sch), the static gate (g4_39.sh) and the reader made
# from f2's log and run on it.
R=/home/claude/main/build_gl39; cd $R || exit 1; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g39
[ -e $R/pk_f39 ] && { echo "fin39.sh: pk_f39 exists"; exit 1; }
mkdir -p $S $R/pk_f39 && cp $R/psp/EBOOT.PBP $R/psp/ovl_*.bin $R/pk_f39/ && cp $R/cfg/mgs2_gl39.cfg $R/pk_f39/mgs2.cfg && cp $R/cfg/hashref_gl38.txt $R/pk_f39/hashref.txt \
  && cp /home/claude/main/build_gl38/pk_f38/pad.txt $R/pk_f39/pad.txt || exit 1
( cd $R/pk_f39 && md5sum * ) > $R/pk_f39.md5
( PKG=$R/pk_f39 nice -n 10 sh em39.sh f2 > $R/em_f2.out 2>&1; rm -rf "/home/claude/main/build_gl39/ms/em_f2" ) &
( PKG=$R/pk_f39 nice -n 10 sh em39.sh f3 $R/cfg_emu/mgs2_gl39_f1chk.cfg > $R/em_f3.out 2>&1; rm -rf "/home/claude/main/build_gl39/ms/em_f3" ) &
wait
for t in f2 f3; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200; done
zcat $R/tw2/x86/out_sch.log.gz > $S/sch2.log
echo "== f2's digests by kind against the twin's same-schedule arm (tw2 sch)"; python3 -I tools/k0cmp39.py $S/sch2.log $R/emu_f2/log.txt
echo "== f2 against the no-PGO build's run n1 (the same schedule), digests by kind"; python3 -I tools/k0cmp39.py $R/emu_n1/log.txt $R/emu_f2/log.txt
echo "== totals"; for t in f2 f3; do grep -a "^F1 totals\|^bound totals\|^reuse totals\|^natcull: totals\|headroom\|malloc can still\|^nat: PROBLEMS\|NAT PROBLEMS\|the break's highest" $R/emu_$t/log.txt | cut -c1-700; done
sh g4_39.sh $R/psp > $R/g4_39.out 2>&1; cat $R/g4_39.out
python3 -I rd/mkpc39.py $R/emu_f2/log.txt $R/rd/rd39pc.py && python3 -I rd/rd39pc.py $R/emu_f2 > $R/rd/rd39pc_f2.out 2>&1; grep -n '^== \(END\|HASHES\|DIGESTS\|SCHEDULE\)' $R/rd/rd39pc_f2.out | cut -c1-300
echo "fin39.sh: done $(date -u +%H:%M:%S)"
