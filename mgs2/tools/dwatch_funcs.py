#!/usr/bin/env python3
"""dwatch_funcs.py <out> <steps> <elf> [<elf> ...] [--names a,b,c,d] [--top N]: PspProf PSPPROF_DWATCH ranges (<out>.dwatch: range pc count)
summed per function next to the function's instructions (<out>), per logic step.  Numbers only."""
import bisect, collections, subprocess, sys
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
argv = sys.argv[1:]
names = argv[argv.index('--names') + 1].split(',') if '--names' in argv else ['r0', 'r1', 'r2', 'r3']
top = int(argv[argv.index('--top') + 1]) if '--top' in argv else 25
args = []; skip = False
for a in argv:
    if skip: skip = False; continue
    if a in ('--names', '--top'): skip = True; continue
    args.append(a)
prof, steps, elfs = args[0], float(args[1]), args[2:]
syms = {}
for e in elfs:
    for ln in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.split('\n'):
        p = ln.split()
        if len(p) == 3 and p[1] in 'tTwW': syms.setdefault(int(p[0], 16), p[2])
syms = sorted(syms.items()); A = [a for a, _ in syms]
name = lambda pc: syms[bisect.bisect_right(A, pc) - 1][1] if bisect.bisect_right(A, pc) else '?'
ins = collections.Counter()
for ln in open(prof):
    if ln[0] == '#': continue
    pc, c, _, _ = ln.split(); ins[name(int(pc, 16))] += int(c)
acc = collections.defaultdict(collections.Counter); tot = collections.Counter()
for ln in open(prof + '.dwatch'):
    r, pc, n = ln.split(); r = int(r); n = int(n); acc[r][name(int(pc, 16))] += n; tot[r] += n
print('instructions per step %.0f k; accesses per step: %s' % (sum(ins.values()) / steps / 1e3, ', '.join('%s %.0f k' % (names[r], tot[r] / steps / 1e3) for r in sorted(tot))))
for r in sorted(acc):
    print('\n%s: top functions (accesses k/step, function k instr/step, share of its instructions)' % names[r])
    for f, n in acc[r].most_common(top):
        print('  %-28s %7.1f  %7.1f  %4.0f%%' % (f, n / steps / 1e3, ins[f] / steps / 1e3, 100.0 * n / max(ins[f], 1)))
