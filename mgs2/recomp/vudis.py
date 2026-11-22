"""VU0 / VU1 micro program disassembler (listing only): usage vudis.py <elf> <start vaddr> <end vaddr> [symbols file = /tmp/all.dis].
Each 64-bit instruction word: low 32 = lower op, high 32 = upper op (the game's packets store the pair as two 32-bit words,
lower first).  Covers the ops VCL-compiled renderers use; unknown encodings print as raw words."""
import sys, struct, re
UP = {0x00: 'addx', 0x01: 'addy', 0x02: 'addz', 0x03: 'addw', 0x04: 'subx', 0x05: 'suby', 0x06: 'subz', 0x07: 'subw', 0x08: 'maddx', 0x09: 'maddy', 0x0a: 'maddz', 0x0b: 'maddw',
      0x0c: 'msubx', 0x0d: 'msuby', 0x0e: 'msubz', 0x0f: 'msubw', 0x10: 'maxx', 0x11: 'maxy', 0x12: 'maxz', 0x13: 'maxw', 0x14: 'minix', 0x15: 'miniy', 0x16: 'miniz', 0x17: 'miniw',
      0x18: 'mulx', 0x19: 'muly', 0x1a: 'mulz', 0x1b: 'mulw', 0x1c: 'mulq', 0x1d: 'maxi', 0x1e: 'muli', 0x1f: 'minii', 0x20: 'addq', 0x21: 'maddq', 0x22: 'addi', 0x23: 'maddi',
      0x24: 'subq', 0x25: 'msubq', 0x26: 'subi', 0x27: 'msubi', 0x28: 'add', 0x29: 'madd', 0x2a: 'mul', 0x2b: 'max', 0x2c: 'sub', 0x2d: 'msub', 0x2e: 'opmsub', 0x2f: 'mini'}
UP2 = {0x00: 'addax', 0x01: 'adday', 0x02: 'addaz', 0x03: 'addaw', 0x04: 'subax', 0x05: 'subay', 0x06: 'subaz', 0x07: 'subaw', 0x08: 'maddax', 0x09: 'madday', 0x0a: 'maddaz', 0x0b: 'maddaw',
       0x0c: 'msubax', 0x0d: 'msubay', 0x0e: 'msubaz', 0x0f: 'msubaw', 0x10: 'itof0', 0x11: 'itof4', 0x12: 'itof12', 0x13: 'itof15', 0x14: 'ftoi0', 0x15: 'ftoi4', 0x16: 'ftoi12', 0x17: 'ftoi15',
       0x18: 'mulax', 0x19: 'mulay', 0x1a: 'mulaz', 0x1b: 'mulaw', 0x1c: 'mulaq', 0x1d: 'abs', 0x1e: 'mulai', 0x1f: 'clip', 0x20: 'addaq', 0x21: 'maddaq', 0x22: 'addai', 0x23: 'maddai',
       0x24: 'subaq', 0x25: 'msubaq', 0x26: 'subai', 0x27: 'msubai', 0x28: 'adda', 0x29: 'madda', 0x2a: 'mula', 0x2c: 'suba', 0x2d: 'msuba', 0x2e: 'opmula', 0x2f: 'nop'}
def dest(d): return ''.join(c for c, b in zip('xyzw', (8, 4, 2, 1)) if d & b)
def upper(w):
    d = (w >> 21) & 15; ft = (w >> 16) & 31; fs = (w >> 11) & 31; fd = (w >> 6) & 31; fn = w & 0x3f; bc = 'xyzw'[w & 3]
    fl = ''.join(c for c, b in (('I', 1 << 31), ('E', 1 << 30), ('M', 1 << 29), ('D', 1 << 28), ('T', 1 << 27)) if w & b)
    if fn < 0x30:
        n = UP[fn]; s = '%s.%s vf%02d, vf%02d, vf%02d' % (n, dest(d), fd, fs, ft)
        if fn < 0x1c or fn in (0x1d, 0x1e, 0x1f, 0x22, 0x23, 0x26, 0x27): s = '%s.%s vf%02d, vf%02d, %s' % (n, dest(d), fd, fs, ('vf%02d%s' % (ft, bc)) if fn < 0x1c else 'I')
        if fn in (0x1c, 0x20, 0x21, 0x24, 0x25): s = '%s.%s vf%02d, vf%02d, Q' % (n, dest(d), fd, fs)
    else:
        n = UP2.get((((w >> 6) & 31) << 2) | (fn & 3), '?%02x' % fn) if fn >= 0x3c else UP2.get(fn, '?%02x' % fn)
        if n == 'nop': s = 'nop'
        elif n == 'clip': s = 'clipw.xyz vf%02d, vf%02dw' % (fs, ft)
        elif n == 'abs': s = 'abs.%s vf%02d, vf%02d' % (dest(d), ft, fs)
        elif n.startswith('itof') or n.startswith('ftoi'): s = '%s.%s vf%02d, vf%02d' % (n, dest(d), ft, fs)
        elif n[-1] in 'xyzw' and n not in ('mulaq',): s = '%s.%s ACC, vf%02d, vf%02d%s' % (n[:-1], dest(d), fs, ft, n[-1])
        elif n.endswith('q'): s = '%s.%s ACC, vf%02d, Q' % (n[:-1], dest(d), fs)
        elif n.endswith('i'): s = '%s.%s ACC, vf%02d, I' % (n[:-1], dest(d), fs)
        else: s = '%s.%s ACC, vf%02d, vf%02d' % (n, dest(d), fs, ft)
    return s + (('  [' + fl + ']') if fl else '')
