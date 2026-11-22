#!/usr/bin/env python3
"""steps_cfg.py <steps.csv> [from_vb [to_vb]] [--base C] [--max-ms N]: per configuration index (column cfg) the mean step, draw (dg) and
GS walk, and for every other cfg a paired difference against the base cfg (default 0): each step of that cfg against the nearest step of
the base cfg of the same kind (full or reduced call; 2, 4 or 6 vblanks away), mean and 95 % interval of the pair differences (normal
approximation), with the sign count.  Built for v23's cycle (cfg 0 / 9 / 10 every vblank from deck vb 24000).

Two walk columns, because the GS walk in step v runs with step v's configuration on the packets of call v - 1:
  walk run  = the row's own GS column: compares configurations that change the walk itself (v23 cfg 9, the one-loop walk off);
  own walk  = the next row's GS column, the walk of this step's packets: compares configurations that change the packets (the reduced
              pipeline); "step + own walk" = step - gs(v) + gs(v + 1).
(Until 2026-10-23 the tool paired v - 1 / v + 1 neighbours and reported only the own walk, which put v23's walk gain on cfg 0.)"""
import csv, math, sys
args = sys.argv[1:]; mx = None; base = '0'
if '--max-ms' in args: i = args.index('--max-ms'); mx = float(args[i + 1]) * 1000; del args[i:i + 2]
if '--base' in args: i = args.index('--base'); base = args[i + 1]; del args[i:i + 2]
rows = list(csv.DictReader(open(args[0])))
lo = int(args[1]) if len(args) > 1 else 0
hi = int(args[2]) if len(args) > 2 else 1 << 30
by = {int(r['vb']): r for r in rows}
st = {}   # vb -> (cfg, kind, step, draw, walk run, own walk, step + own walk)
for r in rows:
    v = int(r['vb']); n = by.get(v + 1)
    if v < lo or v >= hi or not n: continue
    if mx is not None and (int(r['step_us']) > mx or int(n['step_us']) > mx): continue
    st[v] = (r['cfg'], r.get('full', '1'), int(r['step_us']), int(r['dg_us']), int(r['gs_us']), int(n['gs_us']), int(r['step_us']) - int(r['gs_us']) + int(n['gs_us']))
def ms(x): return x / 1000.0
cfgs = sorted({s[0] for s in st.values()}, key=lambda c: int(c))
print('cfg      n    step    draw  walk run  own walk  step + own walk   (ms)')
for c in cfgs:
    xs = [s for s in st.values() if s[0] == c]
    print('%-5s %5d  %6.2f  %6.2f  %8.2f  %8.2f  %15.2f' % (c, len(xs), *(ms(sum(s[k] for s in xs) / len(xs)) for k in (2, 3, 4, 5, 6))))
for c in cfgs:
    if c == base: continue
    for col, name in ((2, 'step'), (3, 'draw'), (4, 'walk run'), (5, 'own walk'), (6, 'step + own walk')):
        d = []
        for v, s in st.items():
            if s[0] != c: continue
            for w in (v - 2, v + 2, v - 4, v + 4, v - 6, v + 6):
                t = st.get(w)
                if t and t[0] == base and t[1] == s[1]: d.append(s[col] - t[col]); break
        if len(d) < 2: continue
        m = sum(d) / len(d); sd = math.sqrt(sum((x - m) ** 2 for x in d) / (len(d) - 1)); h = 1.96 * sd / math.sqrt(len(d))
        print('cfg %s - cfg %s, %-15s: %+7.3f ms  (95%% %+.3f .. %+.3f), %d pairs, %d positive' % (c, base, name, ms(m), ms(m - h), ms(m + h), len(d), sum(1 for x in d if x > 0)))
