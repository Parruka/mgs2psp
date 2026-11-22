#!/usr/bin/env python3
"""prof_guest.py: a PspProf dump attributed to the guest (PS2) instructions the PSP code came from.

  prof_guest.py <dump> <steps> <elf> <ovl> <elf_g> <ovl_g> <ps2 elf> <stage.bin> [out.txt]

<elf> / <ovl>: the profiled build's ELF and stage overlay.  <elf_g> / <ovl_g>: the same link from objects compiled with -g.  PCs are
moved by function and offset (psp-nm), so a -g link whose functions shifted still maps; GCC 15's -g code differs from the plain code in
a few functions (a branch offset or one instruction), where the lines after the first difference are approximate.  Each executed PSP instruction is mapped (addr2line -i, outermost frame)
to its line; a line of a generated header carries the guest address in its leading /*hex*/, and the guest word gives the class.
Lines of a generated function without a guest address are its entry / exit (the FN_ENTER line, the register loads, the closing brace).
Cost per step = (1.4 x instructions + 70 x (I-miss + D-miss)) / 333,000 (speed_notes §21)."""
import sys, os, re, struct, subprocess, collections

dump, steps, elf_p, ovl_p, elf_g, ovl_g, ps2elf, stagebin = sys.argv[1:9]
steps = int(steps)
out = open(sys.argv[9], 'w') if len(sys.argv) > 9 else sys.stdout
BIN = '/home/claude/pspsdk/pspdev/bin/'

D = open(ps2elf, 'rb').read()
SD = open(stagebin, 'rb').read()
SBASE = struct.unpack_from('<I', SD, 0)[0]
def W(a):
    if SBASE <= a < SBASE + len(SD): return struct.unpack_from('<I', SD, a - SBASE)[0]
    o = a - 0x100000 + 0x80
    return struct.unpack_from('<I', D, o)[0] if 0 <= o < len(D) - 3 else None

SPECIAL = {0: 'sll', 2: 'srl', 3: 'sra', 4: 'sllv', 6: 'srlv', 7: 'srav', 8: 'jr', 9: 'jalr', 10: 'movz', 11: 'movn', 12: 'syscall',
           13: 'break', 15: 'sync', 16: 'mfhi', 17: 'mthi', 18: 'mflo', 19: 'mtlo', 20: 'dsllv', 22: 'dsrlv', 23: 'dsrav', 24: 'mult',
           25: 'multu', 26: 'div', 27: 'divu', 32: 'add', 33: 'addu', 34: 'sub', 35: 'subu', 36: 'and', 37: 'or', 38: 'xor', 39: 'nor',
           40: 'mfsa', 41: 'mtsa', 42: 'slt', 43: 'sltu', 44: 'dadd', 45: 'daddu', 46: 'dsub', 47: 'dsubu', 56: 'dsll', 58: 'dsrl',
           59: 'dsra', 60: 'dsll32', 62: 'dsrl32', 63: 'dsra32'}
OPS = {2: 'j', 3: 'jal', 4: 'beq', 5: 'bne', 6: 'blez', 7: 'bgtz', 8: 'addi', 9: 'addiu', 10: 'slti', 11: 'sltiu', 12: 'andi', 13: 'ori',
       14: 'xori', 15: 'lui', 20: 'beql', 21: 'bnel', 22: 'blezl', 23: 'bgtzl', 24: 'daddi', 25: 'daddiu', 26: 'ldl', 27: 'ldr',
       30: 'lq', 31: 'sq', 32: 'lb', 33: 'lh', 34: 'lwl', 35: 'lw', 36: 'lbu', 37: 'lhu', 38: 'lwr', 39: 'lwu', 40: 'sb', 41: 'sh',
       42: 'swl', 43: 'sw', 44: 'sdl', 45: 'sdr', 46: 'swr', 47: 'cache', 49: 'lwc1', 51: 'pref', 54: 'lqc2', 55: 'ld', 57: 'swc1',
       62: 'sqc2', 63: 'sd'}
def mnem(w):
    op = w >> 26
    if op == 0:
        f = w & 63
        if w == 0: return 'nop'
        return SPECIAL.get(f, 'special%d' % f)
    if op == 1: return 'regimm'
    if op == 16: return 'cop0'
    if op == 17:
        fmt = (w >> 21) & 31
        if fmt == 8: return 'bc1'
        if fmt in (0, 2, 4, 6): return {0: 'mfc1', 2: 'cfc1', 4: 'mtc1', 6: 'ctc1'}[fmt]
        if fmt == 16:
            fn = w & 63
            return {0: 'add.s', 1: 'sub.s', 2: 'mul.s', 3: 'div.s', 4: 'sqrt.s', 5: 'abs.s', 6: 'mov.s', 7: 'neg.s', 22: 'rsqrt.s',
                    24: 'adda.s', 25: 'suba.s', 26: 'mula.s', 28: 'madd.s', 29: 'msub.s', 30: 'madda.s', 31: 'msuba.s', 36: 'cvt.w.s',
                    40: 'max.s', 41: 'min.s', 48: 'c.f.s', 50: 'c.eq.s', 52: 'c.lt.s', 54: 'c.le.s'}.get(fn, 'fpu%d' % fn)
        if fmt == 20: return 'cvt.s.w'
        return 'cop1'
    if op == 18:
        if (w >> 25) & 1: return 'vu0op'
        fmt = (w >> 21) & 31
        return {1: 'qmfc2', 2: 'cfc2', 5: 'qmtc2', 6: 'ctc2', 8: 'bc2'}.get(fmt, 'cop2')
    if op == 28: return 'mmi'
    return OPS.get(op, 'op%d' % op)

