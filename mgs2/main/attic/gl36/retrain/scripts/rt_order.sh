#!/bin/sh
# retrain/rt_order.sh (prepared, NOT RUN; called by rt_b.sh): the code order of the use build u with the new objective, from the traces
# lay/t0.it, t1.it, t2.it.  As order_new36.sh (tested 2026-11-22 on gl36's u) plus order36.sh's layout step.
R=${R:-/home/claude/main/build_gl36}; cd $R/lay || exit 1; D=$R/u; LS=$R/layout_sim36; mkdir -p $R/ord
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
B0=$(psp-nm $D/MGS2T.elf | awk '$3 == "_ftext" { print $1 }'); B1=$(printf %08x $((0x$A0 + 0x34))); echo "base0 $B0 base1 $B1" > bases.txt
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w00a.elf $D w00a > funcs_deck.txt 2> funcs_deck.err
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w01a.elf $D w01a > funcs_w01a.txt 2> funcs_w01a.err
python3 -I -c "
import numpy as np
def wins(p, n):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == n + 1, (p, len(m)); return [a[m[k]:m[k + 1]] for k in range(n)]
lo, hi = 0x$A0, 0x$A0 + $OS
def mv(x, tag):
    x = x.copy(); s = (x[:, 0] >= lo) & (x[:, 0] < hi); x[s, 0] += 0x1000000; print(tag, 'arena fetch runs moved:', int(s.sum()), 'of', len(x)); return x
d = wins('t0.it', 8); e = wins('t2.it', 2)
d = [mv(d[k], 't0 window %d' % k) if k < 4 else d[k] for k in range(8)]; e = [mv(e[k], 't2 window %d' % k) for k in range(2)]
np.concatenate([d[0], e[0], d[2], e[1], d[3], d[4], d[5], d[6], d[7], d[1]]).tofile('t0n.it'); del d, e   # 16001 16101 26501 27501 27801 29601 29801 30101 31101 | 17501
w = wins('t1.it', 8); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1n.it'); del w   # ... | 36401
" > merge_n.out 2>&1 || { cat merge_n.out; exit 1; }
cat merge_n.out; rm -f t0.it t1.it t2.it
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
$LS $L -opt 1 -j 4 -out order_hot.txt > ls_hot.out 2>&1 || exit 1   # the hot functions
python3 -I - <<'PY' || exit 1
# the seed (variant B, tested 2026-11-22: order_new36b.sh): gl36's last search order (lay_old/order_s1.txt) for every function it lists
# that the new lists still have (with -inall the ones no longer fetched keep their slots), the newly hot appended at each region's end
# in first-fetch order; the comparison column gl35's delivered order (hot ones), the rest of the seed's set appended so it lists the same set
R = '/home/claude/main/build_gl36/'
hot = [tuple(l.split()) for l in open(R + 'lay/order_hot.txt') if l.strip()]; hs = set(hot)
have = set()
for f, reg in (('lay/funcs_deck.txt', {'0': '0', '1': '1'}), ('lay/funcs_w01a.txt', {'0': '0', '1': '2'})):
    for l in open(R + f):
        w = l.split()
        if len(w) == 4 and w[0] in reg: have.add((reg[w[0]], w[3]))
old = [tuple(l.split()) for l in open(R + 'lay_old/order_s1.txt') if l.strip()]
keep = [x for x in old if x in have]; ks = set(keep)
seed = {r: [n for (q, n) in keep if q == r] for r in '012'}
for r, n in hot:
    if (r, n) not in ks: seed[r].append(n)
open(R + 'ord/seed.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in seed[r]))
sset = set((r, n) for r in '012' for n in seed[r])
g35 = [tuple(l.split()) for l in open('/home/claude/main/build_gl35/lay/order_s1.txt') if l.strip()]
c = {r: [] for r in '012'}; cs = set()
for r, n in g35:
    if (r, n) in hs and (r, n) not in cs: c[r].append(n); cs.add((r, n))
for r in '012':
    for n in seed[r]:
        if (r, n) not in cs: c[r].append(n); cs.add((r, n))
open(R + 'lay/gl35_n.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in c[r]))
print('seed from gl36\'s last order: listed', len(old), 'still in the lists', len(keep), 'of them not fetched now', len(ks - hs),
      'newly hot appended', len([x for x in hot if x not in ks]), '; gl35 order column:', len(cs), 'functions')
PY
$LS $L -inall -norm -guard 10 -opt ${OPT:-40000} -j ${JS:-4} -seed 1 -in $R/ord/seed.txt -cmp $R/lay/gl35_n.txt -out order_s1.txt > ls_s1.out 2>&1; echo "order_s1 rc $?"
grep -v '^it ' ls_s1.out | tail -n 30
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl36/'
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
