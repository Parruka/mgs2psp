#!/usr/bin/env python3
"""skc_cmp.py <gsref out, all vblanks> <gsref out, GSREF_SKIP=1> [first vblank to compare]: the shown-frames check (tools/gsref.c
GSREF_SKIPCHK, renderer_notes 11.12, task #47).  Compares the "skc vb" lines of the two runs on the shown vblanks (kind bit 0: the
vblank's packets came from a full call): VU1 calls, primitives and their hash (the state and vertices drawn), the textured primitives
and the hash of what they can read, apart the render-target reads.  With "skc call" lines (GSREF_SKIPCHK=2) it names the first call
that differs in each differing vblank.  Prints the kinds, the transfers that started inside a call, and the records left out."""
import re, sys
V = re.compile(r'skc vb (\d+) k (-?\d+) calls (\d+) prims (\d+) hp (\w+) tex (\d+) ht (\w+) rt (\d+) hr (\w+) mip (\d+) xin (\d+) skipped (\d+)')
C = re.compile(r'skc call vb (\d+) (\d+) e(\w+) prims (\d+) hp (\w+) ht (\w+)')
def load(fn):
    v, c = {}, {}
    for ln in open(fn, errors='replace'):
        m = V.match(ln)
        if m: g = m.groups(); v[int(g[0])] = dict(k=int(g[1]), calls=int(g[2]), prims=int(g[3]), hp=g[4], tex=int(g[5]), ht=g[6], rt=int(g[7]),
                                                hr=g[8], mip=int(g[9]), xin=int(g[10]), skipped=int(g[11])); continue
        m = C.match(ln)
        if m: g = m.groups(); c.setdefault(int(g[0]), []).append((int(g[1]), g[2], int(g[3]), g[4], g[5]))
    return v, c
a, ac = load(sys.argv[1]); b, bc = load(sys.argv[2]); first = int(sys.argv[3]) if len(sys.argv) > 3 else -1
vbs = sorted(set(a) & set(b))
kinds = {}
for x in vbs: kinds[a[x]['k']] = kinds.get(a[x]['k'], 0) + 1
print('vblanks in both: %d (%d..%d); by kind (bit 0 full call, bit 1 reduced call): %s' % (len(vbs), vbs[0] if vbs else -1, vbs[-1] if vbs else -1,
      ', '.join('%d: %d' % (k, n) for k, n in sorted(kinds.items()))))
print('transfers started inside a call: all-vblank run %d, skip run %d; records left out by the skip run %d' % (
      sum(a[x]['xin'] for x in vbs), sum(b[x]['xin'] for x in vbs), sum(b[x]['skipped'] for x in vbs)))
F = [('calls', 'calls'), ('prims', 'prims'), ('hp', 'primitive hash'), ('tex', 'textured'), ('ht', 'texture hash'), ('rt', 'render-target reads'), ('hr', 'render-target hash')]
n = 0; eq = {f: 0 for f, _ in F}; alleq = 0; shown = []; diffs = []
for x in vbs:
    if not (a[x]['k'] & 1) or x < first: continue
    n += 1; d = [f for f, _ in F if a[x][f] != b[x][f]]
    for f, _ in F: eq[f] += a[x][f] == b[x][f]
    if not d: alleq += 1
    else: diffs.append((x, d))
print('shown vblanks compared: %d; equal in everything: %d' % (n, alleq))
print('  equal by field: ' + ', '.join('%s %d' % (nm, eq[f]) for f, nm in F))
print('  primitives drawn in them: %d (all-vblank run), textured %d, render-target reads %d, mipmapped %d' % (
      sum(a[x]['prims'] for x in vbs if a[x]['k'] & 1 and x >= first), sum(a[x]['tex'] for x in vbs if a[x]['k'] & 1 and x >= first),
      sum(a[x]['rt'] for x in vbs if a[x]['k'] & 1 and x >= first), sum(a[x]['mip'] for x in vbs if a[x]['k'] & 1 and x >= first)))
for x, d in diffs[:12]:
    s = 'vb %d differs in %s' % (x, ', '.join(d))
    if x in ac and x in bc:
        ca, cb = ac[x], bc[x]
        for i in range(min(len(ca), len(cb))):
            if ca[i] != cb[i]:
                w = [nm for j, nm in ((1, 'entry'), (2, 'prims'), (3, 'primitive hash'), (4, 'texture hash')) if ca[i][j] != cb[i][j]]
                s += '; first differing call %d (entry %s, %d / %d primitives): %s' % (ca[i][0], ca[i][1], ca[i][2], cb[i][2], ', '.join(w)); break
        else:
            if len(ca) != len(cb): s += '; calls %d / %d' % (len(ca), len(cb))
            else: s += '; every call equal (the difference is in DIRECT / path 3 primitives)'
    print('  ' + s)
if len(diffs) > 12: print('  ... %d more' % (len(diffs) - 12))
