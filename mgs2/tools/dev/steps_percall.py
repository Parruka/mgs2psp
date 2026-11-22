#!/usr/bin/env python3
"""steps_percall.py <steps.csv> [from_vb [to_vb]] [--max-ms N]: per-kind means with the GS walk moved to the call that built the packets,
a paired difference with its interval, and an A/A control.
The GS walk at vblank v walks the packets of call v - 1 (one pipeline call per vblank on the deck), so a step's own walk is the next line's
GS column.  A step's kind: from v21 steps.csv has the step's full and reduced pipeline calls (columns full, lite); before that (v20) the
configuration says it (cfg 8 = reduced renderer off).  Prints, per kind: draw, the walk of its own packets, and the step with its own walk
(step - gs(v) + gs(v + 1)).  Then:
 - paired: every two neighbouring vblanks of different kinds (full / reduced), the full step minus the reduced one (with own walks): mean,
   95 % interval (normal approximation of the pair mean) and the sign count;
 - A/A (research_review.md M16): each kind's steps split into two interleaved halves (1st, 3rd, ... against 2nd, 4th, ...): the difference
   of their means with its 95 % interval.  A difference between kinds smaller than the A/A spread is noise.
--max-ms N leaves out a step whose own or next line is over N ms (v19 and v20 have one 1.4 s step at vb 24667: use --max-ms 300)."""
import csv, math, sys
args = sys.argv[1:]; mx = None
if '--max-ms' in args: i = args.index('--max-ms'); mx = float(args[i + 1]) * 1000; del args[i:i + 2]
rows = list(csv.DictReader(open(args[0])))
lo = int(args[1]) if len(args) > 1 else 0
hi = int(args[2]) if len(args) > 2 else 1 << 30
by = {int(r['vb']): r for r in rows}
def kind(r):
    if 'full' in r and r['full'] != '':
        f, l = int(r['full']), int(r['lite'])
        return 'none' if not f and not l else 'reduced' if not f else 'full' if not l else 'mixed'
    return 'full' if r['cfg'] == '8' else 'reduced'
step = {}
for r in rows:
    v = int(r['vb']); n = by.get(v + 1)
    if v < lo or v >= hi or not n: continue
    if mx is not None and (int(r['step_us']) > mx or int(n['step_us']) > mx): continue
    step[v] = (kind(r), int(r['dg_us']), int(n['gs_us']), int(r['step_us']) - int(r['gs_us']) + int(n['gs_us']), int(r['step_us']))
def ci(xs):
    k = len(xs); m = sum(xs) / k
    if k < 2: return m, float('nan')
    sd = math.sqrt(sum((x - m) ** 2 for x in xs) / (k - 1)); return m, 1.96 * sd / math.sqrt(k)
acc = {}
for v in sorted(step):
    k, dg, gs, own, logged = step[v]; acc.setdefault(k, []).append((dg, gs, own, logged))
for k in sorted(acc):
    a = acc[k]; n = len(a)
    print('%-7s %5d steps: draw %.2f ms, GS walk of its packets %.2f ms; step with its own walk %.2f ms (as logged %.2f)'
          % (k, n, sum(x[0] for x in a) / n / 1000, sum(x[1] for x in a) / n / 1000, sum(x[2] for x in a) / n / 1000, sum(x[3] for x in a) / n / 1000))
pd = []                                  # pairs (v, v + 1) of different kinds, no vblank in two pairs
for v in sorted(step):
    w = step.get(v + 1)
    if w and {step[v][0], w[0]} == {'full', 'reduced'} and not (pd and pd[-1][0] == v):
        f, r = (step[v], w) if step[v][0] == 'full' else (w, step[v]); pd.append((v + 1, (f[3] - r[3]) / 1000))
if pd:
    m, h = ci([d for _, d in pd]); pos = sum(1 for _, d in pd if d > 0)
    print('paired full - reduced (%d pairs of neighbouring vblanks): %.2f ms, 95%% interval %.2f..%.2f; full slower in %d of %d pairs'
          % (len(pd), m, m - h, m + h, pos, len(pd)))
for k in sorted(acc):
    a = [x[2] / 1000 for x in acc[k]]
    if len(a) < 4: continue
    h1, h2 = a[0::2], a[1::2]; m1, e1 = ci(h1); m2, e2 = ci(h2); d = m1 - m2; e = math.sqrt((e1 / 1.96) ** 2 + (e2 / 1.96) ** 2) * 1.96
    print('A/A %-7s interleaved halves (%d / %d steps): %.2f - %.2f = %.2f ms, 95%% interval %.2f..%.2f' % (k, len(h1), len(h2), m1, m2, d, d - e, d + e))
    qa = []                                 # the paired control: same-kind steps two vblanks apart, each step in one pair
    for v in sorted(step):
        w = step.get(v + 2)
        if step[v][0] == k and w and w[0] == k and not (qa and qa[-1][0] >= v): qa.append((v + 2, (step[v][3] - w[3]) / 1000))
    if len(qa) >= 2:
        m, h = ci([d for _, d in qa])
        print('A/A %-7s paired, steps two vblanks apart (%d pairs): %.2f ms, 95%% interval %.2f..%.2f' % (k, len(qa), m, m - h, m + h))
