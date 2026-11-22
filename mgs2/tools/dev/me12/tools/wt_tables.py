#!/usr/bin/env python3
# wt_tables.py <binary> <log>...: me12 (task #142) the design note's writer tables from full "me12 wt:" lists: per route, the class-0
# writers (any kind) and the class-1 writers into texture sources (REF upload), by function; the counts of functions and sites by class
# and kind; and what the scratchpad DMA (dma208) writes
import sys, subprocess, bisect, re, collections
binary = sys.argv[1]
syms = []
for line in subprocess.run(['nm', '-n', '--defined-only', binary], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [a for a, _ in syms]
def name(off):
    if off < 0x1000: return 'dma%x' % off
    i = bisect.bisect_right(addrs, off) - 1
    return syms[i][1] if i >= 0 else '?%x' % off
rx = re.compile(r'me12 wt: writer ([0-9a-f]+) class (\d) (tag|after tag|REF|normal)( upload)?( in pipeline)?: writes (\d+) bytes (\d+) steps (\d+), first vb (\d+) at ([0-9a-f]+)')
for log in sys.argv[2:]:
    fn = collections.defaultdict(lambda: [0, 0, 0, 0, None, set()])   # (func, class, kind) -> writes, bytes, max steps, sites, (vb, addr), addrs
    tot = collections.Counter(); sites = collections.Counter(); funcs = collections.defaultdict(set); n = 0
    for line in open(log, errors='replace'):
        m = rx.search(line)
        if not m: continue
        n += 1
        w, c, k, up, pl, wr, b, st, vb, a = m.groups(); c = int(c); kk = k + (' upload' if up else '')
        f = name(int(w, 16)); e = fn[(f, c, kk)]
        e[0] += int(wr); e[1] += int(b); e[2] = max(e[2], int(st)); e[3] += 1
        if e[4] is None or int(vb) < e[4][0]: e[4] = (int(vb), a)
        e[5].add(int(a, 16) >> 16)
        tot[(c, kk)] += int(b); sites[(c, kk)] += 1; funcs[(c, kk)].add(f)
    print('== %s: %d writer lines' % (log, n))
    for (c, kk) in sorted(tot): print('  class %d %-16s bytes %11d sites %5d functions %4d' % (c, kk, tot[(c, kk)], sites[(c, kk)], len(funcs[(c, kk)])))
    allf = set(); [allf.update(v) for v in funcs.values()]
    c1ref = set(); [c1ref.update(funcs[x]) for x in funcs if x[0] == 1 and x[1] in ('REF',)]
    print('  functions in all: %d; class 1 REF (not texture sources): %d (dma208 among them: %s)' % (len(allf), len(c1ref), 'dma208' in c1ref))
    print('  class 0, by function: writes, bytes, most steps, sites, first vb at, 64 KB regions')
    for (f, c, kk), e in sorted(fn.items(), key=lambda x: -x[1][1]):
        if c == 0: print('   | %s | %s | %d | %d | %d | %d | vb %d at %s | %s |' % (f, kk, e[0], e[1], e[2], e[3], e[4][0], e[4][1], ' '.join('%03x' % r for r in sorted(e[5]))))
    print('  class 1 into texture sources (REF upload), by function')
    for (f, c, kk), e in sorted(fn.items(), key=lambda x: -x[1][1]):
        if c == 1 and kk == 'REF upload': print('   | %s | %d | %d | %d | %d | vb %d at %s | %s |' % (f, e[0], e[1], e[2], e[3], e[4][0], e[4][1], ' '.join('%03x' % r for r in sorted(e[5]))))
