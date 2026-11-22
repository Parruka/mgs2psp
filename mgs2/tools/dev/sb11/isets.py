# isets.py <dump off> <dump on> <elf> [steps] [nsets]: I-misses of two PspProf dumps of one build by I-cache set (16 KB, 2-way, 64 B
# lines: set = pc >> 6 & 127), the sets that gained most, and in each the functions that miss there (on)
import sys, subprocess, bisect, collections
d0, d1, elf = sys.argv[1], sys.argv[2], sys.argv[3]; steps = float(sys.argv[4]) if len(sys.argv) > 4 else 80; ns = int(sys.argv[5]) if len(sys.argv) > 5 else 12
syms = []
for l in subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-S', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    t = l.split()
    if len(t) == 4 and t[2] in 'tTwW': syms.append((int(t[0], 16), int(t[1], 16), t[3]))
syms.sort(); st = [s[0] for s in syms]
def fn(pc):
    i = bisect.bisect_right(st, pc) - 1
    return syms[i][2] if i >= 0 and pc < syms[i][0] + max(syms[i][1], 4) else '?'
def rd(p):
    s = collections.Counter(); f = collections.defaultdict(collections.Counter); lines = collections.defaultdict(set)
    for l in open(p):
        if l.startswith('#'): continue
        t = l.split(); pc = int(t[0], 16); im = int(t[2])
        if im: k = pc >> 6 & 127; s[k] += im; f[k][fn(pc)] += im; lines[k].add(pc >> 6)
    return s, f, lines
s0, f0, l0 = rd(d0); s1, f1, l1 = rd(d1)
ms = 68.2 / 333e3 / steps * 1e3  # ms a step per miss... (68.2 cycles per miss at 333 MHz)
print('I-misses a step: off %.0f, on %.0f (%.2f -> %.2f ms)' % (sum(s0.values()) / steps, sum(s1.values()) / steps, sum(s0.values()) * 68.2 / 333e3 / steps, sum(s1.values()) * 68.2 / 333e3 / steps))
gain = sorted(range(128), key=lambda k: -(s1[k] - s0[k]))
for k in gain[:ns]:
    print('set %3d: off %7.0f on %7.0f a step (+%.3f ms); lines missing on %d' % (k, s0[k] / steps, s1[k] / steps, (s1[k] - s0[k]) * 68.2 / 333e3 / steps, len(l1[k])))
    print('    on: ' + ', '.join('%s %.0f' % (n, c / steps) for n, c in f1[k].most_common(7)))
    print('   off: ' + ', '.join('%s %.0f' % (n, c / steps) for n, c in f0[k].most_common(5)))
# by function: I-misses a step
F0 = collections.Counter(); F1 = collections.Counter()
for k in range(128):
    F0.update(f0[k]); F1.update(f1[k])
print('functions, I-misses a step (on - off largest):')
for n in sorted(set(F0) | set(F1), key=lambda n: -(F1[n] - F0[n]))[:20]:
    print('  %-24s %7.0f -> %7.0f  (%+.3f ms)' % (n, F0[n] / steps, F1[n] / steps, (F1[n] - F0[n]) * 68.2 / 333e3 / steps))
