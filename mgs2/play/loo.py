"""loo.py <tag>...: leave-one-out: for each session, the compiled set = functions the OTHER sessions ran (all of them, or only those at or
above a per-step instruction threshold); what the left-out session then runs outside that set (functions, guest instructions per step)."""
import sys, collections
exec(open('/home/claude/play/ws.py').read().split("tags = sys.argv[1:]")[0])
tags = sys.argv[1:]
C = {}
for t in tags:
    c = collections.Counter()
    for ln in open('/home/claude/play/run/gi_%s.txt' % t):
        pc, a, d = ln.split(); pc = int(pc, 16); u = unit_of(pc); f = pc2f.get((u, pc))
        if f is not None: c[(u, f)] += int(a) + int(d)
    C[t] = c
size = lambda k: U[k[0]][1].get(k[1], 0)
N = 2999
for th in [0, 1, 10, 100]:
    print('compiled set = functions the other sessions ran at >= %g instructions per step (summed over them)' % th)
    tot_out = []; 
    for t in tags:
        oth = collections.Counter()
        for s in tags:
            if s != t: oth.update(C[s])
        n_oth = N * (len(tags) - 1)
        comp = {k for k, v in oth.items() if v / n_oth >= th} if th > 0 else set(oth)
        out = {k: v for k, v in C[t].items() if k not in comp}
        o = sum(out.values()) / N; tot_out.append(o)
        print('  %-6s: set %4d functions %.2f MB; session runs %4d functions, %3d outside the set (%.1f KB), %9.1f instructions per step outside (%.4f%%)' % (
            t, len(comp), sum(size(k) for k in comp) / 1e6, len(C[t]), len(out), sum(size(k) for k in out) / 1e3, o, 100 * o / (sum(C[t].values()) / N)))
    print('  mean outside %.1f, max %.1f instructions per step' % (sum(tot_out) / len(tot_out), max(tot_out)))
# growth: functions seen as sessions are added (in the listed order)
seen = set(); print('growth of the set (listed order):')
for t in tags:
    new = set(C[t]) - seen; seen |= set(C[t])
    print('  + %-6s %4d new functions (%.1f KB), total %4d (%.2f MB)' % (t, len(new), sum(size(k) for k in new) / 1e3, len(seen), sum(size(k) for k in seen) / 1e6))
