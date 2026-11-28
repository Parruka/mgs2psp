#!/bin/sh
# model42.sh <psp dir> <batch ...> (gl42's scoring [C model], as build_gl41/model41.sh but ONE RUN AT A TIME, nice 19: PspProf runs on
# PPSSPPHeadless_fcr (pgo.sh's default) with PSPPROF_DHIST=64; a window is vblanks 10-90 of a turn; each game folder deleted after its
# run (literal paths).  Turns: tools/sched42.py.  Batches:
#  a  ka42: the delivery cfg (cfg/mgs2_gl42.cfg, hashref_gl42), rain wait: 29,710 t57 K-N1 | 29,810 t58 K-A | 29,910 t59 K-N2 |
#           30,010 t60 K-A | 30,110 t61 K-N2R | 30,210 t62 K-A | 30,310 t63 K7 | 30,410 t64 K-A
#     kb42: heavier rain wait and walk: 30,510 t65 K-N2 | 30,610 t66 K-A | 30,710 t67 K-N2R | 30,810 t68 K-A walk | 30,910 t69 K-N2 walk |
#           31,010 t70 K-A walk | 31,110 t71 K-N2R walk | 31,210 t72 K-A walk
#     gd42: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg, hashref_gl38): 16,110 21,110 23,110 27,510
#  c  kc42: steady deck: 27,010 t30 K-A | 27,110 t31 K-N2 | 27,210 t32 K-A | 27,310 t33 K-N2R | 27,410 t34 K-A | 27,510 t35 K-N1 |
#           27,610 t36 K-A | 27,910 t39 K7
#  g  gd42 alone.  r  ka42 + kb42.  X=<prefix> gives the tags a prefix (u, s1, s2, pa .. pd; only the tags listed in mr's case are deleted).
R=/home/claude/main/build_gl42; P=${1:-$R/psp}; [ $# -gt 0 ] && shift; B=${*:-a c}; cd $R/m || exit 1; H=$R/cfg/hashref_gl42.txt; H38=/home/claude/main/build_gl41/cfg/hashref_gl38.txt
G=/home/claude/main/build_gl38/cfg_emu/mgs2_m37_rc0.cfg; C=$R/cfg/mgs2_gl42.cfg
mr() { t=$X$1; pd=$2; c=$3; h=$4; shift 4; [ -e $R/ms/$t ] && { echo "model42.sh: $R/ms/$t exists"; return 1; }
  PSPPROF_DHIST=64 nice -n 19 sh scripts/pgo.sh model $pd $t $c $h "$@" > $R/pgo_$t.out 2>&1; echo "model42.sh: $t rc $? $(date -u +%H:%M:%S)"
  [ -d $R/ms/$t/.ppsspp ] || return 0
  case $t in
    ka42|kb42|kc42|gd42|uka42|ukb42|ukc42|ugd42|s1ka42|s1kb42|s1kc42|s1gd42|s2ka42|s2kb42|s2kc42|s2gd42|paka42|pakb42|pakc42|pagd42|pbka42|pbkb42|pbkc42|pbgd42|pcka42|pckb42|pckc42|pcgd42|pdka42|pdkb42|pdkc42|pdgd42)
      rm -rf "/home/claude/main/build_gl42/ms/$t" ;;
  esac; }
for b in $B; do case $b in
  a) mr ka42 $P $C $H 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30210:30290 30310:30390 30410:30490
     mr kb42 $P $C $H 30510:30590 30610:30690 30710:30790 30810:30890 30910:30990 31010:31090 31110:31190 31210:31290
     mr gd42 $P $G $H38 16110:16190 21110:21190 23110:23190 27510:27590 ;;
  c) mr kc42 $P $C $H 27010:27090 27110:27190 27210:27290 27310:27390 27410:27490 27510:27590 27610:27690 27910:27990 ;;
  g) mr gd42 $P $G $H38 16110:16190 21110:21190 23110:23190 27510:27590 ;;
  r) mr ka42 $P $C $H 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30210:30290 30310:30390 30410:30490
     mr kb42 $P $C $H 30510:30590 30610:30690 30710:30790 30810:30890 30910:30990 31010:31090 31110:31190 31210:31290 ;;
esac; echo "model42.sh: batch $b done $(date -u +%H:%M:%S)"; done
