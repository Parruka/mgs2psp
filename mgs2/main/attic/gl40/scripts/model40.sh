#!/bin/sh
# model40.sh <psp dir> <batch ...> (gl40's scoring [C model], as build_gl39/model39.sh: PspProf runs on PPSSPPHeadless_fcr (pgo.sh's
# default) with PSPPROF_DHIST=64; a window is vblanks 10-90 of a turn; up to three runs at a time; each game folder deleted after its
# run, literal path).  Batches:
#  a  ka40: the delivery cfg (cfg/mgs2_gl40.cfg): 29,510 t55 K0 XP7 digests on | 29,610 t56 K0 XP0 | 29,710 t57 K0 XP3 | 29,810 t58
#           K7 M5 on | 29,910 t59 K0 XP1 | 30,010 t60 K0 XP3 | 30,110 t61 K0 XP0 | 30,410 t64 K7 M5 off
#     kb40: the delivery cfg: 30,210 t62 K8 no load | 30,310 t63 K0 XP3 | 30,510 t65 K0 XP0 digests on M5 off | 30,610 t66 K0 XP3 |
#           30,710 t67 K0 XP0 | 30,910 t69 walk K0 XP0 | 31,010 t70 walk K0 XP3 | 31,110 t71 walk K7
#     gd40: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg): 16,110 21,110 23,110 27,510 (against exp40/model x36, x39)
#  b  dn40: gl38's guard cfg without turns' kinds (build_gl38/cfg_emu/mgs2_m36_c.cfg): 29,810 an ME turn, 29,910 a main-drawing turn
#           (against build_gl38/model/d36, d38 and build_gl39/model/dn39)
#  g  gd40 alone (a pad candidate).  k  kb40 alone.  r  ka40 alone (a pad candidate's rain windows).  X=<prefix> gives the tags a prefix (pa .. pf for pad candidates, u for the use build;
#     only the tags listed in mr's case are deleted).
R=/home/claude/main/build_gl40; P=${1:-$R/psp}; [ $# -gt 0 ] && shift; B=${*:-a b}; cd $R/m || exit 1; H=$R/cfg/hashref_gl38.txt
G=/home/claude/main/build_gl38/cfg_emu/mgs2_m37_rc0.cfg
mr() { t=$X$1; pd=$2; c=$3; shift 3; [ -e $R/ms/$t ] && { echo "model40.sh: $R/ms/$t exists"; return 1; }
  PSPPROF_DHIST=64 nice -n 10 sh scripts/pgo.sh model $pd $t $c $H "$@" > $R/pgo_$t.out 2>&1
  case $t in ka40) rm -rf "/home/claude/main/build_gl40/ms/ka40" ;; kb40) rm -rf "/home/claude/main/build_gl40/ms/kb40" ;;
    gd40) rm -rf "/home/claude/main/build_gl40/ms/gd40" ;; dn40) rm -rf "/home/claude/main/build_gl40/ms/dn40" ;;
    pagd40) rm -rf "/home/claude/main/build_gl40/ms/pagd40" ;; pbgd40) rm -rf "/home/claude/main/build_gl40/ms/pbgd40" ;;
    pcgd40) rm -rf "/home/claude/main/build_gl40/ms/pcgd40" ;; pdgd40) rm -rf "/home/claude/main/build_gl40/ms/pdgd40" ;;
    pegd40) rm -rf "/home/claude/main/build_gl40/ms/pegd40" ;; pfgd40) rm -rf "/home/claude/main/build_gl40/ms/pfgd40" ;;
    uka40) rm -rf "/home/claude/main/build_gl40/ms/uka40" ;; ukb40) rm -rf "/home/claude/main/build_gl40/ms/ukb40" ;;
    paka40) rm -rf "/home/claude/main/build_gl40/ms/paka40" ;; pcka40) rm -rf "/home/claude/main/build_gl40/ms/pcka40" ;; pfka40) rm -rf "/home/claude/main/build_gl40/ms/pfka40" ;; esac; }
for b in $B; do case $b in
  a) mr ka40 $P $R/cfg/mgs2_gl40.cfg 29510:29590 29610:29690 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30410:30490 &
     mr kb40 $P $R/cfg/mgs2_gl40.cfg 30210:30290 30310:30390 30510:30590 30610:30690 30710:30790 30910:30990 31010:31090 31110:31190 &
     mr gd40 $P $G 16110:16190 21110:21190 23110:23190 27510:27590 & wait ;;
  b) mr dn40 $P /home/claude/main/build_gl38/cfg_emu/mgs2_m36_c.cfg 29810:29890 29910:29990 ;;
  g) mr gd40 $P $G 16110:16190 21110:21190 23110:23190 27510:27590 ;;
  k) mr kb40 $P $R/cfg/mgs2_gl40.cfg 30210:30290 30310:30390 30510:30590 30610:30690 30710:30790 30910:30990 31010:31090 31110:31190 ;;
  r) mr ka40 $P $R/cfg/mgs2_gl40.cfg 29510:29590 29610:29690 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30410:30490 ;;
esac; echo "model40.sh: batch $b done $(date -u +%H:%M:%S)"; done
