#!/usr/bin/env python3
"""dacc_map.py <out>.dacc <steps> <elf> [--top N]: PspProf PSPPROF_DACC=1 data accesses per 64-byte line, summed per data symbol of the
harness ELF (psp-nm sizes), with the guest RAM g_mem split into the guest scratchpad (first 16 KB under GUEST_SPR0) and the rest, and
anything else in the 0x0b000000+ area counted as host stacks / heap.  Per logic step.  Numbers only."""
import bisect, collections, subprocess, sys
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
args = [a for a in sys.argv[1:] if not a.startswith('--')]
top = int(sys.argv[sys.argv.index('--top') + 1]) if '--top' in sys.argv else 30
if '--top' in sys.argv: args.remove(str(top))
dacc, steps, elf = args[0], float(args[1]), args[2]
syms = []
for ln in subprocess.run([NM, '-S', '-n', elf], capture_output=True, text=True).stdout.split('\n'):
    p = ln.split()
    if len(p) == 4 and p[2] in 'dDbBsSgGrRvV': syms.append((int(p[0], 16), int(p[1], 16), p[3]))
syms.sort(); A = [s[0] for s in syms]
gm = next(s for s in syms if s[2] == 'g_mem')
agg = collections.Counter(); tot = 0
for ln in open(dacc):
    a, n = ln.split(); a = int(a, 16); n = int(n); tot += n
    if gm[0] <= a < gm[0] + gm[1]: agg['g_mem: guest scratchpad (0-16 KB)' if a - gm[0] < 0x4000 else 'g_mem: guest RAM'] += n; continue
    i = bisect.bisect_right(A, a) - 1
    if i >= 0 and a < syms[i][0] + max(syms[i][1], 64): agg[syms[i][2]] += n; continue
    agg['host stack / heap' if a >= 0x0b000000 else 'other image data'] += n
print('data accesses per step: %.0f k' % (tot / steps / 1e3))
for k, n in agg.most_common(top): print('  %-36s %8.1f k  %5.1f%%' % (k, n / steps / 1e3, 100.0 * n / tot))
