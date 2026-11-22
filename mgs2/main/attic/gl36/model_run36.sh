#!/bin/sh
# model_run36.sh (gl36 step C, lead 03:05): model36.sh on build_gl36/psp under nice 10, then the not-drawn arm g36n4, each run's game
# folder deleted as the run ends (literal paths), then score36.sh.
R=/home/claude/main/build_gl36; cd $R || exit 1
cl() { for t in "$@"; do [ -d $R/ms/$t ] && rm -rf "$R/ms/$t"; done; }
nice -n 10 sh model36.sh > $R/model36.out 2>&1; cl g36p g36r g36pc2 g36rc2 g36pc1 g36rc1
cd $R/m && nice -n 10 sh scripts/pgo.sh model $R/psp g36n4 $R/cfg_emu/mgs2_m36_n4.cfg $R/cfg/hashref_gl36.txt 29810:29890 29910:29990 31010:31090 31110:31190 > $R/pgo_g36n4.out 2>&1; cl g36n4
cd $R && sh score36.sh > $R/score36.out 2>&1
echo "model_run36.sh: done $(date -u +%H:%M:%S)"
