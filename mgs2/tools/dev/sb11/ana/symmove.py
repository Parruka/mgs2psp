# symmove.py <elf A> <elf B>: data symbols (d, b, r, s, g, v) of both builds, by name: address mod 8 KB in each and the shift mod 8 KB
import sys, subprocess, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def rd(e):
    d = {}
    for l in subprocess.run([NM, '-S', '-n', e], capture_output=True, text=True).stdout.splitlines():
        t = l.split()
        if len(t) == 4 and t[2] in 'bBdDrRsSgGvV': d.setdefault(t[3], (int(t[0], 16), int(t[1], 16), t[2]))
    return d
a, b = rd(sys.argv[1]), rd(sys.argv[2])
rows = []
for n in set(a) & set(b):
    (aa, sa, ta), (ab, sb, tb) = a[n], b[n]
    rows.append((ab, n, aa, sa, sb, ((ab - aa) % 8192)))
rows.sort()
grp = collections.Counter(r[5] for r in rows if r[1] != 'g_mem')
print('shift mod 8K -> number of symbols:', grp.most_common(12))
big = [r for r in rows if r[4] >= 4096 or r[1] in ('g_mem',)]
for ab, n, aa, sa, sb, sh in big[:80]:
    print('%-32s A %08x (mod8K %04x) B %08x (mod8K %04x) size %6x -> %6x shift %04x' % (n, aa, aa % 8192, ab, ab % 8192, sa, sb, sh))
only_b = sorted((b[n][0], n, b[n][1]) for n in set(b) - set(a))
print('only in B (%d):' % len(only_b), ', '.join('%s %08x +%x' % (n, ad, sz) for ad, n, sz in only_b[:40]))
