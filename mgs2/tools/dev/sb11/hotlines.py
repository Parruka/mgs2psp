# hotlines.py <dump> <elf> <min execs a step> [steps] [func ...]: the 64-byte code lines run at least that often a step, by function
# (the I-cache footprint of a hot loop), with each line's set; the functions' lines that share a set with 2+ other hot lines
import sys, subprocess, bisect, collections
dump, elf, mn = sys.argv[1], sys.argv[2], float(sys.argv[3]); steps = float(sys.argv[4]) if len(sys.argv) > 4 else 80; want = set(sys.argv[5:])
syms = []
for l in subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-S', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    t = l.split()
    if len(t) == 4 and t[2] in 'tTwW': syms.append((int(t[0], 16), int(t[1], 16), t[3]))
syms.sort(); st = [s[0] for s in syms]
def fn(pc):
    i = bisect.bisect_right(st, pc) - 1
    return syms[i][2] if i >= 0 and pc < syms[i][0] + max(syms[i][1], 4) else '?'
ln = collections.defaultdict(lambda: [0, 0])
for l in open(dump):
    if l.startswith('#'): continue
    t = l.split(); pc = int(t[0], 16); c = int(t[1]); im = int(t[2])
    a = ln[pc >> 6]; a[0] = max(a[0], c); a[1] += im
hot = {k: v for k, v in ln.items() if v[0] / steps >= mn}
byf = collections.defaultdict(list)
for k in hot: byf[fn(k << 6)].append(k)
tot = 0
for f in sorted(byf, key=lambda f: -len(byf[f])):
    if want and f not in want: continue
    tot += len(byf[f])
    print('%-24s %3d lines (%5d B), I-misses %6.0f a step' % (f, len(byf[f]), len(byf[f]) * 64, sum(hot[k][1] for k in byf[f]) / steps))
print('lines run >= %g a step: %d (%d KB) in all; listed %d (%d B)' % (mn, len(hot), len(hot) * 64 // 1024, tot, tot * 64))
sets = collections.Counter((k & 127) for k in hot if (not want or fn(k << 6) in want))
print('sets holding 3+ of the listed hot lines: %d; 4+: %d' % (sum(1 for s in sets.values() if s >= 3), sum(1 for s in sets.values() if s >= 4)))
