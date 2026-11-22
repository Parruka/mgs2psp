#!/usr/bin/env python3
"""steps_in.py <steps.csv A> [<steps.csv B>]: v30's indoor route (play/w01a_route.md "The timing route", timed vb 31,801-37,800)
step for step: ms per step by part of the route with logic = step - draw - IOP - GS, p50 / p90 / p99 / max, full and reduced steps,
pack reads; the model's five 100-step windows against prof/model_v30_in.txt; with B (a later run of the same route) also B and B - A."""
import csv, sys
def load(p):
    R = {}
    for r in csv.DictReader(open(p)):
        R[int(r['vb'])] = {k: int(v) for k, v in r.items()}
    return R
runs = [load(p) for p in sys.argv[1:3]]
parts = [('stand', 31801, 32300), ('walk', 32301, 34110), ('crawl', 34111, 34510), ('punch and fire', 34511, 34900),
         ('first person', 34901, 35500), ('sneak', 35501, 36440), ('alert (entry)', 36441, 37200), ('evasion', 37201, 37600),
         ('alert again', 37601, 37800), ('ALL', 31801, 37800)]
model = [('model walk', 32601, 32700, 31.92), ('model crawl', 34301, 34400, 32.81), ('model first person', 35101, 35200, 26.61),
         ('model alert', 36901, 37000, 39.87), ('model evasion', 37201, 37300, 36.08)]
def stat(R, a, b):
    s = [R[v] for v in range(a, b + 1) if v in R]
    n = len(s)
    if not n: return 0, 0, 0, 0, 0, 0
    f = lambda k: sum(x[k] for x in s) / n / 1000
    step, iop, gs, dg = f('step_us'), f('iop_us'), f('gs_us'), f('dg_us')
    return n, step, step - iop - gs - dg, dg, iop, gs
def pct(R, a, b):
    s = sorted(R[v]['step_us'] for v in range(a, b + 1) if v in R)
    if not s: return 0, 0, 0, 0, 0
    n = len(s)
    return s[n // 2] / 1000, s[n * 9 // 10] / 1000, s[n * 99 // 100] / 1000, s[-1] / 1000, max(range(a, b + 1), key=lambda v: R[v]['step_us'] if v in R else -1)
hdr = ' '.join('%-36s' % ('%s step (logic draw iop gs)' % 'AB'[i]) for i in range(len(runs)))
print('%-20s %5s  %s%s' % ('part', 'n', hdr, '  delta step / logic' if len(runs) > 1 else ''))
for name, a, b in parts:
    st = [stat(R, a, b) for R in runs]
    line = '%-20s %5d  ' % (name, st[0][0]) + ' '.join('%6.2f (%5.2f %5.2f %4.2f %4.2f)        ' % s[1:] for s in st)
    if len(runs) > 1: line += '%+5.2f / %+5.2f' % (st[1][1] - st[0][1], st[1][2] - st[0][2])
    print(line)
print()
print('%-20s %-14s %8s %8s %7s' % ('model window', 'vb', 'PSP A', 'model', 'A/model') + ('  %8s' % 'PSP B' if len(runs) > 1 else ''))
ra = []
for name, a, b, m in model:
    sa = stat(runs[0], a, b)
    if sa[0]: ra.append(sa[1] / m)
    print('%-20s %-14s %8.2f %8.2f %7.3f' % (name, '%d-%d' % (a, b), sa[1], m, sa[1] / m if sa[0] else 0) +
          ('  %8.2f' % stat(runs[1], a, b)[1] if len(runs) > 1 else ''))
if ra: print('%-20s %-14s %8s %8.2f %7.3f' % ('mean of windows', '', '', sum(m for *_, m in model) / len(model), sum(ra) / len(ra)))
print()
for i, R in enumerate(runs):
    p50, p90, p99, mx, at = pct(R, 31801, 37800)
    s = [R[v] for v in range(31801, 37801) if v in R]
    full = [x for x in s if x.get('full')]; lite = [x for x in s if x.get('lite')]
    dn = sum(x.get('disc_n', 0) for x in s); dk = sum(x.get('disc_kb', 0) for x in s); du = sum(x.get('disc_us', 0) for x in s)
    print('%s: %d steps; p50 %.2f, p90 %.2f, p99 %.2f, max %.2f (vb %d); full %d (%.2f ms), reduced %d (%.2f ms); pack reads %d, %d KB, %.1f ms'
          % ('AB'[i], len(s), p50, p90, p99, mx, at, len(full), sum(x['step_us'] for x in full) / max(1, len(full)) / 1000,
             len(lite), sum(x['step_us'] for x in lite) / max(1, len(lite)) / 1000, dn, dk, du / 1000))
    slow = sorted(s, key=lambda x: -x['step_us'])[:8]
    print('   slowest: ' + ', '.join('vb %d %.1f' % (x['vb'], x['step_us'] / 1000) for x in slow))
