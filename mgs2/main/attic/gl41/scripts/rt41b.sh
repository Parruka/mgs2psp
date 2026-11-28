#!/bin/sh
# rt41b.sh (gl41; build_gl40/rt40b.sh with gl41's traces and two stages):
#  - merge: lay/t0n.it = window 0 16,001 | 1 16,101 | 2 26,801 (t28 K-A deck) | 3 27,101 (t31 K-N1 deck) | 4 29,801 (t58 K-A rain) |
#    5 29,901 (t59 K-N1) | 6 30,401 (t64 K7) | 7 30,601 (t66 K-A heavier) | 8 30,801 (t68 K-N1 walk) | 9 30,901 (t69 K-A walk) |
#    10 31,101 (t71 K-A walk) | 11..18 the guard cfg's 16,121 16,161 21,121 21,161 23,121 23,161 27,521 27,561 | 19 17,501 (test).
#    Windows before the rain have another overlay in the arena: their arena fetch runs are moved away (as gl39-gl40).  t1n.it = gl40's form.
#  - caps (the binding guard), as gl40: cap = u's PspProf I-misses in the guard window (lay/t3[.N]) x 1.02 x gl36's I-misses in its
#    80-vblank model window / u's there (exp40/model/x36*, model/mu*) [I: a 5-vblank window's misses scale as its 80-vblank window's].
#  - seed: gl40's FINAL order (build_gl40/lay/order_s1.txt) with gl40's alias rule, and gl41s's rule for the clones P1 removed: a seed
#    name X.isra.N that no longer exists is replaced in place by X when X is fetched; the newly hot appended.
#  - P3's copy (vupv_96936c0f_0_p3) is not searched: layout_sim41 -pin places it after the hot list on the box cull form's sets two
#    lines in (attic/gl41s/results.md section 2); tools/pin41.py writes gl41s's two linker lines for it into each layout_elf.ld.
#  - stage 1 (s1) = gl40's search: layout_sim41 -norm -guard 10 -bind 20 -wf wf1: rain windows 4..10 weight 4, guard windows 11..18
#    weight 2 with their caps, the rest 1; start temperature 0.0005, OPT (40,000) moves, from the seed.
#  - stage 2 (s2, rain-weighted): from s1: rain windows 4..10 weight 12, guard windows weight 1 with the same binding caps, the rest 1;
#    -guard 10 against s1 (no window more than 1 % above s1's unless under its cap); start temperature 0.0002, OPT2 (30,000) moves.
#  Layout files: ord/s1 and ord/s2 (layout_elf.ld, layout_w00a.ld, layout_w01a.ld); the model picks one (rl41.sh, model41.sh).
R=/home/claude/main/build_gl41; cd $R/lay || exit 1; D=$R/u; LS=$R/layout_sim41; mkdir -p $R/ord
PIN=vupv_96936c0f_0_p3; PREF=vupv_8baafbc3_0; PN="-pin $PIN:$PREF:0x80"
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
B0=$(psp-nm $D/MGS2T.elf | awk '$3 == "_ftext" { print $1 }'); B1=$(printf %08x $((0x$A0 + 0x34))); echo "base0 $B0 base1 $B1" > bases.txt
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w00a.elf $D w00a > funcs_deck.txt 2> funcs_deck.err
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w01a.elf $D w01a > funcs_w01a.txt 2> funcs_w01a.err
grep -q " $PIN\$" funcs_deck.txt && grep -q " $PREF\$" funcs_deck.txt || { echo "rt41b.sh: $PIN or $PREF not in funcs_deck.txt"; exit 1; }
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
" > merge_n.out 2>&1 || { cat merge_n.out; exit 1; }; cat merge_n.out; }
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl41/'
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
open(R + 'lay/wf2.txt', 'w').write(''.join('1 %d 12 0\n' % w for w in range(4, 11)) + ''.join(
    '1 %s 1 %s\n' % tuple(l.split()[1::2]) for l in out.split('\n') if l and int(l.split()[1]) >= 11))
