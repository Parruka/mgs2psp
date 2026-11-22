#!/usr/bin/env python3
"""rl_cmps.py <log a(.gz)> <log b(.gz)> [<boot binary>]: tools/dev/rl_cmp.py for logs too big to hold in memory: reads the two runs'
"rl:" lines in step (two runs on the same path pair up by vblank and index) and keeps only the reads whose address or bytes differ;
the same groups (reader / writer / 256-byte block / lanes) and output, plus the differing reads per vblank."""
import bisect, gzip, itertools, subprocess, sys
def rl(fn):
    f = gzip.open(fn, 'rt', errors='replace') if fn.endswith('.gz') else open(fn, errors='replace')
    for l in f:
        if l.startswith('rl: vb '):
            p = l.split()
            yield (int(p[2]), int(p[3])), (int(p[4], 16), p[5], p[6], p[7], len(p) > 8)
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
def lanes(x, y):
    n = len(x[1]) // 2; m = 0
    if x[0] != y[0] or len(y[1]) != len(x[1]): return 'address'
    for i in range(n):
        j = (n - 1 - i) * 2
        if x[1][j:j + 2] != y[1][j:j + 2]: m |= 1 << ((x[0] + i) & 15) // 4
    return ''.join('xyzw'[l] for l in range(4) if m >> l & 1)
n = 0; diff = []; per = {}; nvb = {}
for ea, eb in itertools.zip_longest(rl(sys.argv[1]), rl(sys.argv[2])):
    if ea is None or eb is None or ea[0] != eb[0]:
        print('paths differ (or a log ends) after %d reads: a %s / b %s' % (n, ea and ea[0], eb and eb[0])); break
    n += 1; nvb[ea[0][0]] = nvb.get(ea[0][0], 0) + 1
    if ea[1][:2] != eb[1][:2]: diff.append((ea[0], ea[1], eb[1])); per[ea[0][0]] = per.get(ea[0][0], 0) + 1
print('reads paired: %d in %d vblanks' % (n, len(nvb)))
print('differ: %d (address differs in %d) in %d vblanks' % (len(diff), sum(1 for k, x, y in diff if x[0] != y[0]), len(per)))
print('per vblank (differing / logged): ' + ' '.join('%d:%d/%d' % (v, per[v], nvb[v]) for v in sorted(per)))
g = {}
for k, x, y in diff:
    rd = name(x[2]); wa = name(x[3]) + (' in' if x[4] else ''); wb = name(y[3]) + (' in' if y[4] else '')
    key = (rd, wa, wb, x[0] >> 8, lanes(x, y))
    if key not in g: g[key] = [0, k, x, y, set()]
    g[key][0] += 1; g[key][4].add(k[0])
for key, (c, k, x, y, vs) in sorted(g.items(), key=lambda t: -t[1][0]):
    rd, wa, wb, blk, ln = key
    print('%6d  block %08x  lanes %s  reader %s  (%d vblanks)\n        writer a %s / b %s\n        first: vb %d #%d  %08x: %s / %s' % (c, blk << 8, ln, rd, len(vs), wa, wb, k[0], k[1], x[0], x[1], y[1]))
