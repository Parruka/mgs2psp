#!/bin/sh
# order34.sh (gl34; gl33's order33.sh = gl20's order step, build_gl20's round 2): the code order from the traces lay/t0.it and lay/t1.it of the use build u.
#  1. m/scripts/pgo.sh order with one move (OPT=1, seed "h"): the function lists, the merged traces, and the list of the functions the
#     traces fetch ("hot"), by region (0 the main ELF, 1 w00a's overlay, 2 w01a's), in lay/order_sh.txt
#  2. the search's start: gl33's final order (build_gl33/lay/order_s1.txt; gl33 started from gl20's) for the functions still hot, then the newly hot ones in
#     order_sh.txt's order at each region's end: ord/seed.txt
#  3. the search: IN=ord/seed.txt, 40,000 moves (OPT), 4 threads, seed 1: lay/order_s1.txt
#  4. tools/layout_order.py per route on u's objects (links): ord/final/layout_elf.ld, layout_w00a.ld, layout_w01a.ld
R=/home/claude/main/build_gl34; cd $R/m || exit 1; mkdir -p $R/ord
OPT=1 JS=4 sh scripts/pgo2.sh order $R/u h > $R/order_h.out 2>&1; cat $R/order_h.out | tail -3
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl34/'
hot = [l.split() for l in open(R + 'lay/order_sh.txt') if l.strip()]
old = [l.split() for l in open('/home/claude/main/build_gl33/lay/order_s1.txt') if l.strip()]
hs = set((r, n) for r, n in hot); seen = set(); out = {'0': [], '1': [], '2': []}
for r, n in old:
    if (r, n) in hs and (r, n) not in seen: out[r].append(n); seen.add((r, n))
new = {'0': 0, '1': 0, '2': 0}
for r, n in hot:
    if (r, n) not in seen: out[r].append(n); seen.add((r, n)); new[r] += 1
gone = {'0': 0, '1': 0, '2': 0}
for r, n in old:
    if (r, n) not in hs: gone[r] += 1
open(R + 'ord/seed.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in out[r]))
print('seed: hot functions by region %s; from gl33\'s order %s; newly hot (appended) %s; of gl33\'s order no longer hot %s' % (
    {r: len(out[r]) for r in '012'}, {r: len(out[r]) - new[r] for r in '012'}, new, gone))
PY
IN=$R/ord/seed.txt OPT=${OPT:-40000} JS=4 sh scripts/pgo2.sh order $R/u 1 > $R/order_s1.out 2>&1; cat $R/order_s1.out
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl34/'
L = [l.split() for l in open(R + 'lay/order_s1.txt') if l.strip()]
open(R + 'ord/order_deck.txt', 'w').write(''.join('%s %s\n' % (r, n) for r, n in L if r in '01'))
open(R + 'ord/order_w01a.txt', 'w').write(''.join('%s %s\n' % ('1' if r == '2' else r, n) for r, n in L if r in '02'))
PY
cd $R/ord || exit 1
for d in deck w01a; do mkdir -p $d; for o in $R/u/*.o; do ln -sf $o $d/; done; done
python3 -I $R/m/tools/layout_order.py order_deck.txt $R/ord/deck w00a > lo_deck.out 2>&1; tail -n 2 lo_deck.out
python3 -I $R/m/tools/layout_order.py order_w01a.txt $R/ord/w01a w01a > lo_w01a.out 2>&1; tail -n 2 lo_w01a.out
mkdir -p final; cmp deck/layout_elf.ld w01a/layout_elf.ld && echo "layout_elf.ld equal for both routes"
cp deck/layout_elf.ld deck/layout_w00a.ld w01a/layout_w01a.ld final/ && md5sum final/*.ld
