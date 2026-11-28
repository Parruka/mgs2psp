#!/bin/sh
# rt_order38c.sh (gl38, a CHAINED order search, 18:20): layout_sim36 again, started from the last search's own order (the way gl36's
# order came from gl35's): each window normalised by its misses under that order and guarded at 1 % against it.
# Why: gl36's order does not carry over to gl38 (109 of region 0's hot functions changed size, the overlays' base moved 24 KB, i.e.
# 8 KB in the 16 KB of a cache way), so gl38's seed evaluated like a packed order (8.1M I-misses against gl36's own 6.2M seed) and one
# search of 40,000-60,000 moves ends about 10 % over gl36's windows.
# Usage: IN=<n of lay/order_s<n>.txt> OUT=<m> [OPT=60000] sh rt_order38c.sh   -> lay/order_s<m>.txt, lay/ls_s<m>.out, ord<m>/final/*.ld
R=/home/claude/main/build_gl38; cd $R/lay || exit 1; LS=/home/claude/main/build_gl36/layout_sim36; IN=${IN:?}; OUT=${OUT:?}
[ -s order_s$IN.txt ] || { echo "no lay/order_s$IN.txt"; exit 1; }; [ -e $R/ord$OUT ] && { echo "ord$OUT exists"; exit 1; }; mkdir -p $R/ord$OUT
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
B0=$(awk '{ print $2 }' bases.txt); B1=$(awk '{ print $4 }' bases.txt)
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
$LS $L -inall -norm -guard 10 -opt ${OPT:-60000} -j ${JS:-4} -seed ${SEED:-2} -in $R/lay/order_s$IN.txt -cmp $R/lay/gl36_n2.txt -out order_s$OUT.txt > ls_s$OUT.out 2>&1; echo "order_s$OUT rc $?"
grep -v '^it ' ls_s$OUT.out | tail -n 30
O=$OUT python3 -I - <<'PY' || exit 1
import os
R = '/home/claude/main/build_gl38/'; o = os.environ['O']
L = [l.split() for l in open(R + 'lay/order_s%s.txt' % o) if l.strip()]
open(R + 'ord%s/order_deck.txt' % o, 'w').write(''.join('%s %s\n' % (r, n) for r, n in L if r in '01'))
open(R + 'ord%s/order_w01a.txt' % o, 'w').write(''.join('%s %s\n' % ('1' if r == '2' else r, n) for r, n in L if r in '02'))
PY
cd $R/ord$OUT || exit 1
for d in deck w01a; do mkdir -p $d; for o in $R/u/*.o; do ln -sf $o $d/; done; done
python3 -I $R/m/tools/layout_order.py order_deck.txt $R/ord$OUT/deck w00a > lo_deck.out 2>&1; tail -n 2 lo_deck.out
python3 -I $R/m/tools/layout_order.py order_w01a.txt $R/ord$OUT/w01a w01a > lo_w01a.out 2>&1; tail -n 2 lo_w01a.out
mkdir -p final; cmp deck/layout_elf.ld w01a/layout_elf.ld && echo "layout_elf.ld equal for both routes"
cp deck/layout_elf.ld deck/layout_w00a.ld w01a/layout_w01a.ld final/ && md5sum final/*.ld
echo "rt_order38c.sh: done $(date -u +%H:%M:%S)"
