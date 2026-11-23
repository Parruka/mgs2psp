#!/bin/sh
# model39.sh <psp dir> <batch ...> (gl39's scoring [C model]; PspProf runs with PSPPROF_DHIST=64 (D-misses by 64-byte data line ->
# model/<tag>[.n].dhist); m/scripts/pgo.sh model takes the shots out of the cfg; a window is vblanks 10-90 of a turn, 40 pictures; three
# runs at a time; each game folder deleted after its run, literal path).  Batches:
#  a  ka39: the delivery cfg (cfg/mgs2_gl39.cfg): 29,510 t55 K0 digests on | 29,610 t56 K0 digest-off | 29,710 t57 K7 | 29,810 t58 K8
#           no load | 30,210 t62 K0 digest-off | 30,410 t64 K7 M5 off | 30,910 t69 walk K0 digest-off | 31,010 t70 walk K7
#     km39: M5 off in every turn (cfg_emu/mgs2_gl39_m5off.cfg): 29,610 29,710 30,210 31,010      (M5 = ka39 - km39, the same vblanks)
#     kf39: F1 off in every turn (cfg_emu/mgs2_gl39_f1off.cfg): 29,610 30,210 30,910 (F1 = ka39 - kf39) and 30,710 t67 K7 with digests on
#           (the same vblanks, kind, digests, F1 and re-use as gl38's t67: the K7 guard against k38x)
#  b  kd39: digests on in every turn (cfg_emu/mgs2_gl39_digon.cfg): 29,610 30,210 30,910          (the digest = kd39 - ka39)
#     dn39: gl38's guard cfg without turns' kinds (build_gl38/cfg_emu/mgs2_m36_c.cfg): 29,810 an ME turn, 29,910 a main-drawing turn
#           (against build_gl38/model/d38 and d36: the same cfg and windows on gl38's and gl36's final builds)
#     k38x: gl38's final build (build_gl38/psp; P is not used) with gl38's own cfg and DHIST: 29,810 t58 K0 | 30,010 t60 K0 | 30,510 t65
#           K7 | 30,710 t67 K7 | 30,910 t69 walk K7 | 30,810 t68 walk K0           (gl38's K0 / K7 turns for the guard)
#  X=<prefix> in the environment: the tags get it in front (another build's runs: only the tags of the case below can be deleted).
R=/home/claude/main/build_gl39; P=${1:-$R/psp}; [ $# -gt 0 ] && shift; B=${*:-a b}; cd $R/m || exit 1; H=$R/cfg/hashref_gl38.txt
mr() { t=$X$1; pd=$2; c=$3; shift 3; PSPPROF_DHIST=64 nice -n 10 sh scripts/pgo.sh model $pd $t $c $H "$@" > $R/pgo_$t.out 2>&1
  case $t in ka39) rm -rf "/home/claude/main/build_gl39/ms/ka39" ;; km39) rm -rf "/home/claude/main/build_gl39/ms/km39" ;; kf39) rm -rf "/home/claude/main/build_gl39/ms/kf39" ;;
    kd39) rm -rf "/home/claude/main/build_gl39/ms/kd39" ;; dn39) rm -rf "/home/claude/main/build_gl39/ms/dn39" ;; k38x) rm -rf "/home/claude/main/build_gl39/ms/k38x" ;;
    nka39) rm -rf "/home/claude/main/build_gl39/ms/nka39" ;; ndn39) rm -rf "/home/claude/main/build_gl39/ms/ndn39" ;; hdn39) rm -rf "/home/claude/main/build_gl39/ms/hdn39" ;;
    hka39) rm -rf "/home/claude/main/build_gl39/ms/hka39" ;; uka39) rm -rf "/home/claude/main/build_gl39/ms/uka39" ;; udn39) rm -rf "/home/claude/main/build_gl39/ms/udn39" ;; esac; }
for b in $B; do case $b in
  a) mr ka39 $P $R/cfg/mgs2_gl39.cfg 29510:29590 29610:29690 29710:29790 29810:29890 30210:30290 30410:30490 30910:30990 31010:31090 &
     mr km39 $P $R/cfg_emu/mgs2_gl39_m5off.cfg 29610:29690 29710:29790 30210:30290 31010:31090 &
     mr kf39 $P $R/cfg_emu/mgs2_gl39_f1off.cfg 29610:29690 30210:30290 30710:30790 30910:30990 & wait ;;
  b) mr kd39 $P $R/cfg_emu/mgs2_gl39_digon.cfg 29610:29690 30210:30290 30910:30990 &
     mr dn39 $P /home/claude/main/build_gl38/cfg_emu/mgs2_m36_c.cfg 29810:29890 29910:29990 &
     X= mr k38x /home/claude/main/build_gl38/psp /home/claude/main/build_gl38/cfg/mgs2_gl38.cfg 29810:29890 30010:30090 30510:30590 30710:30790 30810:30890 30910:30990 & wait ;;
  dn) mr dn39 $P /home/claude/main/build_gl38/cfg_emu/mgs2_m36_c.cfg 29810:29890 29910:29990 ;;
  ka) mr ka39 $P $R/cfg/mgs2_gl39.cfg 29510:29590 29610:29690 29710:29790 29810:29890 30210:30290 30410:30490 30910:30990 31010:31090 ;;
esac; echo "model39.sh: batch $b done $(date -u +%H:%M:%S)"; done
