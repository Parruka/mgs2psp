#!/bin/sh
# model43.sh <psp dir> <batch ...> (gl43's scoring [C model], build_gl42/model42.sh's method, ONE RUN AT A TIME, nice 19: PspProf runs on
# PPSSPPHeadless_fcr with PSPPROF_DHIST=64; a window is vblanks 10-90 of a turn; each game folder deleted after its run (literal paths)).
# Turns: tools/sched43.py.  Batches:
#  a  ka43 (the delivery cfg cfg/mgs2_gl43.cfg, hashref_gl43): 29,010 t50 K-A | 29,110 t51 K-B | 29,210 t52 K-A | 29,310 t53 K-B |
#           29,410 t54 K-A | 29,610 t56 K-N2 | 29,710 t57 K-N2B | 29,810 t58 K-N2
#     kb43: 29,910 t59 K-N2B | 30,010 t60 K-N2 | 30,110 t61 K-A | 30,210 t62 K-B | 30,310 t63 K-A | 30,410 t64 K7 | 30,510 t65 K7B | 30,610 t66 K7
#     kw43 (walk): 30,710 t67 K-A | 30,810 t68 K-A | 30,910 t69 K-B | 31,010 t70 K-A | 31,110 t71 K-N2 | 31,210 t72 K-N2B
#  g  gd43: gl38's guard cfg with B out of its OPT2 words (cfg_emu/mgs2_m37_rc0_b43off.cfg; hashref_gl38): 16,110 21,110 23,110 27,510
R=/home/claude/main/build_gl43; P=${1:-$R/psp}; [ $# -gt 0 ] && shift; B=${*:-a g}; cd $R/m || exit 1; H=$R/cfg/hashref_gl43.txt; H38=/home/claude/main/build_gl41/cfg/hashref_gl38.txt
G=/home/claude/main/build_gl43/cfg_emu/mgs2_m37_rc0_b43off.cfg; C=$R/cfg/mgs2_gl43.cfg
mr() { t=$X$1; pd=$2; c=$3; h=$4; shift 4; [ -e $R/ms/$t ] && { echo "model43.sh: $R/ms/$t exists"; return 1; }
  PSPPROF_DHIST=64 nice -n 19 sh scripts/pgo.sh model $pd $t $c $h "$@" > $R/pgo_$t.out 2>&1; echo "model43.sh: $t rc $? $(date -u +%H:%M:%S)"
  [ -d $R/ms/$t/.ppsspp ] || return 0
  case $t in
    ka43|kb43|kw43|gd43|uka43|ukb43|ukw43|ugd43) rm -rf "/home/claude/main/build_gl43/ms/$t" ;;
  esac; }
for b in $B; do case $b in
  a) mr ka43 $P $C $H 29010:29090 29110:29190 29210:29290 29310:29390 29410:29490 29610:29690 29710:29790 29810:29890
     mr kb43 $P $C $H 29910:29990 30010:30090 30110:30190 30210:30290 30310:30390 30410:30490 30510:30590 30610:30690
     mr kw43 $P $C $H 30710:30790 30810:30890 30910:30990 31010:31090 31110:31190 31210:31290 ;;
  g) mr gd43 $P $G $H38 16110:16190 21110:21190 23110:23190 27510:27590 ;;
esac; echo "model43.sh: batch $b done $(date -u +%H:%M:%S)"; done
