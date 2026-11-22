#!/bin/sh
# relink_order.sh <order> <build dir>: applies a tools/layout_sim.c order (lines "region name": 0 = main ELF, 1 = w00a overlay, 2 = w01a
# overlay, from one `-and` search over both routes) to a PGO=use LAYOUT=1 build: per-route order files, layout_elf.ld / layout_<stage>.ld
# through tools/layout_order.py (both routes must give the same ELF order), then a relink with every object kept (rebuild_use.sh with no
# object; W, TITLE, VP as there).  Check the result with `layout_sim -new` on the relinked functions.
O=$1; B=$2; T=/mnt/project-files/mgs2/tools
awk '$1==0 || $1==1' $O > $B/order_deck.txt
awk '$1==0; $1==2 {print 1, $2}' $O > $B/order_w01a.txt
cd $B || exit 1
python3 $T/layout_order.py $B/order_deck.txt $B w00a > lo_deck.log 2>&1; cp layout_elf.ld layout_elf.ld.deck
python3 $T/layout_order.py $B/order_w01a.txt $B w01a > lo_w01a.log 2>&1
cmp layout_elf.ld layout_elf.ld.deck || echo "ELF orders differ"
sh $T/dev/rebuild_use.sh $B
cat lo_deck.log lo_w01a.log
