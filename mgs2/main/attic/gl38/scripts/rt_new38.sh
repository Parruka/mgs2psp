#!/bin/sh
# retrain/rt_new.sh (prepared, NOT RUN): the relinked build psp's real addresses on both routes' merged traces, window by window
# (layout_sim36 -new, one route at a time as pgo2.sh's 'new'), against the seed order and the search's order.
R=${R:-/home/claude/main/build_gl38}; cd $R/lay || exit 1; P=$R/psp; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
python3 -I $R/m/tools/layout_funcs.py $P/MGS2T.elf $P/ovl_w00a.elf $P w00a > funcs_deck_f.txt 2> funcs_deck_f.err
python3 -I $R/m/tools/layout_funcs.py $P/MGS2T.elf $P/ovl_w01a.elf $P w01a > funcs_w01a_f.txt 2> funcs_w01a_f.err
B0=$(awk '{ print $2 }' bases.txt); B1=$(awk '{ print $4 }' bases.txt)
/home/claude/main/build_gl36/layout_sim36 t0n.it funcs_deck.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1 -new funcs_deck_f.txt > new_f_0.log 2>&1
/home/claude/main/build_gl36/layout_sim36 t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1 -new funcs_w01a_f.txt > new_f_1.log 2>&1
tail -n 16 new_f_0.log; tail -n 12 new_f_1.log
