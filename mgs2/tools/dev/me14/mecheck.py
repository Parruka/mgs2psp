# mecheck.py <dir> [callmap]: task #149, the ME unit's check (mgs2/me_handoff_design.md 4.8; hle_me.h's rules).
# <dir> holds mkme_psp.sh's output: meunit.o (the unit kept from the ME's entries), all.o (every function), entries.txt, stack.su.
# Reads the ELF files itself (no disassembler text) and checks the unit's code:
#  1. relocations: only R_MIPS_26 / HI16 / LO16 in code and R_MIPS_32 in data; nothing undefined (libgcc's pure integer helpers are
#     listed apart, double helpers fail); no code address in a data section (a table of functions would be called at a user address);
#  2. functions whose address is taken somewhere but which the unit dropped (a host may install one for the ME to call: review);
#  3. every jalr and every jr through a register other than ra takes an address made by ME_CALLP: a data-flow pass over each function
#     tags registers (SEG = srl x,gp,31; CALLP = ins x,SEG,31,1; moves copy tags; calls clobber the caller-saved ones; joins meet);
#  4. no VFPU, COP0, COP2, SPECIAL2 (halt, mfic, mtic), cache, syscall, ll/sc, cfc1/ctc1; no $gp write and no $gp read other than
#     ME_CALLP's srl; reads and writes of $k0/$k1 and $23 (the guest base, -ffixed-23) reported; break and trap counts reported;
#  5. stack: the deepest chain of frames (-fstack-usage) from each entry, through jal edges and the pointer calls' known targets
#     (callmap: function with a pointer call -> what it can reach, "glue:<frame bytes>:<unit function>" for the host's callbacks).
# Prints a report; the last line is "mecheck: ok" or "mecheck: N faults".
import sys, struct, collections, re, os

D = sys.argv[1]
faults = []
def fault(s): faults.append(s); print('FAULT ' + s)

