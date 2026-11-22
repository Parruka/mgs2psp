#!/usr/bin/env python3
"""padfit.py <base.map> <new.map> [PAD_TEXT PAD_RODATA PAD_DATA PAD_BSS]: a build that links new units after a base build's objects
(psp/v26me.sh, as psp/v25f.sh did by hand) must keep the base's code at its addresses and its read-only data and data at their offsets
modulo 8 KB (the PSP's caches index 8 KB per way).  Compares the two link maps (ld -Map) input section by input section, in the base's
address order, and prints the pad sizes for the next link: the first input section of .rodata .. .bss whose move is not a multiple of
8 KB gets the pad before it (pad.o's .text.zz_pad, .rodata.zz_pad, .data.zz_pad, .bss.zz_pad, whichever lies last below it in the new
link) grown by what is missing.  Prints "PADS <t> <r> <d> <b>" and "FIT" when nothing is left to move, or "MOVE ..." when it changed a
pad; code sections of the base's own objects (not libraries) must not move at all ("TEXT MOVED" lines, an error).  The import tables
between .text and .rodata (.sceStub.text, .lib.stub, .rodata.sceNid, ...) grow with a new import and are only reported."""
import re, sys, collections
CHECK = ('.rodata', '.eh_frame', '.ctors', '.dtors', '.data', '.sdata', '.sbss', '.bss')
PADS = ('.text.zz_pad', '.rodata.zz_pad', '.data.zz_pad', '.bss.zz_pad')

def norm(f):
    f = f.strip()
    if re.search(r'/tmp/cc\w+\.ltrans\d+\.ltrans\.o', f): return 'ltrans'
    m = re.search(r'([^/]+\.a)\(([^)]+)\)$', f)
    if m: return m.group(1) + ':' + m.group(2)
    return f.rsplit('/', 1)[-1]

def parse(path):
    """[(output section, input section, file, occurrence, address, size)] for every input section with an address"""
    out = []; occ = collections.Counter(); cur = None; pend = None
    for ln in open(path, errors='replace'):
        ln = ln.rstrip('\n')
        m = re.match(r'^(\.[^\s]+)(\s+0x[0-9a-f]+\s+0x[0-9a-f]+)?\s*$', ln)
        if m: cur = m.group(1); pend = None; continue
        if cur is None: continue
        m = re.match(r'^ (\.[^\s]+|COMMON)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$', ln)
        if m:
            name, a, s, f = m.group(1), int(m.group(2), 16), int(m.group(3), 16), norm(m.group(4))
        else:
            m = re.match(r'^ (\.[^\s]+|COMMON)\s*$', ln)
            if m: pend = m.group(1); continue
            m = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$', ln)
            if not (m and pend): pend = None; continue
            name, a, s, f = pend, int(m.group(1), 16), int(m.group(2), 16), norm(m.group(3)); pend = None
        k = (cur, name, f); occ[k] += 1
        out.append((cur, name, f, occ[k], a, s))
    return out

def is_lib(f): return ':' in f or f == 'ltrans' or f.startswith('crt')

base, new = parse(sys.argv[1]), parse(sys.argv[2])
pads = [int(x, 0) for x in sys.argv[3:7]] if len(sys.argv) >= 7 else [0, 0, 0, 0]
N = {(o, n, f, i): a for o, n, f, i, a, s in new}
padpos = sorted((a, PADS.index(n)) for o, n, f, i, a, s in new if n in PADS and f == 'pad.o')
rep = collections.defaultdict(collections.Counter); err = 0; fix = None
for o, n, f, i, a, s in sorted(base, key=lambda t: t[4]):
    k = (o, n, f, i)
    if k not in N or not s or not a: continue                        # not loaded (debug, comments, attributes)
    d = N[k] - a; grp = 'lib' if is_lib(f) else 'obj'
    rep[(o, grp)][d] += 1
    if o in ('.text', '.init', '.fini') and grp == 'obj' and d:
        if err < 10: print('TEXT MOVED %s %s %s %+#x' % (o, n, f, d))
        err += 1
    if o in CHECK and d % 8192 and fix is None:
        below = [p for pa, p in padpos if pa <= N[k]]
        fix = (below[-1] if below else None, (-d) % 8192, o, n, f, d)
for (o, grp), c in sorted(rep.items(), key=lambda t: min(a for oo, n, f, i, a, s in base if oo == t[0][0])):
    print('%-22s %s %5d inputs: %s' % (o, grp, sum(c.values()), ', '.join('%+#x (mod 8K %#x) x%d' % (d, d % 8192, m) for d, m in c.most_common(4))))
if fix:
    p, add, o, n, f, d = fix
    if p is None: print('NO PAD below %s %s %s (moved %+#x)' % (o, n, f, d)); sys.exit(2)
    pads[p] += add
    print('MOVE %s %s %s moved %+#x: %s += %#x' % (o, n, f, d, PADS[p], add))
else:
    print('FIT')
print('PADS %#x %#x %#x %#x' % tuple(pads))
sys.exit(1 if err else 0)
