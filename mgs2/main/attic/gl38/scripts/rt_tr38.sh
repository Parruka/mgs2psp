#!/bin/sh
# rt_tr38.sh (gl38): the traces of the use build u again (rt_b38.sh step 2; the merged files were deleted to free the disk before
# the model check showed the relinked build over the guard), and rt_order38.sh's merge into lay/t0n.it and lay/t1n.it.
R=/home/claude/main/build_gl38; cd $R/m || exit 1; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
TCFG=$R/cfg/mgs2_gl38trc.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t0 16001 17501 26501 27801 29601 29801 30101 31101 > $R/trace_t0b.out 2>&1 &
TCFG=$R/cfg/mgs2_gl38trc_t1.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t1 31901 32651 33401 34151 34901 35651 36401 37151 > $R/trace_t1b.out 2>&1 &
TCFG=$R/cfg/mgs2_gl38trc.cfg nice -n 10 sh scripts/pgo.sh trace $R/u t2 16101 27501 > $R/trace_t2b.out 2>&1 &
wait; for t in t0 t1 t2; do case $t in t0) rm -rf "/home/claude/main/build_gl38/ms/t0" ;; t1) rm -rf "/home/claude/main/build_gl38/ms/t1" ;; t2) rm -rf "/home/claude/main/build_gl38/ms/t2" ;; esac; [ -s $R/lay/$t.it ] || { echo "no trace $t"; exit 1; }; done
cd $R/lay || exit 1; D=$R/u
A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
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
np.concatenate([d[0], e[0], d[2], e[1], d[3], d[4], d[5], d[6], d[7], d[1]]).tofile('t0n.it'); del d, e
w = wins('t1.it', 8); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1n.it'); del w
" > merge_n2.out 2>&1 || { cat merge_n2.out; exit 1; }
cmp merge_n.out merge_n2.out && echo "merge counts equal to the first traces'"; rm -f t0.it t1.it t2.it; ls -la t0n.it t1n.it
echo "rt_tr38.sh: done $(date -u +%H:%M:%S)"
