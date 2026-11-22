#!/usr/bin/env python3
"""taken.py <out.json> (dev, recomp/lean_plan.md step 2): ret_open.py's open lists split by reason, for wpo2.py / wpc.py.  taken: a
function whose address an image word, a lui + addiu / ori pair, a hex literal of the text, the ELF entry or stage code below the stage
base gives (a call through a register or the runtime may enter it); xcalls: ELF functions that stage code calls by name (direct calls,
known callers once their stages are read).  Inputs as mk_headers.sh: the post-narrow texts (PN), SLES_503.83 (SL), the stage images (STG)."""
import re, sys, struct, os, json
PN = os.environ.get("PN", "/home/claude/lean/w2/pn"); SL = os.environ.get("SL", "/home/claude/mgs2/SLES_503.83"); STG = os.environ.get("STG", "/home/claude/mgs2/stage"); BASE = 0x1f1c00
FNDEF = re.compile(r'^static void ((?:S_(\w+?)_)?P([0-9a-f]{5,6}))\(void\)$', re.M)
UN = ['elf', 'init', 'r_title', 'r_tnk0', 'n_title', 'd00t', 'd01t', 'w00a', 'w01a']
def hn(u): return os.path.join(PN, 'gen_elf.h' if u == 'elf' else 'gen_s_%s.h' % u)
units = {u: {int(m.group(3), 16): m.group(1) for m in FNDEF.finditer(open(hn(u)).read())} for u in UN}
d = open(SL, 'rb').read(); imgs = []
phoff, = struct.unpack_from('<I', d, 0x1c); n, = struct.unpack_from('<H', d, 0x2c); es, = struct.unpack_from('<H', d, 0x2a); entry, = struct.unpack_from('<I', d, 0x18)
for i in range(n):
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from('<8I', d, phoff + i * es)
    if t == 1: imgs.append(('elf', va, d[off:off + fs]))
for u in UN:
    if u != 'elf': imgs.append((u, BASE, open(os.path.join(STG, u, 'nocache', u + '.bin'), 'rb').read()))
allstarts = set().union(*[set(m) for m in units.values()])
word = {}; lui = {}
for u, base, b in imgs:
    nw = len(b) // 4; w = struct.unpack_from('<%dI' % nw, b)
    for i, v in enumerate(w):
        if v in allstarts: word.setdefault(v, set()).add(u)
        if v >> 26 == 0x0f:
            rt = (v >> 16) & 31; hi = (v & 0xffff) << 16
            for j in range(i + 1, min(nw, i + 33)):
                x = w[j]; op = x >> 26
                if op in (0x09, 0x0d) and (x >> 21) & 31 == rt:
                    lo = x & 0xffff
                    a = (hi + (lo - 0x10000 if (op == 0x09 and lo & 0x8000) else lo)) & 0xffffffff
                    if a in allstarts: lui.setdefault(a, set()).add(u)
lit = {}
SKIP = re.compile(r'^\s*(case 0x|\{0x)|FN_ENTER\(0x')
MISS = re.compile(r'\brt_missing\(0x[0-9a-f]+\)')
for u in UN:
    for ln in open(hn(u)):
        if SKIP.search(ln): continue
        s = MISS.sub('', re.sub(r'/\*.*?\*/', '', ln))
        s = re.sub(r'\b[rR]\[?31\]? = 0x[0-9a-f]+', '', s)
        for m in re.finditer(r'\b0x([0-9a-f]{5,6})\b', s):
            a = int(m.group(1), 16)
            if a in allstarts: lit.setdefault(a, set()).add(u)
xc = {}
for u in UN:
    if u != 'elf':
        for nm in set(re.findall(r'\b(P[0-9a-f]{5,6})\(\)', open(hn(u)).read())): xc.setdefault(nm, set()).add(u)
res = {}
for u in UN:
    tk = []; xo = []
    for a, name in units[u].items():
        t = a in word or a in lui or a in lit or (u == 'elf' and a == entry) or (u != 'elf' and a < BASE)
        x = u == 'elf' and name in xc
        if t: tk.append(name)
        elif x: xo.append(name)
    res[u] = sorted(tk)
    print('%-8s taken %5d, called by name from stages only %5d' % (u, len(tk), len(xo)))
json.dump({'taken': res, 'xcalls': {k: sorted(v) for k, v in xc.items()}}, open(sys.argv[1], 'w'))
