#!/usr/bin/env python3
"""rl_cmp.py <log a> <log b> [<boot binary>]: compares the "rl:" lines of two x86 GEN_DG_TRACE runs with MGS2_READ_LOG=<from>:<to> (boot.c
dgt_rh_add: each guest CPU read outside the draw pipeline, its address, the word, the reader and the word's last writer as host offsets from
rt_dgt_rd).  The two runs must take the same path (equal read counts per vblank, rh_cmp.py), so the reads pair up by index.  Prints the reads
whose address or word differs, grouped by reader / writer / address block (256 bytes), with the first example of each; with the binary, the
offsets are named through nm (the function that holds reader + rt_dgt_rd).  "lanes" names the 4-byte lanes of the 16-byte quadword
(x y z w, by address) in which the bytes differ: leftovers in a vector's unused w lane show as "w"."""
import bisect, subprocess, sys
def load(fn):
    d = {}
    for l in open(fn, errors='replace'):
        if l.startswith('rl: vb '):
            p = l.split()
            d[(int(p[2]), int(p[3]))] = (int(p[4], 16), p[5], p[6], p[7], len(p) > 8)   # bytes: hex, highest address first
    return d
a = load(sys.argv[1]); b = load(sys.argv[2])
name = lambda o: o
if len(sys.argv) > 3:
    syms = []
    for l in subprocess.run(['nm', '-n', sys.argv[3]], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
    base = [s for s in syms if s[1] == 'rt_dgt_rd'][0][0]; addrs = [s[0] for s in syms]
    def name(o):
        if o.startswith('wc'): return o
        x = base + int(o[1:], 16) if o[0] == 'r' or o[0] == 'w' else None
        if x is None: return o
        x &= (1 << 64) - 1
        i = bisect.bisect_right(addrs, x) - 1
        return '%s+%x' % (syms[i][1], x - syms[i][0]) if i >= 0 else o
keys = sorted(set(a) & set(b))
print('reads in both: %d (a only %d, b only %d)' % (len(keys), len(set(a) - set(b)), len(set(b) - set(a))))
diff = [k for k in keys if a[k][:2] != b[k][:2]]
addr = sum(1 for k in diff if a[k][0] != b[k][0])
def lanes(x, y):
    n = len(x[1]) // 2; m = 0
    if x[0] != y[0] or len(y[1]) != len(x[1]): return 'address'
    for i in range(n):
        j = (n - 1 - i) * 2
        if x[1][j:j + 2] != y[1][j:j + 2]: m |= 1 << ((x[0] + i) & 15) // 4
    return ''.join('xyzw'[l] for l in range(4) if m >> l & 1)
print('differ: %d (address differs in %d)' % (len(diff), addr))
g = {}
for k in diff:
    x, y = a[k], b[k]
    rd = name(x[2]); wa = name(x[3]) + (' in' if x[4] else ''); wb = name(y[3]) + (' in' if y[4] else '')
    key = (rd, wa, wb, x[0] >> 8, lanes(x, y))
    if key not in g: g[key] = [0, k, x, y]
    g[key][0] += 1
for key, (n, k, x, y) in sorted(g.items(), key=lambda t: -t[1][0]):
    rd, wa, wb, blk, ln = key
    print('%6d  block %08x  lanes %s  reader %s\n        writer a %s / b %s\n        first: vb %d #%d  %08x: %s / %s' % (n, blk << 8, ln, rd, wa, wb, k[0], k[1], x[0], x[1], y[1]))
