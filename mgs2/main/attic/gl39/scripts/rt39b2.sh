#!/bin/sh
# rt39b2.sh (gl39): the order search's second stage.  rt39b.sh's weight file wf1.txt named the rain windows 5..10; the windows count
# from 0 (t0n.it: 0 16,001 | 1 16,101 | 2 26,901 | 3 27,101 | 4 29,601 | 5 29,701 | 6 30,201 | 7 30,601 | 8 30,901 | 9 31,001 | 10 the
# test 17,501), so stage 1 weighted 5..9 by 4 and left 4 (29,601, the digest-off K0 turn t56) at 1 (line 10 named no training window).
# Stage 2: start = lay/order_s1.txt, wf2.txt = windows 4..9 weighted 4, the same guard (no training window more than 1 % over the
# start), start temperature 0.0005, 20,000 moves, seed 2 -> lay/order_s2.txt; then rt39b.sh's tail (layout files into ord/final).
R=/home/claude/main/build_gl39; cd $R/lay || exit 1; D=$R/u; LS=/home/claude/main/build_gl38/layout_sim38
[ -s order_s1.txt ] || { echo "no lay/order_s1.txt"; exit 1; }; [ -e $R/ord/final_s1 ] && { echo "ord/final_s1 exists"; exit 1; }
B0=$(awk '{ print $2 }' bases.txt); B1=$(awk '{ print $4 }' bases.txt)
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
printf '1 4 4 0\n1 5 4 0\n1 6 4 0\n1 7 4 0\n1 8 4 0\n1 9 4 0\n' > wf2.txt
$LS $L -inall -norm -guard 10 -wf wf2.txt -t0 0.0005 -opt 20000 -j 4 -seed 2 -in $R/lay/order_s1.txt -out order_s2.txt > ls_s2.out 2>&1; echo "order_s2 rc $?"
grep -v '^it ' ls_s2.out | tail -n 34
mv $R/ord/final $R/ord/final_s1 && mv $R/ord/order_deck.txt $R/ord/order_deck_s1.txt && mv $R/ord/order_w01a.txt $R/ord/order_w01a_s1.txt || exit 1
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl39/'
L = [l.split() for l in open(R + 'lay/order_s2.txt') if l.strip()]
open(R + 'ord/order_deck.txt', 'w').write(''.join('%s %s\n' % (r, n) for r, n in L if r in '01'))
open(R + 'ord/order_w01a.txt', 'w').write(''.join('%s %s\n' % ('1' if r == '2' else r, n) for r, n in L if r in '02'))
PY
cd $R/ord || exit 1
python3 -I $R/m/tools/layout_order.py order_deck.txt $R/ord/deck w00a > lo_deck.out 2>&1; tail -n 2 lo_deck.out
python3 -I $R/m/tools/layout_order.py order_w01a.txt $R/ord/w01a w01a > lo_w01a.out 2>&1; tail -n 2 lo_w01a.out
mkdir -p final; cmp deck/layout_elf.ld w01a/layout_elf.ld && echo "layout_elf.ld equal for both routes"
cp deck/layout_elf.ld deck/layout_w00a.ld w01a/layout_w01a.ld final/ && md5sum final/*.ld
echo "rt39b2.sh: done $(date -u +%H:%M:%S)"
