#!/bin/sh
# rt43b.sh (gl43; build_gl42/rt42b.sh with build_gl43, gl43's 24 windows and gl42's final order (build_gl42/lay/order_s2.txt) as the seed;
# layout_sim41, split41.py and pin41.py from build_gl42; nice 19):
#  - merge: lay/t0n.it = window 0 16,001 | 1 16,101 | 2 26,801 (t28 K-A deck) | 3 27,101 (t31 K-N2 deck) | 4 26,901 (t29 K-N2R deck) |
#    5 29,201 (t52 K-A rain) | 6 29,301 (t53 K-B) | 7 29,601 (t56 K-N2) | 8 29,701 (t57 K-N2B) | 9 30,201 (t62 K-B) | 10 30,401 (t64 K7) |
#    11 30,501 (t65 K7B) | 12 30,801 (t68 K-A walk) | 13 30,901 (t69 K-B walk) | 14 31,201 (t72 K-N2B walk) |
#    15..22 the guard cfg's 16,121 16,161 21,121 21,161 23,121 23,161 27,521 27,561 | 23 17,501 (test).
#    Windows before the rain (vb < 29,000) have another overlay in the arena: their arena fetch runs are moved away (as gl39-gl42).
#  - caps, seed rules, pin, stages s1 / s2: as gl42 (rt42b.sh's notes).
R=/home/claude/main/build_gl43; cd $R/lay || exit 1; D=$R/u; LS=/home/claude/main/build_gl42/layout_sim41; mkdir -p $R/ord
SEEDORD=${SEEDORD:-/home/claude/main/build_gl42/lay/order_s2.txt}; [ -s $SEEDORD ] || { echo "rt43b.sh: no $SEEDORD"; exit 1; }
PIN=vupv_96936c0f_0_p3; PREF=vupv_8baafbc3_0; PN="-pin $PIN:$PREF:0x80"
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
B0=$(psp-nm $D/MGS2T.elf | awk '$3 == "_ftext" { print $1 }'); B1=$(printf %08x $((0x$A0 + 0x34))); echo "base0 $B0 base1 $B1" > bases.txt
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w00a.elf $D w00a > funcs_deck.txt 2> funcs_deck.err
python3 -I $R/m/tools/layout_funcs.py $D/MGS2T.elf $D/ovl_w01a.elf $D w01a > funcs_w01a.txt 2> funcs_w01a.err
grep -q " $PIN\$" funcs_deck.txt && grep -q " $PREF\$" funcs_deck.txt || { echo "rt43b.sh: $PIN or $PREF not in funcs_deck.txt"; exit 1; }
[ -s t0n.it ] && [ -s t1n.it ] || { nice -n 19 python3 -I -c "
import numpy as np
def wins(p, n):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == n + 1, (p, len(m)); return [a[m[k]:m[k + 1]] for k in range(n)]
lo, hi = 0x$A0, 0x$A0 + $OS
def mv(x, tag):
    x = x.copy(); s = (x[:, 0] >= lo) & (x[:, 0] < hi); x[s, 0] += 0x1000000; print(tag, 'arena fetch runs moved:', int(s.sum()), 'of', len(x)); return x
d = wins('t0.it', 8); e = wins('t2.it', 8); g = wins('t3.it', 8)
d = [mv(d[k], 't0 window %d' % k) if k < 4 else d[k] for k in range(8)]
e = [mv(e[k], 't2 window %d' % k) if k in (0, 1) else e[k] for k in range(8)]
g = [mv(g[k], 't3 window %d' % k) for k in range(8)]
for k in range(8): print('t0 window', k, 'fetch runs', len(d[k]))
for k in range(8): print('t2 window', k, 'fetch runs', len(e[k]))
for k in range(8): print('t3 window', k, 'fetch runs', len(g[k]))
np.concatenate([d[0], e[0], d[2], d[3], e[1], d[4], d[5], d[6], d[7], e[2], e[3], e[4], e[5], e[6], e[7]] + g + [d[1]]).tofile('t0n.it'); del d, e, g
w = wins('t1.it', 8); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1n.it'); del w
" > merge_n.out 2>&1 || { cat merge_n.out; exit 1; }; cat merge_n.out
  for f in t0.it t2.it t3.it; do [ -s /home/claude/main/build_gl43/lay/$f ] && rm -f "/home/claude/main/build_gl43/lay/$f"; done; }
python3 -I - <<'PY' || exit 1
R = '/home/claude/main/build_gl43/'
def im(p): return int(open(p).readline().split()[4])
x36 = [im('/home/claude/main/exp40/model/x36' + s) for s in ('', '.1', '.2', '.3')]
mu = [im(R + 'model/mu' + s) for s in ('', '.1', '.2', '.3')]
g = [im(R + 'lay/t3' + ('' if k == 0 else '.%d' % k)) for k in range(8)]
out = ''.join('1 %d 4 0\n' % w for w in range(5, 15))
for k in range(8):
    w = k // 2; cap = int(g[k] * 1.02 * x36[w] / mu[w] + 0.5)
    out += '1 %d 2 %d\n' % (15 + k, cap)
    print('guard window %d (t0n window %d, model window %d): u %d, gl36/u %.4f, cap %d' % (k, 15 + k, w, g[k], x36[w] / mu[w], cap))
