#!/usr/bin/env python3
"""parts2.py (scratch): callees by size / leaf / width, address checks, VU0 groups; derived from call_parts.py: what the PSP code of the generated functions' calls, entries, exits and returns is made of (a PspProf dump, by PSP
instruction kind), to size a leaner call protocol.

  call_parts.py <dump> <steps> <elf> <ovl> <elf_g> <ovl_g> <ps2 elf> <stage.bin> [out.txt]

Same inputs as tools/dev/prof_guest.py (scratch copy in this folder).  Each executed PSP instruction of a generated header line is
decoded from the profiled ELF; loads and stores whose base register holds a constant address of the guest machine state (R[] at
0x10400, RH 0x10500, F 0x10600, HI/LO 0x10680, VF 0x10000) are named by register.  The base register's value is tracked by a linear
scan of each generated function (lui / addiu / ori / move; a jal or jalr clears the caller-saved host registers)."""
import sys, os, re, struct, subprocess, collections, bisect

dump, steps, elf_p, ovl_p, elf_g, ovl_g, ps2elf, stagebin = sys.argv[1:9]
steps = int(steps)
out = open(sys.argv[9], 'w') if len(sys.argv) > 9 else sys.stdout
BIN = '/home/claude/pspsdk/pspdev/bin/'

# --- guest code (as prof_guest.py) ---
D = open(ps2elf, 'rb').read()
SD = open(stagebin, 'rb').read()
SBASE = struct.unpack_from('<I', SD, 0)[0]
def W(a):
    if SBASE <= a < SBASE + len(SD): return struct.unpack_from('<I', SD, a - SBASE)[0]
    o = a - 0x100000 + 0x80
    return struct.unpack_from('<I', D, o)[0] if 0 <= o < len(D) - 3 else None
def gmnem(w):
    op = w >> 26
    if op == 0:
        f = w & 63
        return {8: 'jr', 9: 'jalr'}.get(f, 'x')
    return {2: 'j', 3: 'jal'}.get(op, 'x')

# --- PSP ELF bytes ---
class Elf:
    def __init__(self, p):
        self.d = open(p, 'rb').read()
        d = self.d
        shoff, = struct.unpack_from('<I', d, 0x20)
        shentsize, shnum = struct.unpack_from('<HH', d, 0x2e)
        self.secs = []
        for i in range(shnum):
            nm, typ, flags, addr, off, size = struct.unpack_from('<IIIIII', d, shoff + i * shentsize)
            if flags & 4 and typ == 1 and size: self.secs.append((addr, size, off))
    def word(self, a):
        for addr, size, off in self.secs:
            if addr <= a < addr + size: return struct.unpack_from('<I', self.d, off + a - addr)[0]
        return None

def syms(elf):
    o = subprocess.run([BIN + 'psp-nm', '-S', '--defined-only', '-n', elf], capture_output=True, text=True).stdout.splitlines()
    seen = collections.Counter(); res = []
    for l in o:
        p = l.split()
        if len(p) == 4 and p[2] in ('t', 'T'):
            a, sz, n = int(p[0], 16), int(p[1], 16), p[3]
            res.append((a, sz, (n, seen[n]))); seen[n] += 1
    return res
def mover(sp, ge):
    sg = {k: a for a, sz, k in syms(ge)}
    st = [a for a, sz, k in sp]
    def mv(pc):
        i = bisect.bisect_right(st, pc) - 1
        if i < 0: return None
        a, sz, k = sp[i]
        if pc >= a + sz or k not in sg: return None
        return sg[k] + (pc - a)
    return mv

