#!/bin/sh
# model41.sh <psp dir> <batch ...> (gl41's scoring [C model], as build_gl40/model40.sh: PspProf runs on PPSSPPHeadless_fcr (pgo.sh's
# default) with PSPPROF_DHIST=64; a window is vblanks 10-90 of a turn; up to three runs at a time; each game folder deleted after its
# run (literal paths).  Turns: tools/sched41.py.  Batches:
#  a  ka41: the delivery cfg (cfg/mgs2_gl41.cfg), rain wait: 29,710 t57 P3-off | 29,810 t58 K-A | 29,910 t59 K-N1 | 30,010 t60 K-A |
#           30,110 t61 P1-off | 30,210 t62 K-A | 30,310 t63 P3-off | 30,410 t64 K7
#     kb41: the delivery cfg, heavier rain wait and walk: 30,510 t65 K-N1 | 30,610 t66 K-A | 30,710 t67 K-N1 | 30,810 t68 K-N1 walk |
#           30,910 t69 K-A walk | 31,010 t70 P1-off walk | 31,110 t71 K-A walk | 31,210 t72 P3-off walk
#     gd41: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg): 16,110 21,110 23,110 27,510 (against exp40/model x36, x39)
#  c  kc41: the delivery cfg, steady deck: 27,010 t30 K-A | 27,110 t31 K-N1 | 27,210 t32 K-A | 27,310 t33 P1-off | 27,410 t34 K-A |
#           27,510 t35 K-N1 | 27,710 t37 P3-off | 27,910 t39 K7
#     kd41: the delivery cfg, first rain turns and the digest turns: 29,110 t51 K-N1 | 29,210 t52 K-A | 29,310 t53 P1-off |
#           29,410 t54 K-A | 29,510 t55 K-A digests XP7 | 29,610 t56 K-N1 digests XP7 | 31,310 t73 K-N1 digests XP7
#  g  gd41 alone (a pad candidate).  r  ka41 + kb41 (an order's or a pad candidate's rain windows).  X=<prefix> gives the tags a prefix
#     (u, s1, s2, pa .. ph; only the tags listed in mr's case are deleted).
R=/home/claude/main/build_gl41; P=${1:-$R/psp}; [ $# -gt 0 ] && shift; B=${*:-a c}; cd $R/m || exit 1; H=$R/cfg/hashref_gl38.txt
G=/home/claude/main/build_gl38/cfg_emu/mgs2_m37_rc0.cfg; C=$R/cfg/mgs2_gl41.cfg
mr() { t=$X$1; pd=$2; c=$3; shift 3; [ -e $R/ms/$t ] && { echo "model41.sh: $R/ms/$t exists"; return 1; }
  PSPPROF_DHIST=64 nice -n 10 sh scripts/pgo.sh model $pd $t $c $H "$@" > $R/pgo_$t.out 2>&1
  [ -d $R/ms/$t/.ppsspp ] || return 0
  case $t in
    ka41) rm -rf "/home/claude/main/build_gl41/ms/ka41" ;;
    kb41) rm -rf "/home/claude/main/build_gl41/ms/kb41" ;;
    kc41) rm -rf "/home/claude/main/build_gl41/ms/kc41" ;;
    kd41) rm -rf "/home/claude/main/build_gl41/ms/kd41" ;;
    gd41) rm -rf "/home/claude/main/build_gl41/ms/gd41" ;;
    uka41) rm -rf "/home/claude/main/build_gl41/ms/uka41" ;;
    ukb41) rm -rf "/home/claude/main/build_gl41/ms/ukb41" ;;
    ukc41) rm -rf "/home/claude/main/build_gl41/ms/ukc41" ;;
    ukd41) rm -rf "/home/claude/main/build_gl41/ms/ukd41" ;;
    ugd41) rm -rf "/home/claude/main/build_gl41/ms/ugd41" ;;
    s1ka41) rm -rf "/home/claude/main/build_gl41/ms/s1ka41" ;;
    s1kb41) rm -rf "/home/claude/main/build_gl41/ms/s1kb41" ;;
    s1kc41) rm -rf "/home/claude/main/build_gl41/ms/s1kc41" ;;
    s1kd41) rm -rf "/home/claude/main/build_gl41/ms/s1kd41" ;;
    s1gd41) rm -rf "/home/claude/main/build_gl41/ms/s1gd41" ;;
    s2ka41) rm -rf "/home/claude/main/build_gl41/ms/s2ka41" ;;
    s2kb41) rm -rf "/home/claude/main/build_gl41/ms/s2kb41" ;;
    s2kc41) rm -rf "/home/claude/main/build_gl41/ms/s2kc41" ;;
    s2kd41) rm -rf "/home/claude/main/build_gl41/ms/s2kd41" ;;
    s2gd41) rm -rf "/home/claude/main/build_gl41/ms/s2gd41" ;;
    paka41) rm -rf "/home/claude/main/build_gl41/ms/paka41" ;;
    pakb41) rm -rf "/home/claude/main/build_gl41/ms/pakb41" ;;
    pakc41) rm -rf "/home/claude/main/build_gl41/ms/pakc41" ;;
    pakd41) rm -rf "/home/claude/main/build_gl41/ms/pakd41" ;;
    pagd41) rm -rf "/home/claude/main/build_gl41/ms/pagd41" ;;
    pbka41) rm -rf "/home/claude/main/build_gl41/ms/pbka41" ;;
    pbkb41) rm -rf "/home/claude/main/build_gl41/ms/pbkb41" ;;
    pbkc41) rm -rf "/home/claude/main/build_gl41/ms/pbkc41" ;;
    pbkd41) rm -rf "/home/claude/main/build_gl41/ms/pbkd41" ;;
    pbgd41) rm -rf "/home/claude/main/build_gl41/ms/pbgd41" ;;
    pcka41) rm -rf "/home/claude/main/build_gl41/ms/pcka41" ;;
    pckb41) rm -rf "/home/claude/main/build_gl41/ms/pckb41" ;;
    pckc41) rm -rf "/home/claude/main/build_gl41/ms/pckc41" ;;
    pckd41) rm -rf "/home/claude/main/build_gl41/ms/pckd41" ;;
    pcgd41) rm -rf "/home/claude/main/build_gl41/ms/pcgd41" ;;
    pdka41) rm -rf "/home/claude/main/build_gl41/ms/pdka41" ;;
    pdkb41) rm -rf "/home/claude/main/build_gl41/ms/pdkb41" ;;
    pdkc41) rm -rf "/home/claude/main/build_gl41/ms/pdkc41" ;;
    pdkd41) rm -rf "/home/claude/main/build_gl41/ms/pdkd41" ;;
    pdgd41) rm -rf "/home/claude/main/build_gl41/ms/pdgd41" ;;
    peka41) rm -rf "/home/claude/main/build_gl41/ms/peka41" ;;
    pekb41) rm -rf "/home/claude/main/build_gl41/ms/pekb41" ;;
    pekc41) rm -rf "/home/claude/main/build_gl41/ms/pekc41" ;;
    pekd41) rm -rf "/home/claude/main/build_gl41/ms/pekd41" ;;
    pegd41) rm -rf "/home/claude/main/build_gl41/ms/pegd41" ;;
    pfka41) rm -rf "/home/claude/main/build_gl41/ms/pfka41" ;;
    pfkb41) rm -rf "/home/claude/main/build_gl41/ms/pfkb41" ;;
    pfkc41) rm -rf "/home/claude/main/build_gl41/ms/pfkc41" ;;
    pfkd41) rm -rf "/home/claude/main/build_gl41/ms/pfkd41" ;;
    pfgd41) rm -rf "/home/claude/main/build_gl41/ms/pfgd41" ;;
    pgka41) rm -rf "/home/claude/main/build_gl41/ms/pgka41" ;;
    pgkb41) rm -rf "/home/claude/main/build_gl41/ms/pgkb41" ;;
    pgkc41) rm -rf "/home/claude/main/build_gl41/ms/pgkc41" ;;
    pgkd41) rm -rf "/home/claude/main/build_gl41/ms/pgkd41" ;;
    pggd41) rm -rf "/home/claude/main/build_gl41/ms/pggd41" ;;
    phka41) rm -rf "/home/claude/main/build_gl41/ms/phka41" ;;
    phkb41) rm -rf "/home/claude/main/build_gl41/ms/phkb41" ;;
    phkc41) rm -rf "/home/claude/main/build_gl41/ms/phkc41" ;;
    phkd41) rm -rf "/home/claude/main/build_gl41/ms/phkd41" ;;
    phgd41) rm -rf "/home/claude/main/build_gl41/ms/phgd41" ;;
  esac; }