print('model I-misses gl36', x36, 'u', mu)
open(R + 'lay/wf1.txt', 'w').write(out)
open(R + 'lay/wf2.txt', 'w').write(''.join('1 %d 12 0\n' % w for w in range(5, 15)) + ''.join(
    '1 %s 1 %s\n' % tuple(l.split()[1::2]) for l in out.split('\n') if l and int(l.split()[1]) >= 15))
PY
cat wf1.txt wf2.txt
L="t0n.it funcs_deck.txt -and t1n.it funcs_w01a.txt -wl 1 -wd 1 -test 1 -base0 $B0 -base1 $B1 $PN"
nice -n 19 $LS $L -opt 1 -j 4 -out order_hot.txt > ls_hot.out 2>&1 || exit 1   # the hot functions
SEEDORD=$SEEDORD python3 -I - <<'PY' || exit 1
import os
R = '/home/claude/main/build_gl43/'
PIN = ('0', 'vupv_96936c0f_0_p3')
hot = [tuple(l.split()) for l in open(R + 'lay/order_hot.txt') if l.strip()]; hs = set(hot)
have = set(); A = {}; N = {}
for f, reg in (('lay/funcs_deck.txt', {'0': '0', '1': '1'}), ('lay/funcs_w01a.txt', {'0': '0', '1': '2'})):
    for l in open(R + f):
        w = l.split()
        if len(w) == 4 and w[0] in reg: have.add((reg[w[0]], w[3])); A.setdefault((reg[w[0]], w[1]), set()).add(w[3]); N[(reg[w[0]], w[3])] = w[1]
old = [tuple(l.split()) for l in open(os.environ['SEEDORD']) if l.strip()]
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
print('seed', os.environ['SEEDORD'], ': listed', len(old), 'kept', len(keep), 'clone names made plain', len(isra), isra[:16], 'alias names replaced', sub,
      'gone (no such function now)', len(gone), gone[:12], 'not fetched now', len(ks - hs), 'newly hot appended', len(new), new[:60])
print('addresses with more than one name in the seed:', dup[:10])
print('the pinned copy fetched:', PIN in hs)
PY
nice -n 19 $LS $L -inall -norm -guard 10 -bind ${BK:-20} -wf wf1.txt -t0 ${T0:-0.0005} -opt ${OPT:-40000} -j ${JS:-4} -seed 1 -in $R/ord/seed.txt -out order_s1.txt > ls_s1.out 2>&1; echo "order_s1 rc $? $(date -u +%H:%M:%S)"
grep -v '^it ' ls_s1.out | tail -n 50
nice -n 19 $LS $L -inall -norm -guard 10 -bind ${BK:-20} -wf wf2.txt -t0 ${T02:-0.0002} -opt ${OPT2:-30000} -j ${JS:-4} -seed 2 -in order_s1.txt -out order_s2.txt > ls_s2.out 2>&1; echo "order_s2 rc $? $(date -u +%H:%M:%S)"
grep -v '^it ' ls_s2.out | tail -n 50
for s in s1 s2; do
  python3 -I /home/claude/main/build_gl42/tools/split41.py $R/lay/order_$s.txt $R/ord/order_deck_$s.txt $R/ord/order_w01a_$s.txt || exit 1
  ( cd $R/ord || exit 1
    for d in deck_$s w01a_$s; do mkdir -p $d; for o in $R/u/*.o; do ln -sf $o $d/; done; done
    python3 -I $R/m/tools/layout_order.py order_deck_$s.txt $R/ord/deck_$s w00a > lo_deck_$s.out 2>&1; tail -n 2 lo_deck_$s.out
    python3 -I $R/m/tools/layout_order.py order_w01a_$s.txt $R/ord/w01a_$s w01a > lo_w01a_$s.out 2>&1; tail -n 2 lo_w01a_$s.out
    mkdir -p $s; cmp deck_$s/layout_elf.ld w01a_$s/layout_elf.ld && echo "$s: layout_elf.ld equal for both routes"
    cp deck_$s/layout_elf.ld deck_$s/layout_w00a.ld w01a_$s/layout_w01a.ld $s/
    python3 -I /home/claude/main/build_gl42/tools/pin41.py $s/layout_elf.ld $PIN $PREF 0x80 || exit 1
    md5sum $s/*.ld ) || exit 1
done
echo "rt43b.sh: done $(date -u +%H:%M:%S)"