PY
cat wf1.txt wf2.txt
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1 $PN"
$LS $L -opt 1 -j 4 -out order_hot.txt > ls_hot.out 2>&1 || exit 1   # the hot functions
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl41/'
PIN = ('0', 'vupv_96936c0f_0_p3')
hot = [tuple(l.split()) for l in open(R + 'lay/order_hot.txt') if l.strip()]; hs = set(hot)
have = set(); A = {}; N = {}
for f, reg in (('lay/funcs_deck.txt', {'0': '0', '1': '1'}), ('lay/funcs_w01a.txt', {'0': '0', '1': '2'})):
    for l in open(R + f):
        w = l.split()
        if len(w) == 4 and w[0] in reg: have.add((reg[w[0]], w[3])); A.setdefault((reg[w[0]], w[1]), set()).add(w[3]); N[(reg[w[0]], w[3])] = w[1]
old = [tuple(l.split()) for l in open('/home/claude/main/build_gl41/old/gl40_lay_small/order_s1.txt') if l.strip()]
keep = []; sub = []; isra = []
for x in old:
    if x not in have and '.isra.' in x[1] and (x[0], x[1].split('.isra.')[0]) in hs:
        isra.append((x[1], x[1].split('.isra.')[0])); x = (x[0], x[1].split('.isra.')[0])
    if x not in have or x == PIN: continue
    if x not in hs:
        al = [(x[0], n) for n in sorted(A[(x[0], N[x])]) if (x[0], n) in hs]
        if al: sub.append((x, al[0])); x = al[0]
    if x not in keep: keep.append(x)
ks = set(keep)
seed = {r: [n for (q, n) in keep if q == r] for r in '012'}
new = [x for x in hot if x not in ks and x != PIN]
for r, n in new: seed[r].append(n)
open(R + 'ord/seed.txt', 'w').write(''.join('%s %s\n' % (r, n) for r in '012' for n in seed[r]))
dup = [(k, sorted(n for n in v if (k[0], n) in set((r, m) for r in '012' for m in seed[r]))) for k, v in A.items()]
dup = [d for d in dup if len(d[1]) > 1]
gone = [x for x in old if x not in have and not ('.isra.' in x[1] and (x[0], x[1].split('.isra.')[0]) in hs)]
print('seed from gl40\'s final order: listed', len(old), 'kept', len(keep), 'clone names made plain', len(isra), isra[:16], 'alias names replaced', sub,
      'gone (no such function now)', len(gone), gone[:12], 'not fetched now', len(ks - hs), 'newly hot appended', len(new), new[:40])
print('addresses with more than one name in the seed:', dup[:10])
print('the pinned copy fetched:', PIN in hs)
PY
$LS $L -inall -norm -guard 10 -bind ${BK:-20} -wf wf1.txt -t0 ${T0:-0.0005} -opt ${OPT:-40000} -j ${JS:-4} -seed 1 -in $R/ord/seed.txt -out order_s1.txt > ls_s1.out 2>&1; echo "order_s1 rc $?"
grep -v '^it ' ls_s1.out | tail -n 50
$LS $L -inall -norm -guard 10 -bind ${BK:-20} -wf wf2.txt -t0 ${T02:-0.0002} -opt ${OPT2:-30000} -j ${JS:-4} -seed 2 -in order_s1.txt -out order_s2.txt > ls_s2.out 2>&1; echo "order_s2 rc $?"
grep -v '^it ' ls_s2.out | tail -n 50
for s in s1 s2; do
  python3 -I $R/tools/split41.py $R/lay/order_$s.txt $R/ord/order_deck_$s.txt $R/ord/order_w01a_$s.txt || exit 1
  ( cd $R/ord || exit 1
    for d in deck_$s w01a_$s; do mkdir -p $d; for o in $R/u/*.o; do ln -sf $o $d/; done; done
    python3 -I $R/m/tools/layout_order.py order_deck_$s.txt $R/ord/deck_$s w00a > lo_deck_$s.out 2>&1; tail -n 2 lo_deck_$s.out
    python3 -I $R/m/tools/layout_order.py order_w01a_$s.txt $R/ord/w01a_$s w01a > lo_w01a_$s.out 2>&1; tail -n 2 lo_w01a_$s.out
    mkdir -p $s; cmp deck_$s/layout_elf.ld w01a_$s/layout_elf.ld && echo "$s: layout_elf.ld equal for both routes"
    cp deck_$s/layout_elf.ld deck_$s/layout_w00a.ld w01a_$s/layout_w01a.ld $s/
    python3 -I $R/tools/pin41.py $s/layout_elf.ld $PIN $PREF 0x80 || exit 1
    md5sum $s/*.ld ) || exit 1
done
echo "rt41b.sh: done $(date -u +%H:%M:%S)"