def sect(elf, name):
    for l in subprocess.run([BIN + 'psp-objdump', '-h', elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) > 4 and p[1] == name: return int(p[3], 16), int(p[2], 16)
    return None

def a2l(elf, pcs):
    res = {}
    if not pcs: return res
    inp = '\n'.join('%x' % p for p in pcs) + '\n'
    o = subprocess.run([BIN + 'psp-addr2line', '-e', elf, '-a', '-f', '-i'], input=inp, capture_output=True, text=True).stdout.splitlines()
    cur = None; frames = []
    def flush():
        if cur is not None: res[cur] = frames[-1] if frames else ('??', '??:0')
    i = 0
    while i < len(o):
        l = o[i]
        if l.startswith('0x'):
            flush(); cur = int(l, 16); frames = []; i += 1; continue
        frames.append((l, o[i + 1] if i + 1 < len(o) else '??:0')); i += 2
    flush()
    return res

rows = []
tot = None
for l in open(dump):
    if l.startswith('#'):
        tot = l; continue
    p = l.split()
    if len(p) >= 4: rows.append((int(p[0], 16), int(p[1]), int(p[2]), int(p[3])))
cnt = {r[0]: r for r in rows}

ob, osz = sect(ovl_p, '.ovl')
E_m, E_o = Elf(elf_p), Elf(ovl_p)
S_m, S_o = syms(elf_p), syms(ovl_p)
mv_m = mover(S_m, elf_g); mv_o = mover(S_o, ovl_g)
pcs_m = sorted({r[0] for r in rows if not (ob <= r[0] < ob + osz)})
pcs_o = sorted({r[0] for r in rows if ob <= r[0] < ob + osz})
gm = {pc: mv_m(pc) for pc in pcs_m}; go = {pc: mv_o(pc) for pc in pcs_o}
l_m = a2l(elf_g, sorted({g for g in gm.values() if g is not None}))
l_o = a2l(ovl_g, sorted({g for g in go.values() if g is not None}))
loc = {}
for pc, g in gm.items(): loc[pc] = l_m.get(g, ('??', '??:0')) if g is not None else ('(no symbol)', '??:0')
for pc, g in go.items(): loc[pc] = l_o.get(g, ('??', '??:0')) if g is not None else ('(no symbol)', '??:0')

cache = {}
def line_text(path, n):
    if path not in cache:
        try: cache[path] = open(path, errors='replace').read().split('\n')
        except OSError: cache[path] = []
    t = cache[path]
    return t[n - 1] if 0 < n <= len(t) else ''
GA = re.compile(r'^\s*/\*([0-9a-f]+)\*/')

def line_class(pc):
    fn, lc = loc.get(pc, ('??', '??:0'))
    path, _, ln = lc.rpartition(':')
    ln = ln.split()[0] if ln else '0'
    try: ln = int(ln)
    except ValueError: ln = 0
    base = os.path.basename(path)
    if not (base.startswith('gen_') and base.endswith('.h')): return None, None
    t = line_text(path, ln)
    m = GA.match(t)
    if m:
        w = W(int(m.group(1), 16))
        g = gmnem(w) if w is not None else 'x'
        if g in ('jal', 'jalr'): return 'call', (path, ln)
        if g == 'jr': return 'jr', (path, ln)
        return 'body', (path, ln)
    if t.startswith('FN_ENTER') or t.startswith('  float') or t.startswith('  int64_t') or t.startswith('  int32_t') or t.startswith('static void') or t.startswith('{'):
        return 'entry', (path, ln)
    if t.strip() == '}': return 'exit', (path, ln)
    return 'other', (path, ln)

# --- decode with base-register tracking, one generated function at a time ---
RN = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7', 's0', 's1', 's2', 's3',
      's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']
def gcls(n):
    if n in (2, 3): return 'v0-v1'
    if 4 <= n <= 7: return 'a0-a3'
    if 8 <= n <= 15 or n in (24, 25): return 't0-t9'
    if 16 <= n <= 23: return 's0-s7'
    if n == 30: return 'fp'
    if n == 31: return 'ra'
    if n == 29: return 'sp'
    if n == 28: return 'gp'
    if n == 1: return 'at'
    return 'k0/k1/zero'
CALLER = set(range(1, 16)) | {24, 25, 31}
def state_name(a, size):
    if 0x10400 <= a < 0x10500:
        n, hi = (a - 0x10400) // 8, (a - 0x10400) % 8 >= 4
        return 'R', n, hi
    if 0x10500 <= a < 0x10600: return 'RH', (a - 0x10500) // 8, False
    if 0x10600 <= a < 0x10680: return 'F', (a - 0x10600) // 4, False
    if 0x10680 <= a < 0x106a0: return 'HI/LO', 0, False
    if 0x106a0 <= a < 0x106b0: return 'ACC/CC/k_left', 0, False
    if 0x10000 <= a < 0x10400: return 'VF/VI', 0, False
    if 0x10000 <= a < 0x14000: return 'scratchpad other', 0, False
    return None

