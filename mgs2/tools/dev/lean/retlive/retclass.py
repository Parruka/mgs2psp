#!/usr/bin/env python3
"""retclass.py <gi counts> <steps> <SLES elf> <stage bin> <stage base hex> <xcalls dir> <gen_elf.h> <gen_s_<stage>.h>: per-function return
store traffic (PSP instructions per step, weighted by the gi counts as callcost.py does), split by register class and by whether the function
is 'open' (its address appears in the images as a word or as a lui + addiu / ori pair, or it is an ELF function called from stage code)."""
import re, sys, struct, os, glob
from collections import defaultdict
MARK = re.compile(r'^/\*([0-9a-f]{6})\*/', re.M); INNER = re.compile(r'/\*[0-9a-f]{6}\*/')
XFER = re.compile(r'\bgoto\b|\breturn\b|\bJALR\(|\bJR\(|\bJSIF\(|\b(?:S_\w+_)?P[0-9a-f]{5,6}\(\)')
FN = re.compile(r'^static void ([A-Za-z_]\w*)\(void\)\n\{\n(.*?)\n\}$', re.S | re.M)
ST = re.compile(r'\bR\[(\d+)\] = r(\d+)\b'); FST = re.compile(r'\bF\[(\d+)\] = f(\d+)\b')
RET = re.compile(r'\{ ([^{}]*?); (?:\{ ([^{}]*?); )?return; \}(?: \})?')
NOREL = {16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30}; FNOREL = set(range(20, 32))
gi = {}
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 3: gi[int(p[0], 16)] = int(p[1]) + int(p[2])
steps = float(sys.argv[2])
# images: (base, bytes)
d = open(sys.argv[3], 'rb').read(); imgs = []
phoff = struct.unpack_from('<I', d, 0x1c)[0]; n = struct.unpack_from('<H', d, 0x2c)[0]; es = struct.unpack_from('<H', d, 0x2a)[0]
for i in range(n):
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from('<8I', d, phoff + i * es)
    if t == 1: imgs.append((va, d[off:off + fs]))
imgs.append((int(sys.argv[5], 16), open(sys.argv[4], 'rb').read()))
xcalls = set()
for f in glob.glob(os.path.join(sys.argv[6], 'xcalls_*.txt')):
    xcalls |= {l.strip()[:-2] for l in open(f) if l.strip()}
starts = {}   # address -> name
srcs = sys.argv[7:]
per = {}
for path in srcs:
    src = open(path).read()
    for m in FN.finditer(src):
        name, body = m.group(1), m.group(2)
        a = re.search(r'P([0-9a-f]{5,6})$', name)
        if a and name.startswith(('P', 'S_')): starts.setdefault(int(a.group(1), 16), name)
        w64 = set()
        dd = re.search(r'^  (?:int64_t [^;]*; )?(?:int64_t|int32_t) [^;]*;', body, re.M)
        if dd:
            for part in dd.group(0).split(';'):
                if part.strip().startswith('int64_t'): w64 |= {int(x) for x in re.findall(r'\br(\d+)\b', part)}
        c = defaultdict(float)
        for ln in body.split('\n'):
            mm = MARK.match(ln)
            if not mm: continue
            pc = int(mm.group(1), 16); k = (2 if XFER.search(ln) else 1) + len(INNER.findall(ln, mm.end()))
            runs = gi.get(pc, 0) / k / steps
            if not runs: continue
            for r in RET.finditer(ln):
                for x in ST.finditer(r.group(0)):
                    n_ = int(x.group(1)); ins = 2 if int(x.group(2)) in w64 else 3
                    cls = 'norel' if n_ in NOREL else 'v01' if n_ in (2, 3) else 'scr'
                    c[cls] += ins * runs
                for x in FST.finditer(r.group(0)):
                    c['fnorel' if int(x.group(1)) in FNOREL else 'fscr'] += runs
        per[name] = c
# address-taken scan
fstarts = set(starts)
taken = set()
for base, b in imgs:
    nw = len(b) // 4; w = struct.unpack_from('<%dI' % nw, b)
    for v in w:
        if v in fstarts: taken.add(v)
    # lui rt, hi ... addiu / ori rx, rt, lo within 32 instructions
    for i, v in enumerate(w):
        if v >> 26 == 0x0f:
            rt = (v >> 16) & 31; hi = (v & 0xffff) << 16
            for j in range(i + 1, min(nw, i + 33)):
                u = w[j]; op = u >> 26
                if op in (0x09, 0x0d) and (u >> 21) & 31 == rt:
                    lo = u & 0xffff
                    a = (hi + (lo - 0x10000 if (op == 0x09 and lo & 0x8000) else lo)) & 0xffffffff
                    if a in fstarts: taken.add(a)
tot = defaultdict(float); byc = defaultdict(lambda: defaultdict(float))
for name, c in per.items():
    a = re.search(r'P([0-9a-f]{5,6})$', name); addr = int(a.group(1), 16) if a else None
    base = name[:-0] if not name.startswith('O') else name
    cls = 'taken' if addr in taken else 'xunit' if name in xcalls else 'closed'
    for k, v in c.items(): tot[k] += v; byc[cls][k] += v
print('functions %d, starts %d, address-taken %d, ELF functions called from stages %d' % (len(per), len(starts), len(taken), len(xcalls)))
keys = ('scr', 'v01', 'norel', 'fscr', 'fnorel')
print('%-8s' % '' + ''.join('%10s' % k for k in keys))
print('%-8s' % 'all' + ''.join('%10.0f' % tot[k] for k in keys))
for cl in ('closed', 'xunit', 'taken'): print('%-8s' % cl + ''.join('%10.0f' % byc[cl][k] for k in keys))
top = sorted(per.items(), key=lambda kv: -(kv[1]['scr'] + kv[1]['v01']))[:25]
for name, c in top:
    a = re.search(r'P([0-9a-f]{5,6})$', name); addr = int(a.group(1), 16) if a else None
    print('%-22s %8.0f scr %8.0f v01 %8.0f norel  %s' % (name, c['scr'], c['v01'], c['norel'], 'taken' if addr in taken else 'xunit' if name in xcalls else 'closed'))