for b in $B; do case $b in
  a) mr ka41 $P $C 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30210:30290 30310:30390 30410:30490 &
     mr kb41 $P $C 30510:30590 30610:30690 30710:30790 30810:30890 30910:30990 31010:31090 31110:31190 31210:31290 &
     mr gd41 $P $G 16110:16190 21110:21190 23110:23190 27510:27590 & wait ;;
  c) mr kc41 $P $C 27010:27090 27110:27190 27210:27290 27310:27390 27410:27490 27510:27590 27710:27790 27910:27990 &
     mr kd41 $P $C 29110:29190 29210:29290 29310:29390 29410:29490 29510:29590 29610:29690 31310:31390 & wait ;;
  g) mr gd41 $P $G 16110:16190 21110:21190 23110:23190 27510:27590 ;;
  r) mr ka41 $P $C 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30210:30290 30310:30390 30410:30490 &
     mr kb41 $P $C 30510:30590 30610:30690 30710:30790 30810:30890 30910:30990 31010:31090 31110:31190 31210:31290 & wait ;;
  gr) mr ka41 $P $C 29710:29790 29810:29890 29910:29990 30010:30090 30110:30190 30210:30290 30310:30390 30410:30490 &
     mr kb41 $P $C 30510:30590 30610:30690 30710:30790 30810:30890 30910:30990 31010:31090 31110:31190 31210:31290 &
     mr gd41 $P $G 16110:16190 21110:21190 23110:23190 27510:27590 & wait ;;
esac; echo "model41.sh: batch $b done $(date -u +%H:%M:%S)"; done
