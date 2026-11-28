#!/bin/sh
# rt_order38d.sh (gl38, 18:50): a chained order search aimed at the windows the guard is judged on, with build_gl38/layout_sim38
# (= gl36's layout_sim36.c + -t0 and -wf: layout_sim38.c).  Start: lay/order_s<IN>.txt.  Objective: each training window by its misses
# under the start order, times the window's weight in lay/wf<OUT>.txt (the rain windows 4, the others 1).  Guard: no window more than
# 1 % above the start order, OR not above its cap in wf (0.98 x gl36's own window on gl36's traces [I: other traces of the same vblanks],
# for the windows where the start order is under gl36's).  -t0: the start temperature (gl36's 0.002 of the objective undoes a start
# that is already a search's result: the stopped search lay/ls_s3_stopped.out was at -0.2 % after 18,000 of 60,000 moves).
# Usage: IN=<n> OUT=<m> [OPT=60000] [T0=0.0005] sh rt_order38d.sh -> lay/order_s<m>.txt, lay/ls_s<m>.out, ord<m>/final/*.ld
R=/home/claude/main/build_gl38; cd $R/lay || exit 1; LS=$R/layout_sim38; IN=${IN:?}; OUT=${OUT:?}
[ -s order_s$IN.txt ] || { echo "no lay/order_s$IN.txt"; exit 1; }; [ -s wf$OUT.txt ] || { echo "no lay/wf$OUT.txt"; exit 1; }
[ -e $R/ord$OUT ] && { echo "ord$OUT exists"; exit 1; }; mkdir -p $R/ord$OUT
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
B0=$(awk '{ print $2 }' bases.txt); B1=$(awk '{ print $4 }' bases.txt)
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
$LS $L -inall -norm -guard 10 -wf wf$OUT.txt -t0 ${T0:-0.0005} -opt ${OPT:-60000} -j ${JS:-4} -seed ${SEED:-2} -in $R/lay/order_s$IN.txt -cmp $R/lay/gl36_n2.txt -out order_s$OUT.txt > ls_s$OUT.out 2>&1; echo "order_s$OUT rc $?"
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
echo "rt_order38d.sh: done $(date -u +%H:%M:%S)"
