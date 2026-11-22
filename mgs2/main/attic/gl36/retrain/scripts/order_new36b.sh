#!/bin/sh
# order_new36b.sh (after order_new36.sh): variant B of the new objective.  The search starts from gl36's own search order as the
# relinked build psp really has it (lay/old_k.txt = lay_old/order_s1.txt with xc_held_pass, newly hot, appended; -inall keeps
# xm_on_call, no longer fetched, in its slot: the start column then equals psp's real addresses window by window), each window
# normalised by ITS misses and guarded at 1 % against it (no window may end more than 1 % above gl36's order); same traces, windows,
# moves and seed as order_new36.sh.  The table also evaluates gl35's seed (lay/seed_k.txt).
R=/home/claude/main/build_gl36; cd $R/lay || exit 1
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 0880403c -base1 08c42034"
nice -n 10 $R/layout_sim36 $L -inall -norm -guard 10 -opt ${OPT:-40000} -j ${JS:-4} -seed 1 -in $R/lay/old_k.txt -cmp $R/lay/seed_k.txt -out $R/lay/order_n2.txt > $R/order_new_s2.out 2>&1
echo "search B rc $? $(date -u +%H:%M:%S)"; grep -v '^it ' $R/order_new_s2.out | tail -24
