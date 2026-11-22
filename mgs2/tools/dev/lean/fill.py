#!/usr/bin/env python3
"""fill.py (scratch): every instruction slot of the touched 64-byte code lines, with the line's I-misses spread evenly over its 16 slots;
derived from call_parts.py: what the PSP code of the generated functions' calls, entries, exits and returns is made of (a PspProf dump, by PSP
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


# guest class of a body line (as prof_guest.py gclass, coarse)
def gkind(key):
    path, ln = key
    t = line_text(path, ln)
    m = GA.match(t)
    if not m: return '-'
    w = W(int(m.group(1), 16))
    if w is None: return '?'
    op = w >> 26
    if op in (32, 33, 35, 36, 37, 39): return 'load 8-32'
    if op in (55, 30): return 'load 64/128'
    if op in (40, 41, 43): return 'store 8-32'
    if op in (63, 31): return 'store 64/128'
    if op in (34, 38, 26, 27, 42, 46, 44, 45): return 'ld/st unaligned'
    if op in (49, 57): return 'fpu ld/st'
    if op in (54, 62): return 'vu0 ld/st'
    if op == 18: return 'vu0 op/move'
    if op == 17: return 'fpu op'
    if op in (4, 5, 6, 7, 20, 21, 22, 23, 1): return 'branch'
    if op == 28: return 'mmi'
    if op == 0 and (w & 63) in (8, 9): return 'jr/jalr'
    if op in (2, 3): return 'j/jal'
    return 'alu'
# line classes for every slot of touched lines
touched = collections.defaultdict(lambda: [0, 0])     # line address -> instr, imiss
for pc, c, im, dm in rows:
    v = touched[pc >> 6]; v[0] += c; v[1] += im
gen_syms = []
for S, E in ((S_m, E_m), (S_o, E_o)):
    for a, sz, (n, k) in S:
        if (n.startswith('S_') and '_P' in n) or re.match(r'^P[0-9a-f]+(\.|$)', n): gen_syms.append((a, sz, n, E))
gen_ranges = []
for a, sz, n, E in gen_syms: gen_ranges.append((a, sz, n, E))
gen_ranges.sort()
gst = [a for a, sz, n, E in gen_ranges]
def gen_of(pc):
    i = bisect.bisect_right(gst, pc) - 1
    if i >= 0 and pc < gen_ranges[i][0] + gen_ranges[i][1]: return gen_ranges[i]
    return None
# all slots of touched lines that lie in generated functions
slots = []
for ln_ in touched:
    for k in range(16):
        pc = (ln_ << 6) + 4 * k
        if gen_of(pc): slots.append(pc)
slots.sort()
# line info for every slot (not only executed ones)
sm = [pc for pc in slots if not (ob <= pc < ob + osz)]; so = [pc for pc in slots if ob <= pc < ob + osz]
gm2 = {pc: mv_m(pc) for pc in sm}; go2 = {pc: mv_o(pc) for pc in so}
l_m2 = a2l(elf_g, sorted({g for g in gm2.values() if g is not None}))
l_o2 = a2l(ovl_g, sorted({g for g in go2.values() if g is not None}))
for pc, g in gm2.items(): loc[pc] = l_m2.get(g, ('??', '??:0')) if g is not None else ('(no symbol)', '??:0')
for pc, g in go2.items(): loc[pc] = l_o2.get(g, ('??', '??:0')) if g is not None else ('(no symbol)', '??:0')
# decode each generated function once (base tracking), keep kinds for the slots
kind_of = {}
done = set()
for pc in slots:
    f = gen_of(pc)
    if f[0] in done: continue
    done.add(f[0])
    a, sz, n, E = f
    val = {}
    for q in range(a, a + sz, 4):
        w = E.word(q)
        if w is None: break
        kind, det = decode(w, val)
        kind_of[q] = (kind, det)
fill = collections.defaultdict(lambda: [0.0, 0])    # (line class, guest kind, psp kind) -> misses, slots
for pc in slots:
    im = touched[pc >> 6][1]
    cls, key = line_class(pc)
    if cls is None: cls, gk = 'not a header line', '-'
    else: gk = gkind(key) if cls in ('body', 'call', 'jr') else '-'
    kind, det = kind_of.get(pc, ('?', None))
    if kind.startswith('st R') or kind.startswith('ld R'):
        kind = kind + (' hi' if det and det[2] else ' lo')
    v = fill[(cls, gk, kind)]; v[0] += im / 16.0; v[1] += 1
T = sum(v[0] for v in fill.values())
print('generated code: %d slots in touched lines (%.0f KB), I-misses %.0f per step' % (len(slots), len(slots) * 4 / 1024, T / steps), file=out)
agg1 = collections.defaultdict(float); agg2 = collections.defaultdict(float); agg3 = collections.defaultdict(float)
for (cls, gk, kind), v in fill.items():
    agg1[cls] += v[0]; agg2[(cls, gk)] += v[0]; agg3[(cls, kind)] += v[0]
print('\nby line class (share of the generated code\'s I-misses)', file=out)
for k, v in sorted(agg1.items(), key=lambda kv: -kv[1]): print('  %-20s %5.1f%%' % (k, 100 * v / T), file=out)
print('\nbody lines by guest kind', file=out)
for k, v in sorted(agg2.items(), key=lambda kv: -kv[1]):
    if k[0] == 'body': print('  %-20s %5.1f%%' % (k[1], 100 * v / T), file=out)
print('\nby line class and PSP kind (>= 0.3%)', file=out)
for k, v in sorted(agg3.items(), key=lambda kv: -kv[1]):
    if v / T >= 0.003: print('  %-8s %-26s %5.1f%%' % (k[0], k[1], 100 * v / T), file=out)
print('\nbody memory lines: PSP kinds (share of all generated I-misses)', file=out)
for gk in ('load 8-32', 'store 8-32', 'load 64/128', 'store 64/128', 'fpu ld/st', 'vu0 ld/st'):
    sub = {kind: v[0] for (cls, g, kind), v in fill.items() if cls == 'body' and g == gk}
    tt = sum(sub.values())
    print('  %-14s %5.1f%%: ' % (gk, 100 * tt / T) + ', '.join('%s %.1f' % (k, 100 * x / T) for k, x in sorted(sub.items(), key=lambda kv: -kv[1])[:9]), file=out)
