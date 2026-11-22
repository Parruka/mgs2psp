"""h1b.py: fit with separate budgets for the ELF (resident) and the w00a overlay (per stage); same choice rule as h1.py"""
import sys, collections
sys.path.insert(0, '/home/claude/play'); from gil import *
B4 = 'idle tour1 tour2 tour3 crawl fpv fire punch menus codec pause roll'.split(); B5 = 'rand1 rand2 rand3 rand4 rand5 rand6 death'.split()
PLAY = B4 + B5; ALL = ['tut'] + PLAY
C = {t: load(t) for t in ALL}; N = {t: STEPS[t] for t in ALL}
rate = collections.defaultdict(float)
for t in ALL:
    for k, v in C[t].items(): rate[k] = max(rate[k], v / N[t])
cand = sorted(rate, key=lambda k: -rate[k] / max(1, size(k)))
for be, bw in ((2.31e6, 6.27e6), (2.0e6, 4.2e6), (2.31e6, 4.2e6), (2.64e6, 4.2e6), (2.31e6, 3.5e6), (2.31e6, 3.0e6), (1.8e6, 3.5e6), (1.5e6, 3.0e6)):
    comp = set(); s = {'elf': 0, 'w00a': 0}; bud = {'elf': be, 'w00a': bw}
    for k in cand:
        if s[k[0]] + size(k) <= bud[k[0]]: comp.add(k); s[k[0]] += size(k)
    left = {t: sum(v for k, v in C[t].items() if k not in comp) / N[t] for t in ALL}
    pl = [left[t] for t in PLAY]
    print('ELF %.2f MB (%d functions, %.2f used), w00a %.2f MB (%d, %.2f used): interpreted per step: tutorial %6.0f, free play mean %6.0f, max %6.0f (%s)' % (
        be / 1e6, sum(1 for k in comp if k[0] == 'elf'), s['elf'] / 1e6, bw / 1e6, sum(1 for k in comp if k[0] == 'w00a'), s['w00a'] / 1e6, left['tut'], sum(pl) / len(pl), max(pl), max(PLAY, key=lambda t: left[t])))
# per-session instruction split again for all
for t in ALL:
    c, d = load(t, split=True)
    e = sum(v for k, v in c.items() if k[0] == 'elf') / N[t]; s_ = sum(v for k, v in c.items() if k[0] == 'w00a') / N[t]; g = sum(d.values()) / N[t]
    print('  %-6s steps %4d: logic %8.0f (ELF %7.0f, w00a %7.0f), draw pipeline %7.0f' % (t, N[t], e + s_, e, s_, g))
