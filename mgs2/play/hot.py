"""hot.py <tag>...: hot-set sizes for the sessions together: functions by guest instructions (outside + inside the draw pipeline, per
step over all sessions), how much PSP code (h1size full objects, -O2, no PGO) covers each share, and what is left to an interpreter."""
import sys, collections
sys.argv = [sys.argv[0]] + sys.argv[1:]
exec(open('/home/claude/play/ws.py').read().split("tags = sys.argv[1:]")[0])
tags = sys.argv[1:]; steps = 2999 * len(tags)
cnt = collections.Counter(); ses = collections.defaultdict(set)
for t in tags:
    for ln in open('/home/claude/play/run/gi_%s.txt' % t):
        pc, c, d = ln.split(); pc = int(pc, 16); u = unit_of(pc); f = pc2f.get((u, pc))
        if f is None: continue
        cnt[(u, f)] += int(c) + int(d); ses[(u, f)].add(t)
tot = sum(cnt.values()); size = lambda k: U[k[0]][1].get(k[1], 0)
order = sorted(cnt, key=lambda k: -cnt[k])
print('sessions %d, %d steps; %d functions ran (%.2f MB of PSP code); %.0f guest instructions per step' % (len(tags), steps, len(order), sum(size(k) for k in order) / 1e6, tot / steps))
acc = 0; marks = [0.9, 0.99, 0.999, 0.9999, 0.99999, 1.0]; mi = 0; sz = 0
for i, k in enumerate(order):
    acc += cnt[k]; sz += size(k)
    while mi < len(marks) and acc >= marks[mi] * tot - 1e-9:
        print('  %8.3f%% of instructions: %5d functions, %6.2f MB; the rest %10.1f instructions per step' % (100 * marks[mi], i + 1, sz / 1e6, (tot - acc) / steps)); mi += 1
# by threshold of instructions per step
for th in [1000, 100, 10, 1, 0.1]:
    hot = [k for k in order if cnt[k] / steps >= th]
    rest = sum(cnt[k] for k in order if cnt[k] / steps < th)
    print('  functions at >= %g instructions per step: %5d, %6.2f MB compiled; the others: %5d functions, %.2f MB, %.1f instructions per step' % (
        th, len(hot), sum(size(k) for k in hot) / 1e6, len(order) - len(hot), sum(size(k) for k in order if cnt[k] / steps < th) / 1e6, rest / steps))
allm = [k for k in order if len(ses[k]) == len(tags)]
print('  in every session: %d functions, %.2f MB; in some only: %d, %.2f MB' % (len(allm), sum(size(k) for k in allm) / 1e6, len(order) - len(allm), sum(size(k) for k in order if len(ses[k]) < len(tags)) / 1e6))