def gclass(m):
    if m in ('lb', 'lbu', 'lh', 'lhu', 'lw', 'lwu'): return 'load 8-32'
    if m in ('ld', 'lq'): return 'load 64/128'
    if m in ('lwl', 'lwr', 'ldl', 'ldr'): return 'load unaligned'
    if m in ('sb', 'sh', 'sw'): return 'store 8-32'
    if m in ('sd', 'sq'): return 'store 64/128'
    if m in ('swl', 'swr', 'sdl', 'sdr'): return 'store unaligned'
    if m in ('lwc1', 'swc1'): return 'fpu load/store'
    if m in ('lqc2', 'sqc2'): return 'vu0 load/store'
    if m in ('jal', 'jalr'): return 'call'
    if m == 'jr': return 'jr (return / switch / tail)'
    if m in ('j',): return 'jump'
    if m in ('beq', 'bne', 'blez', 'bgtz', 'beql', 'bnel', 'blezl', 'bgtzl', 'regimm', 'bc1', 'bc2'): return 'branch'
    if m in ('dadd', 'daddu', 'dsub', 'dsubu', 'dsll', 'dsrl', 'dsra', 'dsll32', 'dsrl32', 'dsra32', 'dsllv', 'dsrlv', 'dsrav',
             'daddi', 'daddiu'): return 'alu 64'
    if m in ('mult', 'multu', 'div', 'divu', 'mfhi', 'mflo', 'mthi', 'mtlo'): return 'mul/div hi/lo'
    if m == 'mmi': return 'mmi'
    if m == 'vu0op': return 'vu0 macro op'
    if m in ('qmfc2', 'qmtc2', 'cfc2', 'ctc2', 'cop2'): return 'vu0 move/ctrl'
    if m.endswith('.s') or m in ('mfc1', 'mtc1', 'cfc1', 'ctc1', 'cvt.s.w', 'cop1') or m.startswith('fpu'): return 'fpu op'
    if m in ('syscall', 'break', 'sync', 'cop0', 'cache', 'pref', 'mfsa', 'mtsa'): return 'system'
    if m == 'nop': return 'nop'
    return 'alu 32'

# 1. the dump
rows = []
tot = None
for l in open(dump):
    if l.startswith('#'):
        tot = l; continue
    p = l.split()
    if len(p) >= 4: rows.append((int(p[0], 16), int(p[1]), int(p[2]), int(p[3])))

