#!/usr/bin/env python3
"""gsref_entries.py <gsref stats> <SLES_503.83> [vblank]: per VU1 entry point (named by the ELF's .DVP overlay as gscap.py does, e.g. P04+df0;
anything outside the ELF's pieces, such as the stage image's program, keeps its guest address) the calls, GS primitives, pixels written and
the GS states it draws with, from gsref's "entry ..." counts.  Numbers only."""
import collections, struct, sys

stats, elf = sys.argv[1], sys.argv[2]
want = sys.argv[3] if len(sys.argv) > 3 else None
d = open(elf, 'rb').read()
e_shoff, = struct.unpack_from('<I', d, 0x20); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
secs = [struct.unpack_from('<IIIIII', d, e_shoff + i * shentsize) for i in range(shnum)]
def secname(o):
    base = secs[shstrndx][4]; return d[base + o:d.index(b'\0', base + o)].decode()
sec = {secname(s[0]): s for s in secs}
tab = sec['.DVP.ovlytab']; strt = sec['.DVP.ovlystrtab']
pieces = []; prog = -1
for i in range(0, tab[5], 12):
    a, ram, vu = struct.unpack_from('<III', d, tab[4] + i)
    name = d[strt[4] + a:d.index(b'\0', strt[4] + a)].decode()
    if vu == 0: prog += 1
    pieces.append((ram, sec[name][5], prog, vu))
def ename(e):
    for ram, size, p, vu in pieces:
        if ram <= e < ram + size: return 'P%02d+%x' % (p, vu + e - ram)
    return 'ram %06x' % e

E = collections.defaultdict(lambda: {'calls': 0, 'prims': 0, 'px': 0, 'states': collections.Counter()})
for l in open(stats):
    p = l.split()
    if len(p) < 5 or p[0] != 'vb' or p[2] != 'entry': continue
    if want and p[1] != want: continue
    e = int(p[3], 16); n = int(p[-1]); rest = p[4:-1]
    if rest == ['calls']: E[e]['calls'] += n
    elif rest == ['prims']: E[e]['prims'] += n
    elif rest == ['px']: E[e]['px'] += n
    elif rest[0] == 'state': E[e]['states'][' '.join(rest[1:])] += n
tot = sum(v['px'] for v in E.values()) or 1
print('%-12s %7s %8s %10s %5s  top states (prims)' % ('entry', 'calls', 'prims', 'pixels', 'px%'))
for e, v in sorted(E.items(), key=lambda kv: -kv[1]['px']):
    st = '; '.join('%s x%d' % (k, n) for k, n in v['states'].most_common(3))
    print('%-12s %7d %8d %10d %5.1f  %s' % (ename(e), v['calls'], v['prims'], v['px'], 100.0 * v['px'] / tot, st))