def decode(w, val):
    """-> (kind, detail), updating val (register -> known constant)"""
    op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; rd = (w >> 11) & 31; sa = (w >> 6) & 31; fn = w & 63
    imm = w & 0xffff; simm = imm - 0x10000 if imm & 0x8000 else imm
    def setr(r, v):
        if r: val[r] = v
    if w == 0: return 'nop', None
    if op == 0:
        if fn == 9:                                     # jalr
            for r in CALLER: val.pop(r, None)
            return 'host call (jalr)', None
        if fn == 8: return 'jr', None
        if fn == 3 and sa == 31:
            val.pop(rd, None); return 'sra 31 (high word)', None
        if fn in (0x25, 0x21) and rt == 0:              # move rd, rs
            if rs == 0: val[rd] = 0; return 'move zero', None
            if rs in val: val[rd] = val[rs]
            else: val.pop(rd, None)
            return 'move', None
        val.pop(rd, None)
        return 'alu', None
    if op == 3:
        for r in CALLER: val.pop(r, None)
        return 'host call (jal)', None
    if op == 2: return 'j', None
    if op == 15:                                         # lui
        setr(rt, (imm << 16) & 0xffffffff); return 'lui', None
    if op == 9:                                          # addiu
        if rs in val: setr(rt, (val[rs] + simm) & 0xffffffff)
        elif rs == 0: setr(rt, simm & 0xffffffff)
        else: val.pop(rt, None)
        return 'addiu', None
    if op == 13:                                         # ori
        if rs in val: setr(rt, val[rs] | imm)
        elif rs == 0: setr(rt, imm)
        else: val.pop(rt, None)
        return 'ori', None
    if op in (4, 5, 6, 7, 20, 21, 22, 23, 1) or (op == 17 and rs == 8) or (op == 18 and rs == 8): return 'branch', None
    loads = {32: 1, 33: 2, 35: 4, 36: 1, 37: 2, 34: 4, 38: 4, 49: 4, 50: 4, 54: 16}
    stores = {40: 1, 41: 2, 43: 4, 42: 4, 46: 4, 57: 4, 58: 4, 62: 16}
    if op in loads or op in stores:
        st = op in stores
        base = rs
        if op in (50, 54, 58, 62): simm &= ~3          # lv.s / lv.q / sv.s / sv.q: the low bits are register bits
        if base == 29: k = 'host stack'; det = None
        elif base in val:
            sn = state_name((val[base] + simm) & 0xffffffff, 4)
            if sn: k = sn[0]; det = sn
            else: k = 'other mem'; det = None
        else: k = 'other mem'; det = None
        if not st and op in (32, 33, 35, 36, 37, 34, 38): val.pop(rt, None)
        return ('st ' if st else 'ld ') + k, det
    if op == 31 and fn == 0x20 and ((w >> 6) & 31) == 0x10:   # seb/seh (special3 bshfl)
        val.pop(rd, None); return 'alu', None
    if op == 31:
        val.pop(rt, None); return 'alu', None
    if op in (8, 10, 11, 12, 14):
        val.pop(rt, None); return 'alu', None
    return 'other', None


gen_syms = []
for S, E in ((S_m, E_m), (S_o, E_o)):
    for a, sz, (n, k) in S:
        if (n.startswith('S_') and '_P' in n) or re.match(r'^P[0-9a-f]+(\.|$)', n): gen_syms.append((a, sz, n, E))
# width of each generated function from the headers (the declaration line after FN_ENTER)
hdrs = ['/home/claude/v26/WP/gen_s_w00a.h', '/home/claude/v26/WP/gen_elf.h']
width = {}
FNRE = re.compile(r'^static void (\w+)\(void\)$')
for h in hdrs:
    try: L = open(h, errors='replace').read().split('\n')
    except OSError: continue
    for i, l in enumerate(L):
        m = FNRE.match(l)
        if not m: continue
        for j in range(i + 1, min(i + 5, len(L))):
            t = L[j]
            if t.startswith('  int64_t r0') : width[m.group(1)] = 'wide'; break
            if t.startswith('  int64_t ') and 'int32_t r0' in t: width[m.group(1)] = 'mixed'; break
            if t.startswith('  int32_t r0'): width[m.group(1)] = 'narrow'; break
        else: width.setdefault(m.group(1), 'no regs')
symaddr = {}
for S in (S_m, S_o):
    for a, sz, (n, k) in S: symaddr[a] = n
