#!/usr/bin/env python3
"""sysdiag_read.py <battery.txt> [--tail N] [--route c]: reads the battery lines of a PSP run (psp/v26me_bat.c) along the gl route's parts, and the
SYS_DIAG part when the build has it (gl2d on: me_switchoff_plan.md "The codec call", task #62):
- per part of the route (vblank ranges from the emulator shots of MGS2GL2_rt, psp_harness.md "Build gl2"; approximate at the edges):
  lines, battery % and mV first -> last, and with SYS_DIAG the low-priority thread (0x6F, asks for a wake every 10 ms): its longest
  wait for the CPU, waits over 100 ms, wakes a second; thread switches a second, comings out of idle a second and the idle share
  (sceKernelReferSystemStatus deltas; PPSSPP leaves them 0);
- the power events (the callback's flags) and the thread lists (the first and the last in full);
- the last N lines (default 8): what the PSP did in the seconds before a stop.
Lines are 5 s apart ("every 5 s"); "start", "end" and "HOME" lines are kept as they are.
--route c (gl4c, 2026-10-28): the codec test route's parts (play/pad_codec.txt): the same to the aft deck, then the codec opened with
SELECT at vb 30,510 and the 15 Otacon calls from their O presses to vb 60,000."""
import re, sys

PARTS = [(-1, 'boot, logos, title, menus'), (3500, 'opening d00t (black)'), (15550, 'codec call, before the dense hashes'),
         (15700, 'codec call, dense hashes 15700-16010'), (16010, 'codec call, after the dense hashes'), (20000, 'd01t (black)'),
         (22950, 'deck moment'), (23600, 'black, heavy steps'), (24100, 'viewfinder'), (25100, 'black'), (26100, 'codec again'),
         (28800, 'deck (w00a)'), (31500, 'indoors (w01a)'), (37800, 'end')]
O_CALLS = [30620, 33586, 35930, 38980, 39949, 41441, 44457, 47839, 50311, 51413, 52220, 53520, 54588, 55851, 56866]
PARTS_C = PARTS[:12] + [(30510, 'codec opened (SELECT, aft deck)')] + \
    [(v, 'Otacon call %d (O at vb %d)' % (i + 1, v)) for i, v in enumerate(O_CALLS)] + [(60000, 'end')]

HEAD = re.compile(r'^(?P<why>[a-zA-Z0-9 ]+?) t (?P<t>\d+) s, vb (?P<vb>-?\d+): (?P<pct>-?\d+)%, (?P<mv>-?\d+) mV')
DIAG = re.compile(r'low-priority thread: longest wait (?P<lw>\d+) ms, waits over 100 ms (?P<w100>\d+), wakes (?P<wk>\d+)'
                  r'(?:; thread switches (?P<sw>\d+), out of idle (?P<oi>\d+), idle (?P<idle>\d+) ms)?')
EXT = re.compile(r'external power (\d), charging (\d), low (\d)')


def part_of(vb):
    name = PARTS[0][1]
    for start, n in PARTS:
        if vb >= start: name = n
    return name


def main():
    a = sys.argv[1:]
    if not a: sys.exit(__doc__)
    tail = 8
    if '--tail' in a: tail = int(a[a.index('--tail') + 1])
    global PARTS
    if '--route' in a and a[a.index('--route') + 1] == 'c': PARTS = PARTS_C
    rows, events, threads, cur = [], [], [], None
    for ln in open(a[0], errors='replace'):
        ln = ln.rstrip('\n')
        if ln.startswith('power event'):
            events.append(ln); continue
        if ln.startswith('threads '):
            cur = [ln]; threads.append(cur); continue
        if ln.startswith('  ') and cur is not None:
            cur.append(ln); continue
        cur = None
        m = HEAD.match(ln)
        if not m: continue
        r = dict(why=m['why'], t=int(m['t']), vb=int(m['vb']), pct=int(m['pct']), mv=int(m['mv']), line=ln)
        d = DIAG.search(ln)
        if d:
            for k in ('lw', 'w100', 'wk', 'sw', 'oi', 'idle'):
                r[k] = int(d[k]) if d[k] is not None else None
        e = EXT.search(ln)
        if e: r['ext'], r['chg'], r['low'] = (int(x) for x in e.groups())
        rows.append(r)
    if not rows: sys.exit('no battery lines in ' + a[0])
    diag = any('lw' in r for r in rows)
    first, last = rows[0], rows[-1]
    print('%d lines, t %d -> %d s, vb %d -> %d; first "%s", last "%s"; SYS_DIAG part %s' %
          (len(rows), first['t'], last['t'], first['vb'], last['vb'], first['why'], last['why'], 'yes' if diag else 'no'))
    print('external power 1 on %d lines, charging 1 on %d, low 1 on %d' %
          (sum(1 for r in rows if r.get('ext') == 1), sum(1 for r in rows if r.get('chg') == 1), sum(1 for r in rows if r.get('low') == 1)))
    print()
    hdr = '%-38s %5s %11s %11s' % ('part', 'lines', 'battery %', 'mV')
    if diag: hdr += ' %8s %7s %7s %9s %9s %6s' % ('wait max', '>100ms', 'wakes/s', 'switch/s', 'unidle/s', 'idle')
    print(hdr)
    order = [n for _, n in PARTS]
    for name in order:
        g = [r for r in rows if r['why'].startswith('every') and part_of(r['vb']) == name]
        if not g: continue
        s = '%-38s %5d %4d -> %3d %5d -> %4d' % (name, len(g), g[0]['pct'], g[-1]['pct'], g[0]['mv'], g[-1]['mv'])
        if diag:
            dg = [r for r in g if 'lw' in r]
            secs = 5.0 * len(dg)
            sw = [r for r in dg if r.get('sw') is not None]
            s += ' %5d ms %7d %7.1f' % (max(r['lw'] for r in dg), sum(r['w100'] for r in dg), sum(r['wk'] for r in dg) / secs)
            if sw:
                s += ' %9.0f %9.0f %5.1f%%' % (sum(r['sw'] for r in sw) / (5.0 * len(sw)), sum(r['oi'] for r in sw) / (5.0 * len(sw)),
                                            100.0 * sum(r['idle'] for r in sw) / (5000.0 * len(sw)))
        print(s)
    if events:
        print('\npower events (%d):' % len(events))
        for e in events[:12]: print('  ' + e[:200])
        if len(events) > 12: print('  ...'); [print('  ' + e[:200]) for e in events[-4:]]
    if threads:
        print('\nthread lists: %d; the first and the last:' % len(threads))
        for t in (threads[0],) + ((threads[-1],) if len(threads) > 1 else ()):
            for l in t: print('  ' + l)
    print('\nthe last %d lines:' % tail)
    for r in rows[-tail:]:
        s = '  %-8s t %5d s, vb %6d: %3d%%, %5d mV' % (r['why'][:8], r['t'], r['vb'], r['pct'], r['mv'])
        if 'lw' in r:
            s += '; wait max %d ms, >100 ms %d, wakes %d' % (r['lw'], r['w100'], r['wk'])
            if r.get('sw') is not None: s += '; switches %d, out of idle %d, idle %d ms' % (r['sw'], r['oi'], r['idle'])
        print(s)


main()
