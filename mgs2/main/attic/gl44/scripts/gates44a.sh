#!/bin/sh
# gates44a.sh (gl44 block A's gates on the no-PGO build n44 / pk_n44): the imports against gl43's final build (g4_44.sh); the emulator,
# loop apart, with MGS2_BBOX=1 (a1: the package's cfg) and MGS2_BBOX=0 (a0: cfg_emu/mgs2_gl44_bb0.cfg) side by side, each against
# hashref_gl43 and its content against the twin's sch arm (build_gl43/tools/k0cmp43.py); the heap lines and the textures refused;
# rdbbox.py on a1's bbox.txt; the steps a1 against a0; then H1's three-start test (rot44.sh).  Game folders deleted after use.
R=/home/claude/main/build_gl44; cd $R || exit 1; T43=/home/claude/main/build_gl43
sh g4_44.sh $R/n44 > $R/g4_44_n.out 2>&1; cat $R/g4_44_n.out | cut -c1-300
( sh em44.sh a1 > $R/em_a1.out 2>&1; [ -d "/home/claude/main/build_gl44/ms/em_a1/.ppsspp" ] && rm -rf "/home/claude/main/build_gl44/ms/em_a1" ) &
( sh em44.sh a0 $R/cfg_emu/mgs2_gl44_bb0.cfg > $R/em_a0.out 2>&1; [ -d "/home/claude/main/build_gl44/ms/em_a0/.ppsspp" ] && rm -rf "/home/claude/main/build_gl44/ms/em_a0" ) &
wait
ls $R/ms 2>/dev/null
for t in a1 a0; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200
  echo "-- $t's content and digests by kind against the twin's sch arm"; python3 -I $T43/tools/k0cmp43.py $T43/tw/x86/out_sch.log.gz $R/emu_$t/log.txt
  echo "-- $t heap, textures, black box"; grep -a "^memory (gl41)\|^picture heap (gl41)\|textures refused\|malloc can still\|headroom\|^gl44 \|me picture loop (gl41)\|me picture loop: no thread\|^nat: PROBLEMS\|NAT PROBLEMS\|XP check" $R/emu_$t/log.txt | cut -c1-500; done
echo "== rdbbox.py on a1's bbox.txt"; python3 -I $R/rd/rdbbox.py $R/emu_a1 > $R/rdbbox_a1.out 2>&1; echo "rdbbox rc $?" >> $R/rdbbox_a1.out; cat $R/rdbbox_a1.out | cut -c1-300
echo "== steps a1 against a0 (emulator)"; python3 -I $R/stepcmp44.py $R/emu_a0/steps.csv $R/emu_a1/steps.csv
echo "== H1 three starts"; sh rot44.sh > $R/rot44.out 2>&1; cat $R/rot44.out | cut -c1-400
echo "gates44a.sh: done $(date -u +%H:%M:%S)"