class Elf:
    def __init__(self, path):
        b = open(path, 'rb').read(); self.b = b
        assert b[:4] == b'\x7fELF' and b[4] == 1 and b[5] == 1, path
        (self.type, self.mach, _, _, _, shoff, self.flags, _, _, _, shentsize, shnum, shstrndx) = struct.unpack_from('<HHIIIIIHHHHHH', b, 16)
        sh = [struct.unpack_from('<IIIIIIIIII', b, shoff + i * shentsize) for i in range(shnum)]
        names = sh[shstrndx]
        def sname(o): e = b.index(b'\0', names[4] + o); return b[names[4] + o:e].decode()
        self.sec = [dict(name=sname(s[0]), type=s[1], flags=s[2], off=s[4], size=s[5], link=s[6], info=s[7]) for s in sh]
        self.sym = []
        for i, s in enumerate(self.sec):
            if s['type'] == 2:   # SYMTAB
                st = self.sec[s['link']]
                def stname(o): e = b.index(b'\0', st['off'] + o); return b[st['off'] + o:e].decode()
                for k in range(s['size'] // 16):
                    n, v, z, info, oth, shndx = struct.unpack_from('<IIIBBH', b, s['off'] + 16 * k)
                    self.sym.append(dict(name=stname(n), value=v, size=z, bind=info >> 4, type=info & 15, shndx=shndx))
        self.rel = collections.defaultdict(list)   # target section index -> [(offset, type, sym index)]
        for s in self.sec:
            if s['type'] == 9:
                for k in range(s['size'] // 8):
                    o, info = struct.unpack_from('<II', b, s['off'] + 8 * k)
                    self.rel[s['info']].append((o, info & 0xff, info >> 8))
            elif s['type'] == 4:
                fault('RELA section ' + s['name'])
        # files of local symbols (STT_FILE precedes each input file's locals)
        cur = None
        for s in self.sym:
            if s['type'] == 4: cur = s['name']
            s['file'] = cur if s['bind'] == 0 else None
    def is_text(self, i): return 0 < i < len(self.sec) and (self.sec[i]['flags'] & 4) != 0
    def word(self, si, off): return struct.unpack_from('<I', self.b, self.sec[si]['off'] + off)[0]
    def funcs(self):
        return [s for s in self.sym if s['type'] == 2 and self.is_text(s['shndx'])]
    def func_at(self, si, off):
        best = None
        for f in self.fby[si]:
            if f['value'] <= off < f['value'] + max(f['size'], 1): best = f
        return best
    def index_funcs(self):
        self.fby = collections.defaultdict(list)
        for f in self.funcs(): self.fby[f['shndx']].append(f)

R_32, R_26, R_HI, R_LO = 2, 4, 5, 6
RNAME = {2: 'R_MIPS_32', 4: 'R_MIPS_26', 5: 'R_MIPS_HI16', 6: 'R_MIPS_LO16', 7: 'R_MIPS_GPREL16', 8: 'R_MIPS_LITERAL', 9: 'R_MIPS_GOT16',
         10: 'R_MIPS_PC16', 11: 'R_MIPS_CALL16', 12: 'R_MIPS_GPREL32'}
LIBGCC_INT = set('__divdi3 __udivdi3 __moddi3 __umoddi3 __udivmoddi4 __ashldi3 __ashrdi3 __lshrdi3 __muldi3 __negdi2 __cmpdi2 __ucmpdi2 '
                 '__clzsi2 __ctzsi2 __clzdi2 __ctzdi2 __ffssi2 __ffsdi2 __popcountsi2 __popcountdi2 __paritysi2 __bswapsi2 __bswapdi2'.split())

U = Elf(os.path.join(D, 'meunit.o')); U.index_funcs()
A = Elf(os.path.join(D, 'all.o')); A.index_funcs()
entries = [l.strip() for l in open(os.path.join(D, 'entries.txt')) if l.strip()]

def fname(e, f): return f['name'] if f['bind'] != 0 else '%s(%s)' % (f['name'], (f['file'] or '?').replace('.c', ''))
def target(e, si, off, w, typ, symi):
    """the (section, offset) a relocation points at, with REL addends; None for an undefined symbol"""
    s = e.sym[symi]
    if s['shndx'] == 0: return None
    base = s['value'] if s['type'] != 3 else 0
    if typ == R_26: add = (w & 0x3ffffff) << 2
    elif typ == R_LO: add = (w & 0xffff) - ((w & 0x8000) << 1)
    elif typ == R_32: add = w
    else: add = 0
    return (s['shndx'], base + add)

# ---- 1. relocations
print('== 1. relocations')
types = collections.Counter(); undef = collections.Counter(); data_code = []
for si, rl in U.rel.items():
    text = U.is_text(si)
    for off, typ, symi in rl:
        types[(text, RNAME.get(typ, str(typ)))] += 1
        s = U.sym[symi]
        if (text and typ not in (R_26, R_HI, R_LO)) or (not text and typ != R_32):
            fault('relocation %s in %s+0x%x (%s)' % (RNAME.get(typ, typ), U.sec[si]['name'], off, s['name']))
        if s['shndx'] == 0: undef[s['name']] += 1
        elif not text and U.is_text(s['shndx']): data_code.append('%s+0x%x -> %s' % (U.sec[si]['name'], off, s['name'] or U.sec[s['shndx']]['name']))
for (t, n), c in sorted(types.items()): print('  %s %s: %d' % ('code' if t else 'data', n, c))
for n, c in sorted(undef.items()):
    if n in LIBGCC_INT: print('  libgcc integer helper (pure): %s x%d' % (n, c))
    else: fault('undefined: %s (x%d)' % (n, c))
for x in data_code: fault('code address in data: ' + x)
if not undef: print('  nothing undefined')
if not data_code: print('  no code address in a data section')

# ---- 2. address-taken functions the unit dropped
print('== 2. functions whose address is taken (all.o) and the unit')
kept = set((f['name'], f['file']) for f in U.funcs())
def taken(e):
    out = collections.defaultdict(set)
    for si, rl in e.rel.items():
        hi = {}
        for off, typ, symi in rl:
            s = e.sym[symi]
            if typ == R_HI: hi[symi] = (e.word(si, off) & 0xffff) << 16 if e.is_text(si) else 0; continue
            if typ not in (R_LO, R_32): continue
            w = e.word(si, off) if (e.is_text(si) or e.sec[si]['type'] != 8) else 0
            t = target(e, si, off, w, typ, symi)
            if t is None or not e.is_text(t[0]): continue
            o = t[1] + (hi.get(symi, 0) if typ == R_LO else 0)
            f = e.func_at(t[0], o)
            where = e.func_at(si, off) if e.is_text(si) else None
            if f: out[(f['name'], f['file'])].add(fname(e, where) if where else e.sec[si]['name'])
    return out
TA, TU = taken(A), taken(U)
for k in sorted(TA, key=lambda k: k[0]):
    st = 'kept' if k in kept else 'DROPPED'
    print('  %-24s %-8s taken in: %s' % (k[0] + ('(' + k[1].replace('.c', '') + ')' if k[1] else ''), st, ', '.join(sorted(TA[k]))))
dropped_taken = [k for k in TA if k not in kept]
print('  address-taken in the unit itself: %s' % ', '.join(sorted(k[0] for k in TU)))

# ---- 3. and 4. code
print('== 3. pointer calls and 4. instructions')
OTHER, SEG, CALLP = 0, 1, 2
CALLER_SAVED = set(list(range(1, 16)) + [24, 25, 31])
inst_count = collections.Counter(); sites = []; gp_other = []; k01 = []; r23 = []
callgraph = collections.defaultdict(set); ptr_funcs = set()
def dec(w):
    return w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
def is_branch(w):
    op, rs, rt, rd, sa, fn = dec(w)
    if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17): return True
    if op == 1 and rt in (0, 1, 2, 3, 16, 17, 18, 19): return True
    if op == 0x11 and rs == 8: return True
    if op == 0x12 and rs == 8: return True   # bvf/bvt (VFPU)
    return False
def likely(w):
    op, rs, rt, rd, sa, fn = dec(w)
    return op in (0x14, 0x15, 0x16, 0x17) or (op == 1 and rt in (2, 3, 18, 19)) or (op == 0x11 and rs == 8 and (rt & 2))
def btarget(pc, w): return pc + 4 + (((w & 0xffff) - ((w & 0x8000) << 1)) << 2)
def uncond(w):
    op, rs, rt, rd, sa, fn = dec(w)
    return op == 4 and rs == 0 and rt == 0
def writes(w):
    """GPRs written (besides HI/LO), and whether this is a call (links)"""
    op, rs, rt, rd, sa, fn = dec(w)
    if op == 0:
        if fn in (8, 12, 13, 15) or fn in (0x30, 0x31, 0x32, 0x33, 0x34, 0x36): return [], False   # jr syscall break sync traps
        if fn == 9: return [rd], True    # jalr
        if fn in (0x11, 0x13, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x2e, 0x2f): return [], False   # mthi mtlo mult div madd msub
        return [rd], False
    if op == 1: return ([31], True) if rt in (16, 17, 18, 19) else ([], False)
    if op == 3: return [31], True
    if op in (2, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17): return [], False
    if op in range(8, 16): return [rt], False
    if op in (0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x30, 0x38): return [rt], False
    if op in (0x28, 0x29, 0x2a, 0x2b, 0x2e, 0x31, 0x39, 0x2f): return [], False
    if op == 0x11: return ([rt], False) if rs in (0, 2) else ([], False)   # mfc1 cfc1
    if op == 0x1f: return ([rd], False) if fn == 0x20 else ([rt], False)   # bshfl: rd; ext/ins: rt
    if op in (0x10, 0x12): return ([rt], False) if rs in (0, 3) else ([], False)
    return [], False
def reads(w):
    op, rs, rt, rd, sa, fn = dec(w)
    if op == 0: return [rs, rt]
    if op in (1, 6, 7, 0x16, 0x17): return [rs]
    if op in (4, 5, 0x14, 0x15): return [rs, rt]
    if op in range(8, 16) or op in (0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x30, 0x31, 0x2f, 0x32, 0x36, 0x35): return [rs] + ([rt] if op in (0x22, 0x26) else [])   # (lwl/lwr merge into rt; lwc1's rt is an FPR)
    if op in (0x28, 0x29, 0x2a, 0x2b, 0x2e, 0x38): return [rs, rt]
    if op in (0x39, 0x3a, 0x3e, 0x3d): return [rs]   # swc1 and the VFPU's stores: rt is not a GPR
    if op == 0x11 and rs in (4, 6): return [rt]
    if op == 0x1f: return [rs, rt]
    return []
def classify(w):
    op, rs, rt, rd, sa, fn = dec(w)
    if op in (0x18, 0x19, 0x1b, 0x34, 0x37, 0x3c, 0x3f, 0x32, 0x36, 0x3a, 0x3e, 0x35, 0x3d) or op == 0x12: return 'VFPU/COP2'
    if op == 0x10: return 'COP0'
    if op == 0x1c: return 'SPECIAL2 (halt/mfic/mtic)'
    if op == 0x2f: return 'cache'
    if op in (0x30, 0x38): return 'll/sc'
    if op == 0x11 and rs in (2, 6): return 'cfc1/ctc1'
    if op == 0x11 and (rs & 0x1f) == 0x11: return 'double FPU'
    if op == 0 and fn == 12: return 'syscall'
    if op == 0 and fn == 13: return 'break'
    if op == 0 and fn in (0x30, 0x31, 0x32, 0x33, 0x34, 0x36): return 'trap'
    if op == 1 and rt in (8, 9, 10, 11, 12, 14): return 'trap'
    if op == 0 and fn == 15: return 'sync'
    return None
BAD = {'VFPU/COP2', 'COP0', 'SPECIAL2 (halt/mfic/mtic)', 'cache', 'll/sc', 'cfc1/ctc1', 'double FPU', 'syscall'}

for f in sorted(U.funcs(), key=lambda f: (f['shndx'], f['value'])):
    si, base, n = f['shndx'], f['value'], f['size'] // 4
    nm = fname(U, f)
    rel = {o: (t, s) for o, t, s in U.rel.get(si, []) if base <= o < base + 4 * n}
    W = [U.word(si, base + 4 * i) for i in range(n)]
    for i, w in enumerate(W):
        c = classify(w)
        if c: inst_count[c] += 1
        if c in BAD: fault('%s at %s+0x%x (%08x)' % (c, nm, 4 * i, w))
        op, rs, rt, rd, sa, fn = dec(w)
        wr, _ = writes(w)
        if 28 in wr: fault('$gp written at %s+0x%x (%08x)' % (nm, 4 * i, w))
        rr = reads(w)
        srl_gp = op == 0 and fn == 2 and rs == 0 and rt == 28 and sa == 31
        if 28 in rr and not srl_gp: gp_other.append('%s+0x%x' % (nm, 4 * i))
        if 26 in rr + wr or 27 in rr + wr: k01.append('%s+0x%x' % (nm, 4 * i))
        if 23 in rr + wr: r23.append('%s+0x%x' % (nm, 4 * i))
        # call graph (jal / j with relocations)
        if op in (2, 3):
            r = rel.get(base + 4 * i)
            if not r or r[0] != R_26: fault('j/jal without R_MIPS_26 at %s+0x%x' % (nm, 4 * i)); continue
            t = target(U, si, base + 4 * i, w, R_26, r[1])
            if t is None: callgraph[nm].add('UND:' + U.sym[r[1]]['name'])
            else:
                g = U.func_at(*t)
                if g is None: fault('jal into no function at %s+0x%x' % (nm, 4 * i))
                elif g is not f or op == 3: callgraph[nm].add(fname(U, g))
    # data flow
    IN = [None] * n; DS = [False] * n   # DS: delay slots (run with their jump)
    IN[0] = ((OTHER,) * 32, frozenset())   # a state: the registers' tags, and the tags of words spilled to the stack (sp offset, tag)
    work = [0]
    def merge(j, st):
        if j < 0 or j >= n: return
        if IN[j] is None: IN[j] = st; work.append(j); return
        m = (tuple(a if a == b else OTHER for a, b in zip(IN[j][0], st[0])), IN[j][1] & st[1])
        if m != IN[j]: IN[j] = m; work.append(j)
    def simm(w): return (w & 0xffff) - ((w & 0x8000) << 1)
    def xfer(w, st):
        op, rs, rt, rd, sa, fn = dec(w)
        regs = list(st[0]); slots = st[1]
        wr, call = writes(w)
        if op == 0 and fn == 2 and rs == 0 and rt == 28 and sa == 31: t = SEG
        elif op == 0x1f and fn == 4 and sa == 31 and rd == 31: t = CALLP if regs[rs] == SEG else OTHER
        elif op == 0 and fn in (0x21, 0x25) and rt == 0: t = regs[rs]    # move (addu / or with $0)
        elif op == 0 and fn in (0x21, 0x25) and rs == 0: t = regs[rt]
        elif op == 0x23 and rs == 29: t = dict(slots).get(simm(w), OTHER)   # lw of a spilled word
        else: t = OTHER
        if rs == 29 and op in (0x28, 0x29, 0x2a, 0x2b, 0x2e, 0x39, 0x3a, 0x3e, 0x3d):   # a store to the stack: the words it overlaps
            o = simm(w); hi = o + (15 if op in (0x3e, 0x3d) else 3)
            slots = frozenset(x for x in slots if not (o - 3 <= x[0] <= hi))
            if op == 0x2b and regs[rt] != OTHER: slots = slots | {(o, regs[rt])}
        for r in wr:
            if r: regs[r] = t
        if 29 in wr: slots = frozenset()
        return (tuple(regs), slots), call
    def clobber(st):
        return (tuple(OTHER if r in CALLER_SAVED else st[0][r] for r in range(32)), st[1])
    while work:
        i = work.pop()
        st = IN[i]; w = W[i]
        op, rs, rt, rd, sa, fn = dec(w)
        jr = op == 0 and fn == 8
        jalr = op == 0 and fn == 9
        if is_branch(w) or op in (2, 3) or jr or jalr:
            s1, call = xfer(w, st)          # the jump (links ra if a call)
            if i + 1 >= n: fault('jump in the last word of %s' % nm); continue
            DS[i + 1] = True
            s2, _ = xfer(W[i + 1], s1)      # its delay slot
            if is_branch(w) and not (op == 1 and rt in (16, 17, 18, 19)):
                tgt = (btarget(base + 4 * i, w) - base) // 4
                if not (0 <= tgt < n): fault('branch out of %s at +0x%x' % (nm, 4 * i))
                merge(tgt, s2)
                if not uncond(w): merge(i + 2, s1 if likely(w) else s2)
            elif op == 1:                   # bal / bgezal / bltzal: a call to a PC-relative target
                fault('PC-relative call at %s+0x%x' % (nm, 4 * i)); merge(i + 2, clobber(s2))
            elif op == 3 or jalr: merge(i + 2, clobber(s2))
            elif op == 2:                   # j: a tail call, or a jump inside the function
                r = rel.get(base + 4 * i); t = target(U, si, base + 4 * i, w, R_26, r[1]) if r else None
                if t and t[0] == si and base <= t[1] < base + 4 * n: merge((t[1] - base) // 4, s2)
            # jr: leaves the function
        else:
            s1, call = xfer(w, st)
            merge(i + 1, s1)
    for i, w in enumerate(W):
        op, rs, rt, rd, sa, fn = dec(w)
        if IN[i] is not None and op == 0 and (fn == 9 or (fn == 8 and rs != 31)):
            sites.append((nm, 4 * i, 'jalr' if fn == 9 else 'jr', IN[i][0][rs] == CALLP)); ptr_funcs.add(nm)
    unreached = [i for i in range(n) if IN[i] is None and not DS[i]]
    if unreached: print('  note: %s: %d words reached by no path (first +0x%x)' % (nm, len(unreached), 4 * unreached[0]))

bad_sites = [s for s in sites if not s[3]]
print('  pointer jumps (jalr, jr not ra): %d, through ME_CALLP: %d' % (len(sites), len(sites) - len(bad_sites)))
for s in sites: print('    %-34s +0x%-5x %-4s %s' % (s[0], s[1], s[2], 'ok' if s[3] else 'NOT ME_CALLP'))
for s in bad_sites: fault('pointer jump not through ME_CALLP: %s+0x%x' % (s[0], s[1]))
for k, c in sorted(inst_count.items()): print('  %s: %d' % (k, c))
print('  $gp read other than by ME_CALLP: %s' % (', '.join(gp_other[:8]) or 'none'))
for g in gp_other: fault('$gp read at ' + g)
print('  $k0/$k1: %s' % (', '.join(k01[:8]) or 'none'))
for g in k01: fault('$k0/$k1 at ' + g)
print('  $23 (guest base) used at: %s' % (', '.join(r23[:8]) or 'none'))

# ---- 5. stack
print('== 5. stack (bytes: frames from -fstack-usage)')
su = collections.defaultdict(list)
for l in open(os.path.join(D, 'stack.txt')):
    m = re.match(r'(.*?):\d+:\d+:(\S+)\t(\d+)\t(\S+)', l.rstrip('\n'))
    if not m: continue
    su[m.group(2)].append((os.path.basename(m.group(1)), int(m.group(3)), m.group(4)))
def frame(nm):
    base = nm.split('(')[0]; fl = nm[len(base) + 1:-1] + '.c' if '(' in nm else None
    c = [x for x in su.get(base, []) or su.get(re.sub(r'\.\d+$', '', base), []) if fl is None or x[0] == fl]   # (.su names clones without .N)
    if not c: return None
    if any(x[2] != 'static' for x in c): fault('stack of %s is %s' % (nm, ','.join(x[2] for x in c)))
    return max(x[1] for x in c)
callmap = {}
if len(sys.argv) > 2:
    for l in open(sys.argv[2]):
        l = l.split('#')[0].split()
        if l: callmap[l[0]] = l[1:]
for p in sorted(ptr_funcs):
    if p not in callmap: fault('no callmap line for the pointer calls in ' + p)
memo = {}; onpath = set(); recur = set()
def depth(nm):
    if nm in memo: return memo[nm]
    if nm.startswith('UND:'): return (0, [nm])
    if nm in onpath: recur.add(nm); return (0, [nm + ' (recursion)'])
    onpath.add(nm)
    fr = frame(nm)
    if fr is None: fault('no stack size for ' + nm); fr = 0
    best = (0, [])
    for c in sorted(callgraph.get(nm, ())):
        d = depth(c)
        if d[0] > best[0]: best = d
    for t in callmap.get(nm, []):
        if t.startswith('glue:'):
            _, gb, fn = t.split(':'); d = depth(fn) if fn != '-' else (0, [])
            d = (int(gb) + d[0], ['glue(%s)' % gb] + d[1])
        else: d = depth(t)
        if d[0] > best[0]: best = d
    onpath.discard(nm)
    r = (fr + best[0], [nm] + best[1]); memo[nm] = r
    return r
worst = (0, [])
for e in entries:
    if not any(f['name'] == e for f in U.funcs()): fault('entry %s not in the unit' % e); continue
    d = depth(e)
    print('  %-16s %6d  %s' % (e, d[0], ' > '.join(d[1][:12]) + (' ...' if len(d[1]) > 12 else '')))
    if d[0] > worst[0]: worst = d
for r in sorted(recur): fault('recursion through ' + r)
print('  deepest: %d bytes (%s)' % (worst[0], ' > '.join(worst[1])))
print('== the unit: %d functions, %d bytes of code' % (len(U.funcs()), sum(f['size'] for f in U.funcs())))
if dropped_taken: print('  review: address-taken functions the unit dropped: %s' % ', '.join(sorted(k[0] for k in dropped_taken)))
print('mecheck: ' + ('ok' if not faults else '%d faults' % len(faults)))
