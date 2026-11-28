#!/bin/sh
# rt_order38b.sh (gl38, the SECOND order search, 17:45): rt_order38.sh's search again with the seed's alias fixed.
# What was wrong with the first: three symbols name one function (vupv_660a267e_0 = vupv_8baafbc3_0 = vupv_96936c0f_0, 3,460 bytes of
# the VU vertex program, hot in the rain).  gl36's order lists it as vupv_96936c0f_0; this build's function list gives the trace's
# fetches to vupv_8baafbc3_0.  The seed kept the old name in its slot (with -inall: a slot nothing fetches) and appended the new
# name as "newly hot" at the end, so the simulator placed the hot code at the end while the linker put the one section at the old
# name's slot: the search optimised a layout the link did not make (simulated 2,848,605 / 2,920,799 I-misses on the two traces, the
# relinked build's real 3,326,845 / 3,241,116), and its seed was no better than a packed order (the guard had nothing to hold).
# Now: an old name that is not hot but has a hot alias at the same address takes the hot name, in the old name's slot.
# The traces are rt_tr38.sh's (the same windows; lay/t0n.it, lay/t1n.it); the function lists and order_hot.txt are the first search's.
R=/home/claude/main/build_gl38; cd $R/lay || exit 1; D=$R/u; LS=/home/claude/main/build_gl36/layout_sim36; mkdir -p $R/ord2
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
B0=$(awk '{ print $2 }' bases.txt); B1=$(awk '{ print $4 }' bases.txt)
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl38/'
hot = [tuple(l.split()) for l in open(R + 'lay/order_hot.txt') if l.strip()]; hs = set(hot)
have = set(); A = {}; N = {}
for f, reg in (('lay/funcs_deck.txt', {'0': '0', '1': '1'}), ('lay/funcs_w01a.txt', {'0': '0', '1': '2'})):
    for l in open(R + f):
        w = l.split()
        if len(w) == 4 and w[0] in reg: have.add((reg[w[0]], w[3])); A.setdefault((reg[w[0]], w[1]), set()).add(w[3]); N[(reg[w[0]], w[3])] = w[1]
old = [tuple(l.split()) for l in open('/home/claude/main/build_gl36/lay/order_s1.txt') if l.strip()]
keep = []; sub = []
for x in old:
    if x not in have: continue
    if x not in hs:
        al = [(x[0], n) for n in sorted(A[(x[0], N[x])]) if (x[0], n) in hs]
        if al: sub.append((x, al[0])); x = al[0]
    if x not in keep: keep.append(x)
ks = set(keep)
seed = {r: [n for (q, n) in keep if q == r] for r in '012'}
new = [x for x in hot if x not in ks]
for r, n in new: seed[r].append(n)
open(R + 'ord2/seed.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in seed[r]))
g36 = [tuple(dict(sub).get(x, x)) for x in old]
c = {r: [] for r in '012'}; cs = set()
for r, n in g36:
    if (r, n) in hs and (r, n) not in cs: c[r].append(n); cs.add((r, n))
for r in '012':
    for n in seed[r]:
        if (r, n) not in cs: c[r].append(n); cs.add((r, n))
open(R + 'lay/gl36_n2.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in c[r]))
print('seed 2 from gl36\'s last order: listed', len(old), 'kept', len(keep), 'alias names replaced', sub, 'not fetched now', len(ks - hs), 'newly hot appended', len(new))
PY
$LS $L -inall -norm -guard 10 -opt ${OPT:-60000} -j ${JS:-4} -seed 1 -in $R/ord2/seed.txt -cmp $R/lay/gl36_n2.txt -cmp $R/lay/order_s1.txt -out order_s2.txt > ls_s2.out 2>&1; echo "order_s2 rc $?"
grep -v '^it ' ls_s2.out | tail -n 30
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl38/'
L = [l.split() for l in open(R + 'lay/order_s2.txt') if l.strip()]
open(R + 'ord2/order_deck.txt', 'w').write(''.join('%s %s\n' % (r, n) for r, n in L if r in '01'))
open(R + 'ord2/order_w01a.txt', 'w').write(''.join('%s %s\n' % ('1' if r == '2' else r, n) for r, n in L if r in '02'))
PY
cd $R/ord2 || exit 1
for d in deck w01a; do mkdir -p $d; for o in $R/u/*.o; do ln -sf $o $d/; done; done
python3 -I $R/m/tools/layout_order.py order_deck.txt $R/ord2/deck w00a > lo_deck.out 2>&1; tail -n 2 lo_deck.out
python3 -I $R/m/tools/layout_order.py order_w01a.txt $R/ord2/w01a w01a > lo_w01a.out 2>&1; tail -n 2 lo_w01a.out
mkdir -p final; cmp deck/layout_elf.ld w01a/layout_elf.ld && echo "layout_elf.ld equal for both routes"
cp deck/layout_elf.ld deck/layout_w00a.ld w01a/layout_w01a.ld final/ && md5sum final/*.ld
