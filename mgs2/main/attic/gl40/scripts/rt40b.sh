#!/bin/sh
# rt40b.sh (gl40; build_gl39/rt39b.sh + rt39b2.sh's weights, with the main-drawing guard windows in the search and a guard that binds):
#  - merge: lay/t0n.it = window 0 16,001 | 1 16,101 | 2 26,901 | 3 27,101 | 4 29,601 (t56 K0 XP0) | 5 29,701 (t57 K0 XP3) |
#    6 29,801 (t58 K7) | 7 30,601 (t66 K0 XP3) | 8 30,901 (t69 walk XP0) | 9 31,001 (t70 walk XP3) | 10 31,101 (t71 walk K7) |
#    11..18 the guard cfg's 16,121 16,161 21,121 21,161 23,121 23,161 27,521 27,561 | 19 17,501 (test).  Windows before the rain
#    have another overlay in the arena: their arena fetch runs are moved away (as gl39).  lay/t1n.it = gl39's form.
#  - caps (the binding guard): for each guard window g of model window w: cap = u's PspProf I-misses in g (lay/t3[.N]) x 1.02 x
#    gl36's I-misses in w / u's I-misses in w (80-vblank model counts: exp40/model/x36*, model/mu*) [I: a 5-vblank window's misses
#    scale between orders as its 80-vblank window's do; the relinked build's own model run is the check].
#  - seed: gl39's FINAL order (build_gl39/lay/order_s2.txt) with gl39's alias rule, the newly hot appended.
#  - search: layout_sim40 -norm -guard 10 -bind 20 -wf: rain windows 4..10 weight 4, guard windows 11..18 weight 2 with their caps,
#    the rest 1; start temperature 0.0005, OPT (40,000) moves.  Layout files into ord/final; then rl40.sh, rt_new40.sh.
R=/home/claude/main/build_gl40; cd $R/lay || exit 1; D=$R/u; LS=$R/layout_sim40; mkdir -p $R/ord
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
B0=$(psp-nm $D/MGS2T.elf | awk '$3 == "_ftext" { print $1 }'); B1=$(printf %08x $((0x$A0 + 0x34))); echo "base0 $B0 base1 $B1" > bases.txt
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w00a.elf $D w00a > funcs_deck.txt 2> funcs_deck.err
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w01a.elf $D w01a > funcs_w01a.txt 2> funcs_w01a.err
[ -s t0n.it ] && [ -s t1n.it ] || { python3 -I -c "
import numpy as np
def wins(p, n):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == n + 1, (p, len(m)); return [a[m[k]:m[k + 1]] for k in range(n)]
lo, hi = 0x$A0, 0x$A0 + $OS
def mv(x, tag):
    x = x.copy(); s = (x[:, 0] >= lo) & (x[:, 0] < hi); x[s, 0] += 0x1000000; print(tag, 'arena fetch runs moved:', int(s.sum()), 'of', len(x)); return x
d = wins('t0.it', 8); e = wins('t2.it', 4); g = wins('t3.it', 8)
d = [mv(d[k], 't0 window %d' % k) if k < 4 else d[k] for k in range(8)]; e = [mv(e[0], 't2 window 0'), e[1], e[2], e[3]]
g = [mv(g[k], 't3 window %d' % k) for k in range(8)]
for k in range(8): print('t0 window', k, 'fetch runs', len(d[k]))
for k in range(4): print('t2 window', k, 'fetch runs', len(e[k]))
for k in range(8): print('t3 window', k, 'fetch runs', len(g[k]))
np.concatenate([d[0], e[0], d[2], d[3], d[4], d[5], d[6], d[7], e[1], e[2], e[3]] + g + [d[1]]).tofile('t0n.it'); del d, e, g
w = wins('t1.it', 8); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1n.it'); del w
" > merge_n.out 2>&1 || { cat merge_n.out; exit 1; }; cat merge_n.out; rm -f t0.it t1.it t2.it t3.it; }
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl40/'
def im(p): return int(open(p).readline().split()[4])
x36 = [im('/home/claude/main/exp40/model/x36' + s) for s in ('', '.1', '.2', '.3')]
mu = [im(R + 'model/mu' + s) for s in ('', '.1', '.2', '.3')]
g = [im(R + 'lay/t3' + ('' if k == 0 else '.%d' % k)) for k in range(8)]
out = ''.join('1 %d 4 0\n' % w for w in range(4, 11))
for k in range(8):
    w = k // 2; cap = int(g[k] * 1.02 * x36[w] / mu[w] + 0.5)
    out += '1 %d 2 %d\n' % (11 + k, cap)
    print('guard window %d (t0n window %d, model window %d): u %d, gl36/u %.4f, cap %d' % (k, 11 + k, w, g[k], x36[w] / mu[w], cap))
print('model I-misses gl36', x36, 'u', mu)
open(R + 'lay/wf1.txt', 'w').write(out)
PY
cat wf1.txt
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1"
$LS $L -opt 1 -j 4 -out order_hot.txt > ls_hot.out 2>&1 || exit 1   # the hot functions
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl40/'
hot = [tuple(l.split()) for l in open(R + 'lay/order_hot.txt') if l.strip()]; hs = set(hot)
have = set(); A = {}; N = {}
for f, reg in (('lay/funcs_deck.txt', {'0': '0', '1': '1'}), ('lay/funcs_w01a.txt', {'0': '0', '1': '2'})):
    for l in open(R + f):
        w = l.split()
        if len(w) == 4 and w[0] in reg: have.add((reg[w[0]], w[3])); A.setdefault((reg[w[0]], w[1]), set()).add(w[3]); N[(reg[w[0]], w[3])] = w[1]
old = [tuple(l.split()) for l in open('/home/claude/main/build_gl39/lay/order_s2.txt') if l.strip()]
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
open(R + 'ord/seed.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in seed[r]))
dup = [(k, sorted(n for n in v if (k[0], n) in set((r, m) for r in '012' for m in seed[r]))) for k, v in A.items()]
dup = [d for d in dup if len(d[1]) > 1]
print('seed from gl39\'s final order: listed', len(old), 'kept', len(keep), 'alias names replaced', sub, 'not fetched now', len(ks - hs), 'newly hot appended', len(new), new[:40])
print('addresses with more than one name in the seed:', dup[:10])
PY
$LS $L -inall -norm -guard 10 -bind ${BK:-20} -wf wf1.txt -t0 ${T0:-0.0005} -opt ${OPT:-40000} -j ${JS:-4} -seed 1 -in $R/ord/seed.txt -out order_s1.txt > ls_s1.out 2>&1; echo "order_s1 rc $?"
grep -v '^it ' ls_s1.out | tail -n 50
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl40/'
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
echo "rt40b.sh: done $(date -u +%H:%M:%S)"
