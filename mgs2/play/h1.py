"""h1.py: the play probe's working-set facts (audit H1) over all sessions; see gil.py for the inputs."""
import sys, collections
sys.path.insert(0, '/home/claude/play'); from gil import *
B4 = 'idle tour1 tour2 tour3 crawl fpv fire punch menus codec pause roll'.split(); B5 = 'rand1 rand2 rand3 rand4 rand5 rand6 death'.split()
PLAY = B4 + B5; ALL = ['tut'] + PLAY
C = {t: load(t) for t in ALL}; N = {t: STEPS[t] for t in ALL}
print('== union of the sessions, by unit (PSP -O2 sizes, no PGO)')
for name, ts in (('free play (19 sessions)', PLAY), ('tutorial window', ['tut']), ('both', ALL)):
    ks = set().union(*(set(C[t]) for t in ts))
    for u in ('elf', 'w00a'):
        k = {f for (uu, f) in ks if uu == u}; out = k - red[u]
        print('  %-24s %-4s: %4d functions %.2f MB; not in the reduced build: %4d, %.2f MB' % (name, u, len(k), sum(U[u][1].get(f, 0) for f in k) / 1e6, len(out), sum(U[u][1].get(f, 0) for f in out) / 1e6))
print('== hot set over free play (all sessions, time-weighted) and its leftovers per step')
tot = collections.Counter()
for t in PLAY: tot.update(C[t])
steps = sum(N[t] for t in PLAY); T = sum(tot.values()); order = sorted(tot, key=lambda k: -tot[k])
acc = sz = 0; marks = [0.9, 0.99, 0.999, 0.9999, 0.99999, 1.0]; mi = 0
for i, k in enumerate(order):
    acc += tot[k]; sz += size(k)
    while mi < len(marks) and acc >= marks[mi] * T - 1e-9:
        print('  %8.3f%%: %5d functions, %5.2f MB; the rest %9.1f instructions per step' % (100 * marks[mi], i + 1, sz / 1e6, (T - acc) / steps)); mi += 1
print('== leave one out: compiled = everything the other sessions (tutorial included) ran')
for t in PLAY:
    oth = set().union(*(set(C[s]) for s in ALL if s != t)); out = {k: v for k, v in C[t].items() if k not in oth}
    o = sum(out.values()) / N[t]
    print('  %-6s %4d functions, %3d unseen (%6.1f KB), %8.1f instructions per step unseen (%.3f%% of its %.0f)' % (t, len(C[t]), len(out), sum(size(k) for k in out) / 1e3, o, 100 * o / (sum(C[t].values()) / N[t]), sum(C[t].values()) / N[t]))
print('== growth (tutorial, then sessions in order)')
seen = set()
for t in ALL:
    new = set(C[t]) - seen; seen |= set(C[t])
    print('  + %-6s %4d new (%6.1f KB), total %4d (%.2f MB)' % (t, len(new), sum(size(k) for k in new) / 1e3, len(seen), sum(size(k) for k in seen) / 1e6))
print('== fit to a code budget: functions chosen by their highest per-step rate in any session / size; what is left to an interpreter')
rate = collections.defaultdict(float)
for t in ALL:
    for k, v in C[t].items(): rate[k] = max(rate[k], v / N[t])
cand = sorted(rate, key=lambda k: -rate[k] / max(1, size(k)))
for budget in (3.0e6, 4.0e6, 4.5e6, 5.0e6, 5.5e6, 6.0e6, 7.0e6, 8.0e6):
    comp = set(); s = 0
    for k in cand:
        if s + size(k) <= budget: comp.add(k); s += size(k)
    left = {t: sum(v for k, v in C[t].items() if k not in comp) / N[t] for t in ALL}
    pl = [left[t] for t in PLAY]
    print('  budget %.1f MB (ELF + w00a): %4d functions; interpreted per step: tutorial %7.0f, free play mean %7.0f, max %7.0f (%s)' % (
        budget / 1e6, len(comp), left['tut'], sum(pl) / len(pl), max(pl), max(PLAY, key=lambda t: left[t])))
