# dhsets.py <dhist> <elfs comma> <set>...: the lines with the most D-misses in the given D-cache sets, with their symbols
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
syms = []
for e in sys.argv[2].split(','):
    for ln in subprocess.run([NM, '-S', '-n', e], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 4 and p[2] in 'bBdDgGsSrRvV': syms.append((int(p[0], 16), int(p[1], 16), p[3]))
syms.sort(); addrs = [x[0] for x in syms]
want = set(int(x) for x in sys.argv[3:])
rows = collections.defaultdict(list)
for ln in open(sys.argv[1]):
    a, c = ln.split(); a = int(a, 16); c = int(c); s = (a >> 6) & 127
    if s in want:
        i = bisect.bisect_right(addrs, a) - 1
        nm = '%s+%x' % (syms[i][2], a - syms[i][0]) if i >= 0 and a < syms[i][0] + max(syms[i][1], 64) else '?'
        rows[s].append((c, a, nm))
for s in sorted(rows):
    r = sorted(rows[s], reverse=True)
    print('set %d: %d misses on %d lines; top: %s' % (s, sum(x[0] for x in r), len(r), ', '.join('%08x %s %d' % (a, nm, c) for c, a, nm in r[:8])))
