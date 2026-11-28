#!/bin/sh
# model38.sh [psp dir] [batch ...] (gl38's scoring [C model]; PspProf runs of the final build, default build_gl38/psp; pgo.sh model
# takes the shots out of the cfg; up to 8 windows a run; three runs at a time; each game folder deleted after its run, literal path).
#  batch g (the retrain's guard, gl36's own arms and windows, no bound turns: the same cfg files as gl36's / gl37's model runs):
#    g38c    cull 1 reuse 0 (cfg_emu/mgs2_m36_c.cfg), the four rain windows                    against build_gl36/model/g36c
#    g38rc   cull 1 reuse 1 with gl36's every-run rule (cfg_emu/mgs2_m37_rc0.cfg), the same     against g36rc
#    g38rcd  the same cfg, the steady deck 27,410-27,490 / 27,510-27,590 and the opening 16,010-16,090 / 16,110-16,190 against g36rcc2, g36rcc1
#  batch k (each kind in its own turn of the gl38 cfg's schedule, as the PSP run will have them: cfg/mgs2_gl38.cfg; a window is
#    vblanks 10-90 of a turn, 40 pictures):
#    k38a  the rain wait's turns t51-t58 (29,100-29,800), k38b t59-t66 (29,900-30,600), k38c the steady deck t27-t29 (26,700 K0,
#          26,800 K5, 26,900 K7) and the walk t68-t72 (30,800-31,200); the kinds by turn: tools/sched38.py table
#  batch d: k38d the first deck scene t1-t8 (24,100-24,800): a cutscene whose content moves, indicative only
#  X=n in the environment: the tags get an n in front (the no-PGO build np2's runs: ng38c ..., nk38a ...).
R=/home/claude/main/build_gl38; P=${1:-$R/psp}; [ $# -gt 0 ] && shift; B=${*:-g k}; cd $R/m || exit 1; H=$R/cfg/hashref_gl38.txt
W4="29810:29890 29910:29990 31010:31090 31110:31190"
ws() { a=$1; n=$2; o=; i=0; while [ $i -lt $n ]; do o="$o $((a + 10 + 100 * i)):$((a + 90 + 100 * i))"; i=$((i + 1)); done; echo $o; }
mr() { t=$X$1; c=$2; shift 2; nice -n 10 sh scripts/pgo.sh model $P $t $c $H "$@" > $R/pgo_$t.out 2>&1
  case $t in g38c) rm -rf "/home/claude/main/build_gl38/ms/g38c" ;; g38rc) rm -rf "/home/claude/main/build_gl38/ms/g38rc" ;; g38rcd) rm -rf "/home/claude/main/build_gl38/ms/g38rcd" ;;
    k38a) rm -rf "/home/claude/main/build_gl38/ms/k38a" ;; k38b) rm -rf "/home/claude/main/build_gl38/ms/k38b" ;; k38c) rm -rf "/home/claude/main/build_gl38/ms/k38c" ;; k38d) rm -rf "/home/claude/main/build_gl38/ms/k38d" ;;
    ng38c) rm -rf "/home/claude/main/build_gl38/ms/ng38c" ;; ng38rc) rm -rf "/home/claude/main/build_gl38/ms/ng38rc" ;; ng38rcd) rm -rf "/home/claude/main/build_gl38/ms/ng38rcd" ;;
    nk38a) rm -rf "/home/claude/main/build_gl38/ms/nk38a" ;; nk38b) rm -rf "/home/claude/main/build_gl38/ms/nk38b" ;; nk38c) rm -rf "/home/claude/main/build_gl38/ms/nk38c" ;; nk38d) rm -rf "/home/claude/main/build_gl38/ms/nk38d" ;; esac; }
for b in $B; do case $b in
  g) mr g38c $R/cfg_emu/mgs2_m36_c.cfg $W4 & mr g38rc $R/cfg_emu/mgs2_m37_rc0.cfg $W4 & mr g38rcd $R/cfg_emu/mgs2_m37_rc0.cfg 16010:16090 16110:16190 27410:27490 27510:27590 & wait ;;
  k) mr k38a $R/cfg/mgs2_gl38.cfg $(ws 29100 8) & mr k38b $R/cfg/mgs2_gl38.cfg $(ws 29900 8) & mr k38c $R/cfg/mgs2_gl38.cfg $(ws 26700 3) $(ws 30800 5) & wait ;;
  d) mr k38d $R/cfg/mgs2_gl38.cfg $(ws 24100 8) ;;
esac; echo "model38.sh: batch $b done $(date -u +%H:%M:%S)"; done