def lower(w, up):
    if w == 0x8000033c: return 'nop'
    if up & (1 << 31): return 'loi 0x%08x (%g)' % (w, struct.unpack('<f', struct.pack('<I', w))[0])
    op = w >> 25; d = (w >> 21) & 15; it = (w >> 16) & 31; is_ = (w >> 11) & 31; id_ = (w >> 6) & 31; imm11 = w & 0x7ff; imm11s = imm11 - 0x800 if imm11 & 0x400 else imm11
    imm15 = ((w >> 21) & 15) << 11 | (w & 0x7ff); imm12 = ((w >> 21) & 1) << 11 | (w & 0x7ff)
    if op == 0x00: return 'lq.%s vf%02d, %d(vi%02d)' % (dest(d), it, imm11s, is_)
    if op == 0x01: return 'sq.%s vf%02d, %d(vi%02d)' % (dest(d), is_, imm11s, it)
    if op == 0x04: return 'ilw.%s vi%02d, %d(vi%02d)' % (dest(d), it, imm11s, is_)
    if op == 0x05: return 'isw.%s vi%02d, %d(vi%02d)' % (dest(d), it, imm11s, is_)
    if op == 0x08: return 'iaddiu vi%02d, vi%02d, %d' % (it, is_, imm15)
    if op == 0x09: return 'isubiu vi%02d, vi%02d, %d' % (it, is_, imm15)
    if op == 0x10: return 'fceq vi01, 0x%06x' % (w & 0xffffff)
    if op == 0x11: return 'fcset 0x%06x' % (w & 0xffffff)
    if op == 0x12: return 'fcand vi01, 0x%06x' % (w & 0xffffff)
    if op == 0x13: return 'fcor vi01, 0x%06x' % (w & 0xffffff)
    if op == 0x14: return 'fseq vi%02d, 0x%03x' % (it, imm12)
    if op == 0x15: return 'fsset 0x%03x' % imm12
    if op == 0x16: return 'fsand vi%02d, 0x%03x' % (it, imm12)
    if op == 0x17: return 'fsor vi%02d, 0x%03x' % (it, imm12)
    if op == 0x18: return 'fmeq vi%02d, vi%02d' % (it, is_)
    if op == 0x1a: return 'fmand vi%02d, vi%02d' % (it, is_)
    if op == 0x1b: return 'fmor vi%02d, vi%02d' % (it, is_)
    if op == 0x1c: return 'fcget vi%02d' % it
    if op == 0x20: return 'b %+d' % imm11s
    if op == 0x21: return 'bal vi%02d, %+d' % (it, imm11s)
    if op == 0x24: return 'jr vi%02d' % is_
    if op == 0x25: return 'jalr vi%02d, vi%02d' % (it, is_)
    if op == 0x28: return 'ibeq vi%02d, vi%02d, %+d' % (it, is_, imm11s)
    if op == 0x29: return 'ibne vi%02d, vi%02d, %+d' % (it, is_, imm11s)
    if op == 0x2c: return 'ibltz vi%02d, %+d' % (is_, imm11s)
    if op == 0x2d: return 'ibgtz vi%02d, %+d' % (is_, imm11s)
    if op == 0x2e: return 'iblez vi%02d, %+d' % (is_, imm11s)
    if op == 0x2f: return 'ibgez vi%02d, %+d' % (is_, imm11s)
    if op == 0x40:
        fn = w & 0x3f; i5 = (w >> 6) & 31; i5s = i5 - 32 if i5 & 16 else i5
        if fn == 0x30: return 'iadd vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if fn == 0x31: return 'isub vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if fn == 0x32: return 'iaddi vi%02d, vi%02d, %d' % (it, is_, i5s)
        if fn == 0x34: return 'iand vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if fn == 0x35: return 'ior vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if fn >= 0x3c:
            idx = (w >> 6) & 31; sel = fn & 3; fsf, ftf = (w >> 21) & 3, (w >> 23) & 3
            t = {(12, 0): 'move.%s vf%02d, vf%02d' % (dest(d), it, is_), (12, 1): 'mr32.%s vf%02d, vf%02d' % (dest(d), it, is_),
                 (13, 0): 'lqi.%s vf%02d, (vi%02d++)' % (dest(d), it, is_), (13, 1): 'sqi.%s vf%02d, (vi%02d++)' % (dest(d), is_, it),
                 (13, 2): 'lqd.%s vf%02d, (--vi%02d)' % (dest(d), it, is_), (13, 3): 'sqd.%s vf%02d, (--vi%02d)' % (dest(d), is_, it),
                 (14, 0): 'div Q, vf%02d%s, vf%02d%s' % (is_, 'xyzw'[fsf], it, 'xyzw'[ftf]), (14, 1): 'sqrt Q, vf%02d%s' % (it, 'xyzw'[ftf]),
                 (14, 2): 'rsqrt Q, vf%02d%s, vf%02d%s' % (is_, 'xyzw'[fsf], it, 'xyzw'[ftf]), (14, 3): 'waitq',
                 (15, 0): 'mtir vi%02d, vf%02d%s' % (it, is_, 'xyzw'[fsf]), (15, 1): 'mfir.%s vf%02d, vi%02d' % (dest(d), it, is_),
                 (15, 2): 'ilwr.%s vi%02d, (vi%02d)' % (dest(d), it, is_), (15, 3): 'iswr.%s vi%02d, (vi%02d)' % (dest(d), it, is_),
                 (16, 0): 'rnext.%s vf%02d, R' % (dest(d), it), (16, 1): 'rget.%s vf%02d, R' % (dest(d), it), (16, 2): 'rinit R, vf%02d%s' % (is_, 'xyzw'[fsf]), (16, 3): 'rxor R, vf%02d%s' % (is_, 'xyzw'[fsf]),
                 (26, 2): 'xtop vi%02d' % it, (26, 3): 'xitop vi%02d' % it, (27, 0): 'xgkick vi%02d' % is_,
                 (28, 0): 'esadd P, vf%02d' % is_, (28, 1): 'ersadd P, vf%02d' % is_, (28, 2): 'eleng P, vf%02d' % is_, (28, 3): 'erleng P, vf%02d' % is_,
                 (29, 0): 'eatanxy P, vf%02d' % is_, (29, 1): 'eatanxz P, vf%02d' % is_, (29, 2): 'esum P, vf%02d' % is_, (30, 0): 'esqrt P, vf%02d%s' % (is_, 'xyzw'[fsf]),
                 (30, 1): 'ersqrt P, vf%02d%s' % (is_, 'xyzw'[fsf]), (30, 2): 'ercpr P, vf%02d%s' % (is_, 'xyzw'[fsf]), (30, 3): 'waitp', (31, 0): 'esin P, vf%02d%s' % (is_, 'xyzw'[fsf]),
                 (31, 1): 'eatan P, vf%02d%s' % (is_, 'xyzw'[fsf]), (31, 2): 'eexp P, vf%02d%s' % (is_, 'xyzw'[fsf]), (32, 0): 'mfp.%s vf%02d, P' % (dest(d), it), (18, 0): 'nop' if False else 'ilw?'}
            if (idx, sel) in t: return t[(idx, sel)]
            if idx == 18 and sel == 0: return 'nop'
    return 'lower? 0x%08x' % w
def main():
    elf, a, b = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
    syms = {}
    for ln in open(sys.argv[4] if len(sys.argv) > 4 else '/tmp/all.dis'):
        m = re.match(r'^([0-9a-f]{8}) <(.+)>:', ln)
        if m: syms[int(m.group(1), 16)] = m.group(2)
    data = open(elf, 'rb').read()
    off = lambda v: v - 0x100000 + 0x80
    for v in range(a, b, 8):
        lo, hi = struct.unpack_from('<II', data, off(v))
        if v in syms: print('%s:' % syms[v])
        print('  %06x  %08x %08x   %-40s | %s' % (v, hi, lo, upper(hi), lower(lo, hi)))
if __name__ == '__main__': main()
