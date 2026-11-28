#!/bin/sh
# gates44b.sh [build folder, default n44b] [package, default pk_n44b] [tag, default b] (gl44 block B's no-PGO gates; gates44a.sh's with V44):
# the imports against gl43's final build (g4_44.sh); the emulator, loop apart: <tag>1 the package as made (gl44's cfg, MGS2_BBOX=1,
# hashref_gl44) and <tag>3 with the XP check in every plan (cfg_emu/mgs2_gl44_xpchk.cfg), side by side; each one's state hashes against
# hashref_gl44 (em44.sh's refcmp), its digests and content by kind against the twin's s44 arm (tools/k0cmp44.py), its turns against
# tools/sched44.py (tools/turnchk44.py: the OPT2 word, B's and V44's turn lines), the heap, textures and XP lines; rdbbox.py on <tag>1's
# bbox.txt.  Game folders deleted after use (literal paths).
R=/home/claude/main/build_gl44; cd $R || exit 1; F=${1:-n44b}; K=${2:-pk_n44b}; G=${3:-b}
sh g4_44.sh $R/$F > $R/g4_44_$F.out 2>&1; cat $R/g4_44_$F.out | cut -c1-300
del() { [ -d "/home/claude/main/build_gl44/ms/em_$1/.ppsspp" ] && case $1 in
  b1) rm -rf "/home/claude/main/build_gl44/ms/em_b1" ;; b3) rm -rf "/home/claude/main/build_gl44/ms/em_b3" ;; esac; }
( PKG=$R/$K sh em44.sh ${G}1 > $R/em_${G}1.out 2>&1; del ${G}1 ) &
( PKG=$R/$K sh em44.sh ${G}3 $R/cfg_emu/mgs2_gl44_xpchk.cfg > $R/em_${G}3.out 2>&1; del ${G}3 ) &
wait
ls $R/ms 2>/dev/null
for t in ${G}1 ${G}3; do echo "== em_$t"; cat $R/em_$t.out | cut -c1-200
  echo "-- $t's content and digests by kind against the twin's s44 arm"; python3 -I $R/tools/k0cmp44.py $R/tw/x86/out_s44.log.gz $R/emu_$t/log.txt
  echo "-- $t's turns against sched44"; python3 -I $R/tools/turnchk44.py $R/emu_$t/log.txt | tail -n 3
  echo "-- $t heap, textures, XP, black box"; grep -a "^memory (gl41)\|^picture heap (gl41)\|textures refused\|malloc can still\|headroom\|^gl44 \|me picture loop (gl41)\|me picture loop: no thread\|^nat: PROBLEMS\|NAT PROBLEMS\|XP check\|^gl40 XP totals" $R/emu_$t/log.txt | cut -c1-500; done
echo "== rdbbox.py on ${G}1's bbox.txt"; python3 -I $R/rd/rdbbox.py $R/emu_${G}1 > $R/rdbbox_${G}1.out 2>&1; echo "rdbbox rc $?" >> $R/rdbbox_${G}1.out; cat $R/rdbbox_${G}1.out | cut -c1-300
echo "gates44b.sh: done $(date -u +%H:%M:%S)"