fstat = []   # name, entries, size, leaf, width, instr executed, imiss
tot_body = collections.Counter(); chk = collections.Counter()
for a, sz, n, E in gen_syms:
    pcs = [pc for pc in range(a, a + sz, 4) if pc in cnt]
    if not pcs: continue
    ent = cnt[a][1] if a in cnt else 0
    leaf = True; ins = 0; im = 0
    for pc in range(a, a + sz, 4):
        w = E.word(pc)
        if w is None: break
        if w >> 26 == 3:
            t = ((w & 0x3ffffff) << 2) | (pc & 0xf0000000)
            tn = symaddr.get(t, '?')
            if tn.startswith(('P', 'S_', 'gen_call', 'jalr_do', 'guest_call')) and not tn.startswith('S_w00a_call'): leaf = False
        if w >> 26 == 0 and (w & 63) == 9: leaf = False
        if pc in cnt:
            ins += cnt[pc][1]; im += cnt[pc][2]
            # GEN_FAST check: srl rd, rt, 28 (or ext rd, rt, 28, 4); the RAM mask: ext rd, rs, 0, 25
            if w >> 26 == 0 and (w & 63) == 2 and ((w >> 6) & 31) == 28: chk['srl 28 (I/O check)'] += cnt[pc][1]
            if w >> 26 == 31 and (w & 63) == 0:
                pos_, msb = (w >> 6) & 31, (w >> 11) & 31
                if pos_ == 0 and msb == 24: chk['ext 0,25 (RAM mask)'] += cnt[pc][1]
                if pos_ == 28 and msb == 3: chk['ext 28,4 (I/O check)'] += cnt[pc][1]
    base = n.split('.')[0]
    fstat.append((n, ent, sz // 4, leaf, width.get(base, '?'), ins, im))

# PSP instructions per guest instruction by guest class, wide vs narrow functions (line = one guest instruction + its delay slot if a branch)
def gk2(t):
    m = GA.match(t)
    if not m: return None
    w = W(int(m.group(1), 16))
    if w is None: return None
    op = w >> 26
    if op in (32, 33, 35, 36, 37, 39): return 'load 8-32'
    if op in (55, 30): return 'load 64/128'
    if op in (40, 41, 43): return 'store 8-32'
    if op in (63, 31): return 'store 64/128'
    if op in (49, 57): return 'fpu ld/st'
    if op in (54, 62): return 'vu0 ld/st'
    if op == 18: return 'vu0 op/move'
    if op == 17: return 'fpu op'
    if op in (4, 5, 6, 7, 20, 21, 22, 23, 1): return 'branch'
    if op in (2, 3) or (op == 0 and (w & 63) in (8, 9)): return 'call/jump'
    if op == 0 and (w & 63) in (44, 45, 46, 47, 56, 58, 59, 60, 62, 63, 20, 22, 23): return 'alu 64'
    if op in (24, 25): return 'alu 64'
    return 'alu 32'
fname = {}
for a, sz, n, E in gen_syms: fname[a] = n.split('.')[0]
starts = sorted(fname)
def fn_of(pc):
    i = bisect.bisect_right(starts, pc) - 1
    return fname[starts[i]] if i >= 0 else None
agg = collections.defaultdict(lambda: [0, 0, 0])   # (width, class) -> psp instr, guest instr (busiest pc per line), imiss
linemax = collections.defaultdict(int); linecls = {}
for pc, c, im, dm in rows:
    f = fn_of(pc)
    if f is None: continue
    fn, lc = loc.get(pc, ('??', '??:0'))
    path, _, ln = lc.rpartition(':')
    try: ln = int(ln.split()[0])
    except (ValueError, IndexError): continue
    if not os.path.basename(path).startswith('gen_'): continue
    t = line_text(path, ln)
    k = gk2(t)
    if k is None: continue
    wd = width.get(f, '?')
    v = agg[(wd, k)]; v[0] += c; v[2] += im
    key = (path, ln)
    if c > linemax[key]: linemax[key] = c
    linecls[key] = (wd, k)
for key, n in linemax.items(): agg[linecls[key]][1] += n
print('PSP instructions per guest instruction, by class: wide / narrow functions (guest instr per step in brackets)', file=out)
for k in sorted({k for w, k in agg}):
    a = agg.get(('wide', k), [0, 0, 0]); b = agg.get(('narrow', k), [0, 0, 0])
    print('  %-14s wide %5.2f (%6.0f)   narrow %5.2f (%6.0f)' % (k, a[0] / max(a[1], 1), a[1] / steps, b[0] / max(b[1], 1), b[1] / steps), file=out)
