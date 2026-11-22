#!/usr/bin/env python3
"""pgo_lost.py <PGO=use psp dir> <pgochk out dir> <PspProf dump>:<stage> ...  (gl20, 2026-11-18; FF8's lesson: a function that runs but
compiled without its profile is a silent loss).  Run it with python3 -I.

1. GCC's own list: the functions scripts/pgochk.sh's warnings-on compile names in a profile warning (control flow checksum mismatch,
   number of counters, ...): GCC dropped their profile.
2. The functions that ran in the PspProf windows (<dump>:<stage>, the overlay in memory then: w00a or w01a) and belong to a profiled
   object, by the input section the use build gave them: .text.hot.<fn> (profiled and hot), .text.<fn> (profiled but cool, or no
   profile: zero counts in the training window count as no profile under -fprofile-partial-training), .text.unlikely.<fn> (the profile
   said never).  Those not in .text.hot are listed with their instructions and I-misses in the windows, GCC's dropped ones marked.
   Parts named <fn>.cold (hot / cold split) that ran are counted apart: the profile said their blocks never run.
3. With <pgochk out dir>/gcno/*.gcno (pgochk.sh writes them): each function's counts in the build's .gcda by its ident, so the ones that
   ran in the windows but whose counts are all zero (not trained: -fprofile-partial-training compiles them with the static guess) or
   whose ident is not in the .gcda are named too.  GCC's "source locations ... have changed" is a warning only (the profile is used).
The lost list = the functions that ran in the windows and were compiled without their profile (dropped, not found, or all zero).
Prints a summary and writes <pgochk out dir>/lost.txt (one line per function that ran outside .text.hot) and lost_noprof.txt."""
import sys, os, re, subprocess, collections
BIN = '/home/claude/pspsdk/pspdev/bin/'
def run(*a): return subprocess.run(list(a), capture_output=True, text=True, check=True).stdout
D, W = sys.argv[1], sys.argv[2]
DUMPS = [a.rsplit(':', 1) for a in sys.argv[3:]]
PROF = ['gen.o', 'gen_dispatch.o', 'boot_rt.o', 'xl_psp.o', 'ge_fxthin.o'] + ['ge_%s.o' % f for f in
        'xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme xme'.split()]
OVL = {'w00a': 'gen_s_w00a.o', 'w01a': 'gen_s_w01a.o'}

# 1. GCC's drop list
drop = collections.defaultdict(dict)   # object -> function -> reason
moved = collections.defaultdict(dict)  # object -> function -> "source locations changed" (profile used)
for o in PROF + list(OVL.values()):
    p = os.path.join(W, 'warn_%s.txt' % o)
    if not os.path.exists(p): continue
    for l in open(p, errors='replace'):
        if 'warning' not in l: continue
        m = re.search(r"function [‘'`\"]?([A-Za-z_0-9.$@]+)[’'`\"]?", l)
        r = re.sub(r'^.*warning: ', '', l.strip())[:90]
        if 'source locations' in l: (moved if m else drop)[o][m.group(1) if m else '?'] = r; continue   # warning only
        if m: drop[o][m.group(1)] = r
        else: drop[o].setdefault('?', r)

# 3. trained or not, by the .gcno names of the .gcda idents
def fbase(f): return re.sub(r'(\.(isra|part|constprop|cold|lto_priv)(\.\d+)?)+$', '', f)
counts = {}   # (object, function) -> sum of its arc counts in the build's .gcda (None: its ident is not in the .gcda)
G = os.path.join(W, 'gcno')
for o in PROF + list(OVL.values()):
    gn, gd = os.path.join(G, o[:-2] + '.gcno'), os.path.join(D, o[:-2] + '.gcda')
    if not os.path.exists(gn): continue
    idn = {}
    for l in run(BIN + 'psp-gcov-dump', gn).splitlines():
        m = re.search(r"FUNCTION ident=(\d+),.*`([^']+)'", l)
        if m: idn[m.group(1)] = m.group(2)
    tot = {}; cur = None; arcs = False
    if os.path.exists(gd):
        for l in run(BIN + 'psp-gcov-dump', '-l', gd).splitlines():
            m = re.search(r'FUNCTION ident=(\d+)', l)
            if m: cur = m.group(1); tot[cur] = 0; arcs = False; continue
            if 'COUNTERS' in l: arcs = 'COUNTERS arcs' in l; continue
            if arcs and cur:
                m = re.match(r'.*:\s+\d+: ([\d ]+)$', l)
                if m: tot[cur] += sum(int(x) for x in m.group(1).split())
    for i, f in idn.items(): counts[(o, f)] = tot.get(i)

# the input section of each function symbol of the profiled objects
sect = {}   # (object, name) -> section
byname = collections.defaultdict(list)
for o in PROF + list(OVL.values()):
    p = os.path.join(D, o)
    if not os.path.exists(p): continue
    for l in run(BIN + 'psp-objdump', '-t', p).splitlines():
        q = l.split()
        if len(q) >= 6 and 'F' in q[1:3] and q[-3].startswith('.text'):
            sect[(o, q[-1])] = q[-3]; byname[q[-1]].append(o)

def syms(elf, lo=None, hi=None):   # sorted [(start, end, name)] of the text symbols with a size
    out = []
    for l in run(BIN + 'psp-nm', '-S', '--defined-only', elf).splitlines():
        q = l.split()
        if len(q) == 4 and q[2] in 'Tt' and int(q[1], 16):
            a = int(q[0], 16)
            if lo is None or lo <= a < hi: out.append((a, a + int(q[1], 16), q[3]))
    return sorted(out)
