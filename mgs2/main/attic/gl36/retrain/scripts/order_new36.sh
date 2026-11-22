#!/bin/sh
# order_new36.sh (gl36, after tr_new36.sh): the new order objective (look b's plan) tested on u's traces with layout_sim36
# (build_gl36/layout_sim36.c: tools/layout_sim.c + -norm, -guard, -cmp and the per-window table).
#  The deck route's trace: t0's 8 windows + t2's 2, as 16,001 16,101 26,501 27,501 27,801 29,601 29,801 30,101 31,101 trained and
#  17,501 held out (-test 1: the opening's second ME-turn window); the d00t/d01t windows' arena fetches moved up 16 MB as pgo2.sh does.
#  The inside route: t1 as pgo2.sh orders it (36,401 held out).
#  The new traces fetch one function the old ones did not (xc_held_pass) and not one they did (xm_on_call): lay/seed_n.txt and
#  lay/old_n.txt are ord/seed.txt and gl36's shipped order (lay_old/order_s1.txt) with xm_on_call dropped and xc_held_pass appended at
#  region 0's end (both treated alike).
#  The search: from lay/seed_n.txt (gl35's order for the functions still hot, the new ones appended: the old search's start), 40,000
#  moves, seed 1, each window normalised by its seed-order misses (-norm), no training window more than 1 % above its seed order
#  (-guard 10).  The table also evaluates gl36's shipped order (lay_old/order_s1.txt) and gl35's own order (build_gl35/lay/order_s1.txt
#  -- only if it lists the same hot functions; it does not, so it is not used) on the same windows.
R=/home/claude/main/build_gl36; cd $R/lay || exit 1; D=$R/u
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
B0=$(psp-nm $D/MGS2T.elf | awk '$3 == "_ftext" { print $1 }'); B1=$(printf %08x $((0x$A0 + 0x34)))
echo "bases $B0 $B1 (old run: $(cat $R/lay_old/bases.txt)); arena $A0 size $OS"
[ -f t0n.it ] && [ -f t1n.it ] || python3 -I -c "
import numpy as np
def wins(p, n):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == n + 1, (p, len(m)); return [a[m[k]:m[k + 1]] for k in range(n)]
lo, hi = 0x$A0, 0x$A0 + $OS
def mv(x, tag):
    x = x.copy(); s = (x[:, 0] >= lo) & (x[:, 0] < hi); x[s, 0] += 0x1000000; print(tag, 'arena fetch runs moved:', int(s.sum()), 'of', len(x)); return x
d = wins('t0.it', 8); e = wins('t2.it', 2)
d = [mv(d[k], 't0 window %d' % k) if k < 4 else d[k] for k in range(8)]; e = [mv(e[k], 't2 window %d' % k) for k in range(2)]
# 16001 16101 26501 27501 27801 29601 29801 30101 31101 | 17501
np.concatenate([d[0], e[0], d[2], e[1], d[3], d[4], d[5], d[6], d[7], d[1]]).tofile('t0n.it'); del d, e
w = wins('t1.it', 8); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1n.it'); del w
" > merge_n.out 2>&1 || { cat merge_n.out; exit 1; }
cat merge_n.out; [ -f t0n.it ] && [ -f t1n.it ] && rm -f t0.it t1.it t2.it   # (the merged copies hold every window)
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
echo "== the old objective's numbers on the new windows: gl36's shipped order and the seed, no search"
$R/layout_sim36 $L -in $R/lay/seed_n.txt -cmp $R/lay/old_n.txt > $R/order_new_eval.out 2>&1; echo "eval rc $?"
echo "== the new objective: search"
nice -n 10 $R/layout_sim36 $L -norm -guard 10 -opt ${OPT:-40000} -j ${JS:-4} -seed 1 -in $R/lay/seed_n.txt -cmp $R/lay/old_n.txt -out $R/lay/order_n1.txt > $R/order_new_s1.out 2>&1
echo "search rc $? $(date -u +%H:%M:%S)"; grep -v '^it ' $R/order_new_s1.out | tail -40
