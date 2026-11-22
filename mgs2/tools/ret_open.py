#!/usr/bin/env python3
"""ret_open.py <out dir> <SLES elf> <stage dir> <gen_elf.h | gen_s_<stage>.h> ... (recomp/lean_plan.md 1c; mk_headers.sh with RC_RETLIVE,
over every unit's survey.py output): per unit, the translated functions whose callers are not all direct calls in the same unit ('open':
regcache.py RC_RETLIVE keeps every store at their returns).  Open: the ELF's entry; any function whose address is a 32-bit word of an
image (the ELF's loaded segment, every stage image at 0x1f1c00) or is built by lui + addiu / ori (within 32 instructions) anywhere in
them; any hex literal of the generated text outside the dispatch tables, FN_ENTER, the /*pc*/ marks and return addresses; an ELF
function that stage code calls by name; a stage function below the stage base (stage code at ELF addresses).  The unpruned text gives a
superset of what the pruned headers hold, so the lists hold for every coverage list.  Writes <out>/open_<unit>.txt (one name per line)
and prints the counts.  <stage dir>/<stage>/nocache/<stage>.bin are the stage images (mk_headers.sh STG)."""
import re, sys, struct, os
out, elf, sdir = sys.argv[1:4]
BASE = 0x1f1c00
FNDEF = re.compile(r'^static void ((?:S_(\w+?)_)?P([0-9a-f]{5,6}))\(void\)$', re.M)
units = {}   # unit -> {addr: name}
hdr = {}     # unit -> header path
for h in sys.argv[4:]:
    u = 'elf' if os.path.basename(h) == 'gen_elf.h' else re.search(r'gen_s_(\w+)\.h$', h).group(1)
    hdr[u] = h
    units[u] = {int(m.group(3), 16): m.group(1) for m in FNDEF.finditer(open(h).read())}
if 'elf' not in units: sys.exit('ret_open: the ELF header (gen_elf.h) is needed')
d = open(elf, 'rb').read(); imgs = []
phoff, = struct.unpack_from('<I', d, 0x1c); n, = struct.unpack_from('<H', d, 0x2c); es, = struct.unpack_from('<H', d, 0x2a); entry, = struct.unpack_from('<I', d, 0x18)
for i in range(n):
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from('<8I', d, phoff + i * es)
    if t == 1: imgs.append(('elf', va, d[off:off + fs]))
for u in units:
    if u != 'elf':
        imgs.append((u, BASE, open(os.path.join(sdir, u, 'nocache', u + '.bin'), 'rb').read()))
allstarts = set().union(*[set(m) for m in units.values()])
found = set()
for u, base, b in imgs:
    nw = len(b) // 4; w = struct.unpack_from('<%dI' % nw, b)
    found |= {v for v in w if v in allstarts}
    for i, v in enumerate(w):
        if v >> 26 == 0x0f:
            rt = (v >> 16) & 31; hi = (v & 0xffff) << 16
            for j in range(i + 1, min(nw, i + 33)):
                x = w[j]; op = x >> 26
                if op in (0x09, 0x0d) and (x >> 21) & 31 == rt:
                    lo = x & 0xffff
                    a = (hi + (lo - 0x10000 if (op == 0x09 and lo & 0x8000) else lo)) & 0xffffffff
                    if a in allstarts: found.add(a)
lit = set()
SKIP = re.compile(r'^\s*(case 0x|\{0x)|FN_ENTER\(0x')
MISS = re.compile(r'\brt_missing\(0x[0-9a-f]+\)')
for h in hdr.values():
    for ln in open(h):
        if SKIP.search(ln): continue
        s = MISS.sub('', re.sub(r'/\*.*?\*/', '', ln))   # a cut function's own stub (prune_cov.py)
        s = re.sub(r'\b[rR]\[?31\]? = 0x[0-9a-f]+', '', s)
        for m in re.finditer(r'\b0x([0-9a-f]{5,6})\b', s):
            a = int(m.group(1), 16)
            if a in allstarts: lit.add(a)
xcalls = set()
for u in units:
    if u != 'elf': xcalls |= set(re.findall(r'\b(P[0-9a-f]{5,6})\(\)', open(hdr[u]).read()))
os.makedirs(out, exist_ok=True)
for u, fs in sorted(units.items()):
    op = set()
    for a, name in fs.items():
        if a in found or a in lit or (u == 'elf' and (name in xcalls or a == entry)) or (u != 'elf' and a < BASE): op.add(name)
    open(os.path.join(out, 'open_%s.txt' % u), 'w').write(''.join(x + '\n' for x in sorted(op)))
    print('%-8s functions %5d, open %5d' % (u, len(fs), len(op)))
print('address words / lui pairs: %d, literals: %d, ELF functions called from stages: %d, entry %08x' % (len(found), len(lit), len(xcalls), entry))
