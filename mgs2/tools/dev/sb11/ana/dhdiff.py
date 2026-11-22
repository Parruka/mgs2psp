# dhdiff.py <dhistA> <elfsA comma> <dhistB> <elfsB comma> [n]: D-misses per data symbol in two builds (dhist with 64-byte lines),
# g_mem split by 1 MB of guest address; the largest changes; and the misses by D-cache set (address bits 6-12) for both
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def syms_of(elfs):
    s = []
    for e in elfs.split(','):
        for ln in subprocess.run([NM, '-S', '-n', e], capture_output=True, text=True).stdout.split('\n'):
            p = ln.split()
            if len(p) == 4 and p[2] in 'bBdDgGsSrRvV': s.append((int(p[0], 16), int(p[1], 16), p[3]))
    s.sort(); return s
def rd(f, elfs):
    syms = syms_of(elfs); addrs = [x[0] for x in syms]; gm = [x for x in syms if x[2] == 'g_mem'][0]
    tot = collections.Counter(); sets = collections.Counter(); n = 0
    for ln in open(f):
        a, c = ln.split(); a = int(a, 16); c = int(c); n += c; sets[(a >> 6) & 127] += c
        if gm[0] <= a < gm[0] + gm[1]:
            o = a - gm[0]; k = 'g_mem SPR 0-16K' if o < 0x4000 else 'g_mem %03x MB' % (o >> 20); tot[k] += c; continue
        i = bisect.bisect_right(addrs, a) - 1
        if i >= 0 and a < syms[i][0] + max(syms[i][1], 64): tot[syms[i][2]] += c
        else: tot['? %07x' % (a & ~0xffff)] += c
    return n, tot, sets
na, ta, sa = rd(sys.argv[1], sys.argv[2]); nb, tb, sb = rd(sys.argv[3], sys.argv[4]); N = int(sys.argv[5]) if len(sys.argv) > 5 else 40
print('total D-misses A %d, B %d (%+d, %+.1f%%)' % (na, nb, nb - na, 100.0 * (nb - na) / na))
rows = sorted(set(ta) | set(tb), key=lambda k: -abs(tb[k] - ta[k]))
for k in rows[:N]: print('%-34s %9d -> %9d  %+9d' % (k, ta[k], tb[k], tb[k] - ta[k]))
print('top sets B (misses A -> B):', ', '.join('%d: %d->%d' % (s, sa[s], sb[s]) for s, _ in sb.most_common(16)))
