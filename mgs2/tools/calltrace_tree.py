#!/usr/bin/env python3
"""calltrace_tree.py <calltrace> [--inside] [--root ADDR] [--depth N] [--min N]: the call tree of one vblank from boot.c MGS2_CALLTRACE (a GEN_COVER
build), rebuilt from each entry's guest $sp (a callee enters with $sp at or below its caller's), per thread.  Calls with the same path are
merged and counted.  --inside keeps only calls inside the draw pipeline (a GEN_DG_TRACE build); --root starts the tree at that function.
Addresses and counts only."""
import collections, sys
a = sys.argv[1:]
path = a[0]
inside = '--inside' in a
root = int(a[a.index('--root') + 1], 16) if '--root' in a else None
maxd = int(a[a.index('--depth') + 1]) if '--depth' in a else 12
minc = int(a[a.index('--min') + 1]) if '--min' in a else 1
stacks = collections.defaultdict(list)        # thread -> [(entry sp, name)]
count = collections.Counter(); first = {}; n = 0
for l in open(path):
    p = l.split()
    if len(p) < 15: continue
    unit, fa, sp, th, ins = p[0], int(p[1], 16), int(p[3], 16), int(p[12]), int(p[14])
    if inside and ins != 1: continue
    name = ('%06x' % fa) if unit == 'elf' else '%s:%06x' % (unit.split('/')[-1].replace('gen_s_', '').replace('.h', ''), fa)
    st = stacks[th]
    while st and st[-1][0] <= sp: st.pop()
    st.append((sp, name))
    key = tuple(x[1] for x in st)
    if root is not None:
        names = [x[1] for x in st]
        r = '%06x' % root
        if r not in names: continue
        key = tuple(names[names.index(r):])
    count[key] += 1; first.setdefault(key, n); n += 1
print('%d calls, %d distinct paths' % (sum(count.values()), len(count)))
order = lambda k: tuple(first.get(k[:i], first[k]) for i in range(1, len(k) + 1))   # each subtree under its parent, in first-call order
for key in sorted(count, key=order):
    if len(key) > maxd or count[key] < minc: continue
    print('  ' * (len(key) - 1) + '%s  x%d' % (key[-1], count[key]))