def secrange(elf, name):
    for l in run(BIN + 'psp-objdump', '-h', elf).splitlines():
        q = l.split()
        if len(q) > 4 and q[1] == name: return int(q[3], 16), int(q[3], 16) + int(q[2], 16)
elf = syms(os.path.join(D, 'MGS2T.elf'))
ovl = {}
for st in set(s for _, s in DUMPS):
    e = os.path.join(D, 'ovl_%s.elf' % st); lo, hi = secrange(e, '.ovl'); ovl[st] = syms(e, lo, hi)
import bisect
def find(tab, starts, pc):
    i = bisect.bisect_right(starts, pc) - 1
    return tab[i][2] if i >= 0 and pc < tab[i][1] else None

ran = collections.defaultdict(lambda: [0, 0])   # (object, name) -> [instructions, I-misses] over all windows
other = collections.Counter(); total = 0
for dump, st in DUMPS:
    tabs = [(elf, [t[0] for t in elf], None), (ovl[st], [t[0] for t in ovl[st]], OVL[st])]
    for l in open(dump):
        if l.startswith('#'): continue
        q = l.split(); pc, n, im = int(q[0], 16), int(q[1]), int(q[2]); total += n
        for tab, starts, o in tabs:
            f = find(tab, starts, pc)
            if f: break
        if not f: other['(no symbol)'] += n; continue
        base = f[:-5] if f.endswith('.cold') else f
        objs = [o] if o else [x for x in byname.get(base, []) if x not in OVL.values()]
        if not objs: other['(not a profiled object)'] += n; continue
        ran[(objs[0], f)][0] += n; ran[(objs[0], f)][1] += im

kinds = collections.Counter(); kinst = collections.Counter(); rows = []; noprof = []
for (o, f), (n, im) in ran.items():
    c = counts.get((o, fbase(f)), 'no gcno') if counts else 'no gcno'
    dr = drop[o].get(fbase(f), '')
    if dr or c is None or c == 0: noprof.append((n, im, o, f, dr or ('ident not in the .gcda' if c is None else 'all counts zero')))
    if f.endswith('.cold'): k = 'cold part'
    else:
        s = sect.get((o, f), '')
        k = 'hot' if s.startswith('.text.hot.') else 'unlikely' if s.startswith('.text.unlikely.') else 'normal' if s == '.text.' + f else 'shared section'
    kinds[k] += 1; kinst[k] += n
    if k != 'hot': rows.append((n, im, o, f, k, drop[o].get(f.replace('.cold', ''), '')))
rows.sort(reverse=True)
print('GCC dropped profiles (warnings-on compile):', sum(len(v) for v in drop.values()), 'functions:',
      ', '.join('%s %d' % (o, len(v)) for o, v in sorted(drop.items()) if v) or 'none')
print('windows: %d instructions; in profiled objects: %d functions ran' % (total, len(ran)))
for k in ('hot', 'normal', 'unlikely', 'shared section', 'cold part'):
    print('  %-15s %6d functions, %12d instructions (%.2f%%)' % (k, kinds[k], kinst[k], 100.0 * kinst[k] / max(total, 1)))
for k, n in other.items(): print('  %-25s %12d instructions (%.2f%%)' % (k, n, 100.0 * n / max(total, 1)))
dropped_ran = [r for r in rows if r[5]]
print('ran and dropped by GCC: %d functions, %d instructions' % (len(dropped_ran), sum(r[0] for r in dropped_ran)))
with open(os.path.join(W, 'lost.txt'), 'w') as fo:
    fo.write('# instructions imiss object function kind gcc-drop-reason (functions of profiled objects that ran outside .text.hot)\n')
    for r in rows: fo.write('%d %d %s %s %s %s\n' % r)
print('top 25 not hot:')
for r in rows[:25]: print('  %10d %8d %-14s %-40s %s %s' % r)
if counts:
    print('gcno names: %d functions of %d objects; untrained (all counts zero) %d, ident not in the .gcda %d' % (len(counts),
          len(set(o for o, _ in counts)), sum(1 for v in counts.values() if v == 0), sum(1 for v in counts.values() if v is None)))
    noprof.sort(reverse=True)
    print('LOST: ran in the windows without a profile: %d functions, %d instructions (%.3f%%), %d I-misses' % (len(noprof),
          sum(r[0] for r in noprof), 100.0 * sum(r[0] for r in noprof) / max(total, 1), sum(r[1] for r in noprof)))
    for k, v in sorted(collections.Counter(r[2] for r in noprof).items()): print('  %-16s %d' % (k, v))
    for r in noprof[:25]: print('  %10d %8d %-14s %-40s %s' % r)
    with open(os.path.join(W, 'lost_noprof.txt'), 'w') as fo:
        fo.write('# instructions imiss object function why (ran in the windows, compiled without its profile)\n')
        for r in noprof: fo.write('%d %d %s %s %s\n' % r)
else: print('no gcno files in %s: the untrained functions are not named (pgochk.sh writes them)' % G)
print('"source locations changed" (profile used): ' + (', '.join('%s %d' % (o, len(v)) for o, v in sorted(moved.items()) if v) or 'none'))