# 2. overlay range
def sect(elf, name):
    for l in subprocess.run([BIN + 'psp-objdump', '-h', elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) > 4 and p[1] == name: return int(p[3], 16), int(p[2], 16)
    return None
ob, osz = sect(ovl_p, '.ovl')

# 3. addr2line -a -f -i, outermost frame
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
        fn = l; loc = o[i + 1] if i + 1 < len(o) else '??:0'
        frames.append((fn, loc)); i += 2
    flush()
    return res

import bisect
def syms(elf):
    """function symbols in address order: [(start, size, (name, k))], k = the name's occurrence count before it"""
    o = subprocess.run([BIN + 'psp-nm', '-S', '--defined-only', '-n', elf], capture_output=True, text=True).stdout.splitlines()
    seen = collections.Counter(); res = []
    for l in o:
        p = l.split()
        if len(p) == 4 and p[2] in ('t', 'T'):
            a, sz, n = int(p[0], 16), int(p[1], 16), p[3]
            res.append((a, sz, (n, seen[n]))); seen[n] += 1
    return res
def mover(pe, ge):
    sp = syms(pe); sg = {k: a for a, sz, k in syms(ge)}
    st = [a for a, sz, k in sp]
    def mv(pc):
        i = bisect.bisect_right(st, pc) - 1
        if i < 0: return None
        a, sz, k = sp[i]
        if pc >= a + sz or k not in sg: return None
        return sg[k] + (pc - a)
    return mv
mv_m = mover(elf_p, elf_g); mv_o = mover(ovl_p, ovl_g)
pcs_m = sorted({r[0] for r in rows if not (ob <= r[0] < ob + osz)})
pcs_o = sorted({r[0] for r in rows if ob <= r[0] < ob + osz})
gm = {pc: mv_m(pc) for pc in pcs_m}; go = {pc: mv_o(pc) for pc in pcs_o}
l_m = a2l(elf_g, sorted({g for g in gm.values() if g is not None}))
l_o = a2l(ovl_g, sorted({g for g in go.values() if g is not None}))
loc = {}
for pc, g in gm.items(): loc[pc] = l_m.get(g, ('??', '??:0')) if g is not None else ('(no symbol)', '??:0')
for pc, g in go.items(): loc[pc] = l_o.get(g, ('??', '??:0')) if g is not None else ('(no symbol)', '??:0')

# 4. lines -> guest address
cache = {}
def line_text(path, n):
    if path not in cache:
        try: cache[path] = open(path, errors='replace').read().split('\n')
        except OSError: cache[path] = []
    t = cache[path]
    return t[n - 1] if 0 < n <= len(t) else ''
GA = re.compile(r'^\s*/\*([0-9a-f]+)\*/')
agg = collections.defaultdict(lambda: [0, 0, 0])      # class -> instr, imiss, dmiss
mn = collections.defaultdict(lambda: [0, 0, 0])
lines_exec = collections.defaultdict(int)             # (path, line) -> max count of its PCs (about the guest executions)
line_cls = {}
for pc, c, im, dm in rows:
    fn, lc = loc.get(pc, ('??', '??:0'))
    path, _, ln = lc.rpartition(':')
    ln = ln.split()[0] if ln else '0'
    try: ln = int(ln)
    except ValueError: ln = 0
    base = os.path.basename(path)
    if base.startswith('gen_') and base.endswith('.h'):
        t = line_text(path, ln)
        m = GA.match(t)
        if m:
            w = W(int(m.group(1), 16))
            mm = mnem(w) if w is not None else '?'
            k = gclass(mm)
            if 'IRQ_POLL' in t and k in ('branch', 'jump'): k += ' + loop poll'
        elif t.startswith('FN_ENTER') or t.startswith('  float') or t.startswith('  int64_t') or t.startswith('  int32_t') or t.startswith('static void') or t.startswith('{'):
            k = 'entry: register loads'; mm = 'entry'
        elif t.strip() == '}':
            k = 'exit: register stores'; mm = 'exit'
        else:
            k = 'generated, other (%s)' % t.strip()[:24]; mm = 'other'
        key = (path, ln)
        lines_exec[key] = max(lines_exec[key], c)
        line_cls[key] = (k, mm)
    else:
        k = 'runtime: ' + fn; mm = None
    a = agg[k]; a[0] += c; a[1] += im; a[2] += dm
    if mm: b = mn[mm]; b[0] += c; b[1] += im; b[2] += dm

# guest executions per class (max count over a line's PCs: about how often the line ran)
gexec = collections.defaultdict(int); gexec_m = collections.defaultdict(int)
for key, n in lines_exec.items():
    gexec[line_cls[key][0]] += n; gexec_m[line_cls[key][1]] += n

def ms(v): return (1.4 * v[0] + 70 * (v[1] + v[2])) / 333000 / steps
T = [sum(v[i] for v in agg.values()) for i in range(3)]
print(tot.strip() if tot else '', file=out)
print('steps %d; per step: %.3f M instructions, %.1f k I-miss, %.1f k D-miss, model %.2f ms' % (steps, T[0] / steps / 1e6, T[1] / steps / 1e3, T[2] / steps / 1e3, ms(T)), file=out)
print('\n%-40s %9s %6s %8s %7s %7s %8s %7s' % ('class', 'instr/st', '%', 'guest/st', 'psp/g', 'Imiss/st', 'Dmiss/st', 'ms'), file=out)
gen_rt = lambda k: k.startswith('runtime: ')
rest = [0, 0, 0]
for k, v in sorted(agg.items(), key=lambda kv: -ms(kv[1])):
    if gen_rt(k) and ms(v) < 0.15:
        for i in range(3): rest[i] += v[i]
        continue
    g = gexec.get(k, 0)
    print('%-40s %9.0f %6.2f %8.0f %7s %7.2f %8.2f %7.2f' % (k[:40], v[0] / steps, 100.0 * v[0] / T[0], g / steps, ('%.1f' % (v[0] / g)) if g else '-', v[1] / steps / 1e3, v[2] / steps / 1e3, ms(v)), file=out)
print('%-40s %9.0f %6.2f %8s %7s %7.2f %8.2f %7.2f' % ('runtime: the rest (each < 0.15 ms)', rest[0] / steps, 100.0 * rest[0] / T[0], '-', '-', rest[1] / steps / 1e3, rest[2] / steps / 1e3, ms(rest)), file=out)
print('\nby guest mnemonic (generated code only)', file=out)
for k, v in sorted(mn.items(), key=lambda kv: -ms(kv[1]))[:45]:
    g = gexec_m.get(k, 0)
    print('%-14s %9.0f %6.2f %8.0f %7s %7.2f %8.2f %7.2f' % (k, v[0] / steps, 100.0 * v[0] / T[0], g / steps, ('%.1f' % (v[0] / g)) if g else '-', v[1] / steps / 1e3, v[2] / steps / 1e3, ms(v)), file=out)
