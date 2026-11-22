#!/usr/bin/env python3
"""dgt_report.py <dg trace out> <boot binary>: summary of a GEN_DG_TRACE run (boot.c MGS2_DG_TRACE): which RAM words the draw pipeline
(the call of 0x11da90) last wrote that code outside it then read, grouped by reader (generated function, named with nm; DMA channels) and
by address range, and which pipeline code wrote the words game code reads; with "cx" lines (boot.c dgt_cnote) also the pipeline's state
across calls: the words it reads that it wrote in an earlier call, by writer and reader; with "pd" lines (boot.c rt_dgt_dma) the packet data
DMA sends after a later call than the one that wrote it.  Numbers only (addresses, counts): no game data."""
import bisect, collections, re, subprocess, sys
out, binary = sys.argv[1:3]
syms = []
for l in subprocess.run(['nm', '-n', binary], capture_output=True, text=True).stdout.split('\n'):
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
addrs = [a for a, _ in syms]
nm_base = dict((n, a) for a, n in syms)['rt_dgt_rd']
DMA = {0: 'VIF0', 1: 'VIF1', 2: 'GIF', 8: 'fromSPR', 9: 'toSPR'}
def who(ra, off):
    if ra < 0x1000:
        return ('dma ' if ra < 0x200 else 'spr copy to ') + DMA.get(ra & 0xff, str(ra & 0xff))
    i = bisect.bisect_right(addrs, ra - off) - 1
    return syms[i][1] if i >= 0 else hex(ra)
def ranges(ws, gap=64):
    r = []
    for a in sorted(ws):
        if r and a - r[-1][1] <= gap: r[-1][1] = a
        else: r.append([a, a])
    return r
off = 0; by_reader = collections.defaultdict(dict); by_word = collections.defaultdict(set); hdr = []; vu = []; inside = []
by_writer = collections.defaultdict(dict); by_dw = collections.defaultdict(collections.Counter)
cx_pair = collections.defaultdict(dict); cx_words = set(); pd = []
for l in open(out):
    p = l.split()
    if p[0] == 'base': off = int(p[2], 16) - nm_base
    elif p[0] == 'rd':
        a, ra, n = int(p[1], 16), int(p[2], 16), int(p[3])
        r = who(ra, off); by_reader[r][a] = by_reader[r].get(a, 0) + n; by_word[a].add(r)
    elif p[0] == 'wr':
        a, wr, n = int(p[1], 16), int(p[2], 16), int(p[3])
        w = who(wr, off) if wr else 'not written inside'; by_writer[w][a] = by_writer[w].get(a, 0) + n
    elif p[0] == 'dw':
        ch, wr, n = int(p[1], 16), int(p[2], 16), int(p[3])
        by_dw[who(ch, off)][who(wr, off) if wr else 'not written inside'] += n
    elif p[0] == 'cx':
        a, wr, rd, n = int(p[1], 16), int(p[2], 16), int(p[3], 16), int(p[4])
        k = (who(wr, off) if wr else 'not written inside', who(rd, off)); cx_pair[k][a] = cx_pair[k].get(a, 0) + n; cx_words.add(a)
    elif p[0] == 'xc': pass
    elif p[0] == 'pd':
        a, b, wr, age = int(p[1], 16), int(p[2], 16), int(p[3], 16), int(p[4])
        pd.append((a, b, who(wr, off) if wr else 'not written inside', age))
    elif p[0] == 'vu': vu.append(p[1:])
    elif p[0] == 'in': inside.append((int(p[1], 16), int(p[2], 16)))
    else: hdr.append(l.rstrip())
print('\n'.join(hdr))
span = lambda a: 'scratchpad' if a < 0x4000 else ''
print('\nreaders outside the draw pipeline of words it last wrote (reader: words, reads; ranges):')
for r, d in sorted(by_reader.items(), key=lambda kv: -sum(kv[1].values())):
    rg = ranges(d)
    print('  %-28s %6d words %9d reads  %s' % (r, len(d), sum(d.values()),
          ' '.join('%06x-%06x%s' % (x, y + 4, span(x)) for x, y in rg[:8]) + (' ... (%d ranges)' % len(rg) if len(rg) > 8 else '')))
print('\nby address range (range: words, readers):')
for x, y in ranges(by_word, 256):
    ws = [a for a in by_word if x <= a <= y]; rs = set().union(*(by_word[a] for a in ws))
    print('  %06x-%06x %6d words  %s' % (x, y + 4, len(ws), ', '.join(sorted(rs))[:200]))
if by_writer:
    print('\nwriters inside the draw pipeline of the words game code reads (writer: words, reads; ranges):')
    for w, d in sorted(by_writer.items(), key=lambda kv: -sum(kv[1].values())):
        print('  %-28s %6d words %9d reads  %s' % (w, len(d), sum(d.values()), ' '.join('%06x-%06x%s' % (x, y + 4, span(x)) for x, y in ranges(d))))
for ch, c in sorted(by_dw.items()):
    t = sum(c.values()); print('\nwriters inside the draw pipeline of what %s reads (%d reads):' % (ch, t))
    for w, n in c.most_common(25): print('  %-28s %9d reads %5.1f%%' % (w, n, 100.0 * n / t))
if cx_pair:
    print('\nthe pipeline\'s state across calls: %d words it reads that it wrote in an earlier call (writer -> reader: words, reads; ranges):' % len(cx_words))
    for (w, r), d in sorted(cx_pair.items(), key=lambda kv: -len(kv[1])):
        rg = ranges(d)
        print('  %-24s -> %-24s %6d words %9d reads  %s' % (w, r, len(d), sum(d.values()),
              ' '.join('%06x-%06x%s' % (x, y + 4, span(x)) for x, y in rg[:6]) + (' ... (%d ranges)' % len(rg) if len(rg) > 6 else '')))
if pd:
    by_pw = collections.defaultdict(list)
    for a, b, w, age in pd: by_pw[w].append((a, b, age))
    print('\npersistent packet data: words DMA sent outside the pipeline after a later call than the one that wrote them (writer: words, largest call age; ranges):')
    for w, rs in sorted(by_pw.items(), key=lambda kv: -sum(b - a for a, b, _ in kv[1])):
        print('  %-28s %7d words  age %5d  %s' % (w, sum(b - a for a, b, _ in rs) // 4, max(g for _, _, g in rs),
              ' '.join('%06x-%06x' % (a, b) for a, b, _ in rs[:8]) + (' ... (%d ranges)' % len(rs) if len(rs) > 8 else '')))
print('\nmicro calls outside the draw pipeline (code hash, entry, code uploaded inside it?, calls):')
for h, pc, pin, n in vu: print('  %s pc %4s uploaded %s %s' % (h, pc, 'inside ' if pin == '1' else 'outside', n))
tot = sum(y - x for x, y in inside)
print('\nwords last written inside the draw pipeline at the stop: %d KB in %d ranges; largest:' % (tot // 1024, len(inside)))
for x, y in sorted(inside, key=lambda r: r[0] - r[1])[:12]: print('  %06x-%06x %d KB' % (x, y, (y - x) // 1024))
