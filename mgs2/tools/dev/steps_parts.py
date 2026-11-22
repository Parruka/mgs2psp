#!/usr/bin/env python3
"""steps_parts.py <steps.csv A> <steps.csv B>: two runs of v25f's free route step for step (A the older): ms per step by part of the
route (stand, walk, crawl, punch and fire, first person, alert; then the model's four 100-step windows) with logic = step - draw - IOP - GS,
p50 / p90 / p99 / max, full and reduced steps, pack reads, and the per-step difference B - A (speed_notes §44, §47)."""
import csv, sys
def load(p):
    R = {}
    for r in csv.DictReader(open(p)):
        R[int(r['vb'])] = {k: int(v) for k, v in r.items()}
    return R
A = load(sys.argv[1]); B = load(sys.argv[2])
parts = [('stand', 30501, 31000), ('walk', 31001, 32800), ('crawl', 32801, 33200), ('punch and fire', 33201, 33600),
         ('first person', 33601, 34200), ('alert', 34201, 36500), ('ALL', 30501, 36500),
         ('model walk', 31501, 31600), ('model crawl', 32901, 33000), ('model first person', 33801, 33900), ('model alert', 35501, 35600)]
def stat(R, a, b):
    s = [R[v] for v in range(a, b + 1) if v in R]
    n = len(s)
    step = sum(x['step_us'] for x in s) / n / 1000
    iop = sum(x['iop_us'] for x in s) / n / 1000
    gs = sum(x['gs_us'] for x in s) / n / 1000
    dg = sum(x['dg_us'] for x in s) / n / 1000
    logic = step - iop - gs - dg
    return n, step, logic, dg, iop, gs
print('%-20s %5s  %-38s  %-38s  %s' % ('part', 'n', 'A step (logic draw iop gs)', 'B step (logic draw iop gs)', 'delta step / logic'))
for name, a, b in parts:
    sa = stat(A, a, b); sb = stat(B, a, b)
    print('%-20s %5d  %6.2f (%5.2f %5.2f %4.2f %4.2f)        %6.2f (%5.2f %5.2f %4.2f %4.2f)        %+5.2f / %+5.2f' % (name, sa[0], *sa[1:], *sb[1:], sb[1] - sa[1], sb[2] - sa[2]))
def pct(R, a, b):
    s = sorted(R[v]['step_us'] for v in range(a, b + 1) if v in R)
    n = len(s)
    q = lambda p: s[min(n - 1, int(p * n))] / 1000
    mx = max(range(a, b + 1), key=lambda v: R[v]['step_us'] if v in R else 0)
    return q(0.5), q(0.9), q(0.99), s[-1] / 1000, mx
for nm, R in (('A', A), ('B', B)):
    p50, p90, p99, mx, mvb = pct(R, 30501, 36500)
    r = R[mvb]
    print('%s: p50 %.2f p90 %.2f p99 %.2f max %.2f (vb %d: draw %.1f iop %.1f gs %.1f disc %d)' % (nm, p50, p90, p99, mx, mvb, r['dg_us'] / 1000, r['iop_us'] / 1000, r['gs_us'] / 1000, r['disc_n']))
for nm, R in (('A', A), ('B', B)):
    for flag in ('full', 'lite'):
        s = [R[v] for v in range(30501, 36501) if R[v][flag]]
        print('%s %s: %d steps, step %.2f, draw %.2f' % (nm, flag, len(s), sum(x['step_us'] for x in s) / len(s) / 1000, sum(x['dg_us'] for x in s) / len(s) / 1000))
    print(nm, 'disc reads in timed steps:', sum(R[v]['disc_n'] for v in range(30501, 36501)), 'wait sum', sum(R[v]['wait'] for v in range(30501, 36501)))
# per-step delta distribution
d = sorted(B[v]['step_us'] - A[v]['step_us'] for v in range(30501, 36501))
n = len(d)
print('per-step delta B - A (ms): p10 %.2f p50 %.2f p90 %.2f min %.2f max %.2f' % (d[n // 10] / 1000, d[n // 2] / 1000, d[9 * n // 10] / 1000, d[0] / 1000, d[-1] / 1000))
dl = sorted((B[v]['step_us'] - B[v]['iop_us'] - B[v]['gs_us'] - B[v]['dg_us']) - (A[v]['step_us'] - A[v]['iop_us'] - A[v]['gs_us'] - A[v]['dg_us']) for v in range(30501, 36501))
print('per-step logic delta (ms): p10 %.2f p50 %.2f p90 %.2f' % (dl[n // 10] / 1000, dl[n // 2] / 1000, dl[9 * n // 10] / 1000))
