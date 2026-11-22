"""Static recompiler: R5900 (integer + COP1 single) functions of SLES_503.83 (+ a stage code image) -> C.  Copied from the TT port (model/recomp.py).  Output uses a global register file and an address-translating
memory accessor, so the generated code is a literal transcription of the original machine code.
Usage: python3 recomp.py out.h name=0xstart-0xend ..."""
import struct, sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vuemit, idioms
# MGS2: guest code = the ELF (0x100000, file offset 0x80) plus, optionally, one stage code image (STAGE=<stage>.bin, loaded at its header word 0)
D = open(__import__('os').environ.get('SLUS', 'SLES_503.83'), 'rb').read()
SD = open(os.environ['STAGE'], 'rb').read() if os.environ.get('STAGE') else b''
SBASE = struct.unpack_from('<I', SD, 0)[0] if SD else 0
def W(a):
    if SD and SBASE <= a < SBASE + len(SD): return struct.unpack_from('<I', SD, a - SBASE)[0]
    return struct.unpack_from('<I', D, a - 0x100000 + 0x80)[0]
def s16(x): return x - 0x10000 if x & 0x8000 else x
externs = set(); JT = {}; TC = set()
BBT_FN = {int(x, 16) for x in os.environ.get('RECOMP_BBT', '').split(',') if x}   # debug: per-instruction trace hook BBT(addr) in these functions
def gen_func(name, start, end, known):
    lines = []; targets = set()
    # pass 1: collect branch targets
    for a in range(start, end, 4):
        ins = W(a); op = ins >> 26; imm = ins & 0xffff
        if (op == 1 and (ins >> 16) & 31 < 24) or op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op in (0x10, 0x11) and (ins >> 21) & 31 == 8):
            targets.add(a + 4 + (s16(imm) << 2))
        if op == 0 and (ins & 63) == 8 and a in JT:
            for k in range(JT[a][1]): targets.add(W(JT[a][0] + 4 * k))
        if op == 2: targets.add(((a & 0xf0000000) | ((ins & 0x3ffffff) << 2)))
    def ex(a, delay=False):
        """C statement(s) for the (non-branch) instruction at a"""
        ins = W(a); op = ins >> 26; rs = (ins >> 21) & 31; rt = (ins >> 16) & 31; rd = (ins >> 11) & 31; sa = (ins >> 6) & 31; fn = ins & 63
        imm = ins & 0xffff; simm = s16(imm)
        def SET(n, e): return '' if n == 0 else 'R[%d] = %s;' % (n, e)
        def S32(e): return '(int64_t)(int32_t)(%s)' % e
        if ins == 0: return ''
        if op == 0 and fn == 0x0c: return 'SYSCALL(0x%x);' % a
        if op == 0 and fn == 0x0d: return 'BREAK(0x%x);' % a
        if op == 0 and fn == 0x11: return 'HI = R[%d];' % rs
        if op == 0 and fn == 0x13: return 'LO = R[%d];' % rs
        if op == 0 and fn == 0x28: return SET(rd, '(int64_t)SAR')                       # mfsa
        if op == 0 and fn == 0x29: return 'SAR = (uint32_t)R[%d] & 127;' % rs           # mtsa
        if op == 0:
            if fn == 0: return SET(rd, S32('(uint32_t)R[%d] << %d' % (rt, sa)))
            if fn == 2: return SET(rd, S32('(uint32_t)R[%d] >> %d' % (rt, sa)))
            if fn == 3: return SET(rd, S32('(int32_t)R[%d] >> %d' % (rt, sa)))
            if fn == 4: return SET(rd, S32('(uint32_t)R[%d] << (R[%d] & 31)' % (rt, rs)))
            if fn == 6: return SET(rd, S32('(uint32_t)R[%d] >> (R[%d] & 31)' % (rt, rs)))
            if fn == 7: return SET(rd, S32('(int32_t)R[%d] >> (R[%d] & 31)' % (rt, rs)))
            if fn == 10: return 'if (R[%d] == 0) %s' % (rt, SET(rd, 'R[%d]' % rs))
            if fn == 11: return 'if (R[%d] != 0) %s' % (rt, SET(rd, 'R[%d]' % rs))
            if fn == 0xf: return ";"                                                 # sync
            if fn == 0x10: return SET(rd, 'HI')
            if fn == 0x12: return SET(rd, 'LO')
            if fn == 0x18: return '{ int64_t p = (int64_t)(int32_t)R[%d] * (int32_t)R[%d]; LO = (int32_t)p; HI = (int32_t)(p >> 32); %s }' % (rs, rt, SET(rd, 'LO') if rd else '')
            if fn == 0x19: return '{ uint64_t p = (uint64_t)(uint32_t)R[%d] * (uint32_t)R[%d]; LO = (int32_t)p; HI = (int32_t)(p >> 32); %s }' % (rs, rt, SET(rd, 'LO') if rd else '')
            if fn == 0x1a: return '{ int32_t a = (int32_t)R[%d], b = (int32_t)R[%d]; if (b == 0) { LO = a >= 0 ? -1 : 1; HI = a; } else { LO = a / b; HI = a %% b; } }' % (rs, rt)
            if fn == 0x1b: return '{ uint32_t a = (uint32_t)R[%d], b = (uint32_t)R[%d]; if (b == 0) { LO = -1; HI = (int32_t)a; } else { LO = (int32_t)(a / b); HI = (int32_t)(a %% b); } }' % (rs, rt)
            if fn in (0x20, 0x21): return SET(rd, S32('R[%d] + R[%d]' % (rs, rt)))
            if fn in (0x22, 0x23): return SET(rd, S32('R[%d] - R[%d]' % (rs, rt)))
            if fn == 0x14: return SET(rd, '(int64_t)((uint64_t)R[%d] << (R[%d] & 63))' % (rt, rs))
            if fn == 0x16: return SET(rd, '(int64_t)((uint64_t)R[%d] >> (R[%d] & 63))' % (rt, rs))
            if fn == 0x17: return SET(rd, 'R[%d] >> (R[%d] & 63)' % (rt, rs))
            if fn == 0x24: return SET(rd, 'R[%d] & R[%d]' % (rs, rt))
            if fn == 0x25: return SET(rd, 'R[%d] | R[%d]' % (rs, rt))
            if fn == 0x26: return SET(rd, 'R[%d] ^ R[%d]' % (rs, rt))
            if fn == 0x27: return SET(rd, '~(R[%d] | R[%d])' % (rs, rt))
            if fn == 0x2a: return SET(rd, '(R[%d] < R[%d])' % (rs, rt))
            if fn == 0x2b: return SET(rd, '((uint64_t)R[%d] < (uint64_t)R[%d])' % (rs, rt))
            if fn in (0x2c, 0x2d): return SET(rd, 'R[%d] + R[%d]' % (rs, rt))
            if fn in (0x2e, 0x2f): return SET(rd, 'R[%d] - R[%d]' % (rs, rt))
            if fn == 0x38: return SET(rd, '(int64_t)((uint64_t)R[%d] << %d)' % (rt, sa))
            if fn == 0x3a: return SET(rd, '(int64_t)((uint64_t)R[%d] >> %d)' % (rt, sa))
            if fn == 0x3b: return SET(rd, 'R[%d] >> %d' % (rt, sa))
            if fn == 0x3c: return SET(rd, '(int64_t)((uint64_t)R[%d] << %d)' % (rt, sa + 32))
            if fn == 0x3e: return SET(rd, '(int64_t)((uint64_t)R[%d] >> %d)' % (rt, sa + 32))
            if fn == 0x3f: return SET(rd, 'R[%d] >> %d' % (rt, sa + 32))
            raise RuntimeError('special %x at %x' % (fn, a))
        if op == 0x1c and fn == 0x09:                   # MMI2 (128-bit GPR: R = low 64, RH = high 64)
            if sa == 0x0e: return '{ int64_t lo_ = R[%d], hi_ = R[%d]; if (%d) { R[%d] = lo_; RH[%d] = hi_; } }' % (rt, rs, rd, rd, rd)        # PCPYLD
            if sa == 0x12: return '{ int64_t lo_ = R[%d] & R[%d], hi_ = RH[%d] & RH[%d]; if (%d) { R[%d] = lo_; RH[%d] = hi_; } }' % (rs, rt, rs, rt, rd, rd, rd)   # PAND
        if op == 0x1c and fn == 0x29:                   # MMI3
            if sa == 0x0e: return '{ int64_t lo_ = RH[%d], hi_ = RH[%d]; if (%d) { R[%d] = lo_; RH[%d] = hi_; } }' % (rs, rt, rd, rd, rd)      # PCPYUD
            if sa == 0x12: return '{ int64_t lo_ = R[%d] | R[%d], hi_ = RH[%d] | RH[%d]; if (%d) { R[%d] = lo_; RH[%d] = hi_; } }' % (rs, rt, rs, rt, rd, rd, rd)   # POR
            if sa == 0x13: return '{ int64_t lo_ = ~(R[%d] | R[%d]), hi_ = ~(RH[%d] | RH[%d]); if (%d) { R[%d] = lo_; RH[%d] = hi_; } }' % (rs, rt, rs, rt, rd, rd, rd)   # PNOR
        if op == 0x1c and fn == 0x10: return SET(rd, 'HI1')
        if op == 0x1c and fn == 0x12: return SET(rd, 'LO1')
        if op == 0x1c and fn == 0x1a: return '{ int32_t a = (int32_t)R[%d], b = (int32_t)R[%d]; if (b == 0) { LO1 = a >= 0 ? -1 : 1; HI1 = a; } else { LO1 = a / b; HI1 = a %% b; } }' % (rs, rt)
        if op == 0x1c and fn == 0x1b: return '{ uint32_t a = (uint32_t)R[%d], b = (uint32_t)R[%d]; if (b == 0) { LO1 = -1; HI1 = (int32_t)a; } else { LO1 = (int32_t)(a / b); HI1 = (int32_t)(a %% b); } }' % (rs, rt)
        if op == 0x1c and fn == 0x18: return '{ int64_t p = (int64_t)(int32_t)R[%d] * (int32_t)R[%d]; LO1 = (int32_t)p; HI1 = (int32_t)(p >> 32); %s }' % (rs, rt, SET(rd, 'LO1') if rd else '')   # MULT1
        if op == 0x1c and fn == 0x19: return '{ uint64_t p = (uint64_t)(uint32_t)R[%d] * (uint32_t)R[%d]; LO1 = (int32_t)p; HI1 = (int32_t)(p >> 32); %s }' % (rs, rt, SET(rd, 'LO1') if rd else '')   # MULTU1
        if op == 0x1c and fn in (0, 1):   # MADD / MADDU: HI:LO += rs * rt (32 x 32 -> 64), rd = LO
            mul = '(int64_t)(int32_t)R[%d] * (int32_t)R[%d]' % (rs, rt) if fn == 0 else '(int64_t)((uint64_t)(uint32_t)R[%d] * (uint32_t)R[%d])' % (rs, rt)
            return '{ int64_t p = (int64_t)(((uint64_t)(uint32_t)HI << 32) | (uint32_t)LO) + %s; LO = (int32_t)p; HI = (int32_t)(p >> 32); %s }' % (mul, SET(rd, 'LO') if rd else '')
        if op == 1 and rt == 24: return 'SAR = (((uint32_t)R[%d] ^ %d) & 15) * 8;' % (rs, imm)    # mtsab
        if op == 1 and rt == 25: return 'SAR = (((uint32_t)R[%d] ^ %d) & 7) * 16;' % (rs, imm)    # mtsah
        if op in (8, 9): return SET(rt, S32('R[%d] + %d' % (rs, simm)))
        if op == 10: return SET(rt, '(R[%d] < %d)' % (rs, simm))
        if op == 11: return SET(rt, '((uint64_t)R[%d] < (uint64_t)(int64_t)%d)' % (rs, simm))
        if op == 12: return SET(rt, 'R[%d] & %d' % (rs, imm))
        if op == 13: return SET(rt, 'R[%d] | %d' % (rs, imm))
        if op == 14: return SET(rt, 'R[%d] ^ %d' % (rs, imm))
        if op == 15: return SET(rt, '%d' % (s16(imm) << 16))
        if op in (0x18, 0x19): return SET(rt, 'R[%d] + %d' % (rs, simm))
        A = '(uint32_t)(R[%d] + %d)' % (rs, simm)
        # MGS2 additions: syscall / break / mthi / mtlo, COP0, unaligned loads and stores, the MMI ops the game uses (runtime: mgs2/runtime/mmi.h)
        if op == 0x10 and rs == 0: return SET(rt, '(int64_t)(int32_t)COP0R(%d, %d)' % (rd, ins & 7))
        if op == 0x10 and rs == 4: return 'COP0W(%d, %d, (uint32_t)R[%d]);' % (rd, ins & 7, rt)
        if op == 0x10 and rs == 16: return 'COP0OP(0x%x, 0x%x);' % (fn, a)          # ei / di / eret / tlb ops
        if op in (0x22, 0x26, 0x1a, 0x1b): return SET(rt, '%s(R[%d], %s)' % ({0x22: 'GLWL', 0x26: 'GLWR', 0x1a: 'GLDL', 0x1b: 'GLDR'}[op], rt, A))
        if op in (0x2a, 0x2e, 0x2c, 0x2d): return '%s(R[%d], %s);' % ({0x2a: 'GSWL', 0x2e: 'GSWR', 0x2c: 'GSDL', 0x2d: 'GSDR'}[op], rt, A)
        MMI = {(4, 0): 'PLZCW', (8, 0x01): 'PSUBW', (8, 0x09): 'PSUBB', (8, 0x12): 'PEXTLW', (8, 0x13): 'PPACW', (8, 0x16): 'PEXTLH',
               (8, 0x17): 'PPACH', (8, 0x1a): 'PEXTLB', (8, 0x04): 'PADDH', (8, 0x1b): 'PPACB', (8, 0x14): 'PADDSH', (8, 0x07): 'PMAXH', (8, 0x06): 'PCGTH', (0x28, 0x1b): 'QFSRV', (0x30, 0x00): 'PMFHL_LW', (0x30, 0x01): 'PMFHL_UW', (0x28, 0x14): 'PADDUH', (0x28, 0x18): 'PADDUB', (0x28, 0x07): 'PMINH', (9, 0x1c): 'PMULTH', (9, 0x1d): 'PDIVBW', (9, 0x08): 'PMFHI', (9, 0x09): 'PMFLO', (0x28, 0x12): 'PEXTUW', (0x28, 0x16): 'PEXTUH', (0x28, 0x1a): 'PEXTUB', (9, 0x1a): 'PEXEH', (9, 0x13): 'PXOR', (9, 0x1f): 'PROT3W', (0x29, 0x1b): 'PCPYH'}
        if op == 0x1c and (fn, sa) in MMI and (rd or MMI[(fn, sa)] in ('PDIVBW', 'PMULTH')): return 'M_%s(%d, %d, %d);' % (MMI[(fn, sa)], rd, rs, rt)
        if op == 0x1c and fn in (0x34, 0x36, 0x37, 0x3c, 0x3e, 0x3f) and rd:       # PSLLH PSRLH PSRAH PSLLW PSRLW PSRAW (sa = shift)
            return 'M_%s(%d, %d, %d);' % ({0x34: 'PSLLH', 0x36: 'PSRLH', 0x37: 'PSRAH', 0x3c: 'PSLLW', 0x3e: 'PSRLW', 0x3f: 'PSRAW'}[fn], rd, rt, sa)
        hwbase = None                                   # EE hardware registers (timers, DMA): `lui $1, 0x1000/0x1001` (within 3 instructions) + lw/sw via $1
        if rs == 1 and op in (0x23, 0x27, 0x2b):
            for k in range(1, 4):
                pw = W(a - 4 * k)
                if (pw >> 16) == 0x3c01: hwbase = pw & 0xffff; break
                po = pw >> 26
                if (po == 0 and ((pw >> 11) & 31) == 1) or (po not in (0, 0x2b, 0x28, 0x29, 0x39, 0x3f, 0x1f) and ((pw >> 16) & 31) == 1): break
        if hwbase in (0x1000, 0x1001):
            HA = (hwbase << 16) + simm
            if op == 0x2b: return 'HWW32(0x%x, (uint32_t)R[%d]);' % (HA, rt)
            return SET(rt, '%sHWR32(0x%x)' % ('(int32_t)' if op == 0x23 else '', HA))
        if op == 0x20: return SET(rt, '(int8_t)RD8(%s)' % A)
        if op == 0x21: return SET(rt, '(int16_t)RD16(%s)' % A)
        if op == 0x23: return SET(rt, '(int32_t)RD32(%s)' % A)
        if op == 0x24: return SET(rt, 'RD8(%s)' % A)
        if op == 0x25: return SET(rt, 'RD16(%s)' % A)
        if op == 0x27: return SET(rt, 'RD32(%s)' % A)
        if op == 0x28: return 'WR8(%s, R[%d]);' % (A, rt)
        if op == 0x29: return 'WR16(%s, R[%d]);' % (A, rt)
        if op == 0x2b: return 'WR32(%s, R[%d]);' % (A, rt)
        if op == 0x37: return SET(rt, '(int64_t)RD64(%s)' % A)
        if op == 0x3f: return 'WR64(%s, R[%d]);' % (A, rt)
        if op == 0x1f: return 'WR64((%s) & ~15u, R[%d]); WR64(((%s) & ~15u) + 8, RH[%d]);' % (A, rt, A, rt)   # SQ: 128 bits
        if op == 0x1e: return '{ uint32_t qa_ = (%s) & ~15u; if (%d) { R[%d] = (int64_t)RD64(qa_); RH[%d] = (int64_t)RD64(qa_ + 8); } }' % (A, rt, rt, rt)   # LQ: 128 bits
        if op == 0x31: return 'F[%d] = RDF(%s);' % (rt, A)
        if op == 0x39: return 'WRF(%s, F[%d]);' % (A, rt)
        if op == 0x2f or op == 0x33: return ''
        if op == 0x11:
            if rs == 0: return SET(rt, S32('F2I(F[%d])' % rd))
            if rs == 4: return 'F[%d] = I2F((uint32_t)R[%d]);' % (rd, rt)
            if rs == 2 or rs == 6: return ''
            if rs == 16:
                d = sa; a_ = 'F[%d]' % rd; b_ = 'F[%d]' % rt
                m = {0: 'F[%d] = %s + %s;' % (d, a_, b_), 1: 'F[%d] = %s - %s;' % (d, a_, b_), 2: 'F[%d] = %s * %s;' % (d, a_, b_),
                     3: 'F[%d] = FDIV(%s, %s);' % (d, a_, b_), 4: 'F[%d] = sqrtf(fabsf(%s));' % (d, b_), 5: 'F[%d] = fabsf(%s);' % (d, a_),
                     6: 'F[%d] = %s;' % (d, a_), 7: 'F[%d] = -%s;' % (d, a_),
                     0x16: 'F[%d] = FDIV(%s, sqrtf(fabsf(%s)));' % (d, a_, b_),
                     0x18: 'ACC = %s + %s;' % (a_, b_), 0x19: 'ACC = %s - %s;' % (a_, b_), 0x1a: 'ACC = %s * %s;' % (a_, b_),
                     0x1c: 'F[%d] = ACC + %s * %s;' % (d, a_, b_), 0x1d: 'F[%d] = ACC - %s * %s;' % (d, a_, b_),
                     0x1e: 'ACC = ACC + %s * %s;' % (a_, b_), 0x1f: 'ACC = ACC - %s * %s;' % (a_, b_),
                     0x28: 'F[%d] = fmaxf(%s, %s);' % (d, a_, b_), 0x29: 'F[%d] = fminf(%s, %s);' % (d, a_, b_),
                     0x24: 'F[%d] = I2F((uint32_t)F2INT(%s));' % (d, a_),
                     0x30: 'CC = 0;', 0x31: 'CC = 0;', 0x32: 'CC = %s == %s;' % (a_, b_), 0x33: 'CC = %s == %s;' % (a_, b_),
                     0x34: 'CC = %s < %s;' % (a_, b_), 0x35: 'CC = %s < %s;' % (a_, b_), 0x36: 'CC = %s <= %s;' % (a_, b_), 0x37: 'CC = %s <= %s;' % (a_, b_)}
                if fn in m: return m[fn]
                raise RuntimeError('cop1 S fn %x at %x' % (fn, a))
            if rs == 20 and fn == 0x20: return 'F[%d] = (float)(int32_t)F2I(F[%d]);' % (sa, rd)
        if op == 0x12:
            if rs == 1: return 'VQMFC2(%d, %d);' % (rt, rd)
            if rs == 5: return 'VQMTC2(%d, %d);' % (rt, rd)
            if rs == 2: return SET(rt, '(int64_t)(int32_t)VCFC2(%d)' % rd)
            if rs == 6: return 'VCTC2(%d, (uint32_t)R[%d]);' % (rd, rt)
            if rs >= 16: return 'VUM(0x%08x);' % ins
        if op == 0x36: return 'VLQC2(%d, (%s) & ~15u);' % (rt, A)
        if op == 0x3e: return 'VSQC2(%d, (%s) & ~15u);' % (rt, A)
        raise RuntimeError('op %x rs %d fn %x sa %x at %x' % (op, rs, fn, sa, a))
    def tgt(t, in_func=True):
        return 'goto L%x;' % t
    def isbr(ins):
        o = ins >> 26; f = ins & 63
        return (o == 1 and (ins >> 16) & 31 < 24) or o in (2, 3, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (o == 0 and f in (8, 9)) or (o in (0x10, 0x11) and (ins >> 21) & 31 == 8)   # REGIMM rt 24/25 = mtsab/mtsah
    dslots = set(a + 4 for a in range(start, end, 4) if isbr(W(a)))
    for a in dslots:
        if a in targets: targets.add(a + 4)
    for a in range(start, end, 4):
        ins = W(a); op = ins >> 26; rs = (ins >> 21) & 31; rt = (ins >> 16) & 31; imm = ins & 0xffff; simm = s16(imm); fn = ins & 63
        if a in targets: lines.append('L%x:;' % a)
        if a in dslots:
            if a in targets: lines.append('/*%x*/ %s goto L%x;' % (a, ex(a), a + 4))
            continue
        is_br = False
        def cond_branch(cond, likely=False, link=False):
            t = a + 4 + (simm << 2); d = ex(a + 4)
            if likely: return '{ if (%s) { %s goto L%x; } }' % (cond, d, t)       # delay slot only when taken
            return '{ int c_ = (%s); %s if (c_) goto L%x; }' % (cond, d, t)
        s = None
        if op == 0 and fn == 8 and rs == 31: s = '{ %s return; }' % ex(a + 4); is_br = True
        elif op == 0 and fn == 8 and a in JT:
            s = '{ uint32_t t_ = (uint32_t)R[%d]; %s switch (t_) { %s default: break; } }' % (rs, ex(a + 4), ' '.join('case 0x%x: goto L%x;' % (v, v) for v in sorted(set(W(JT[a][0] + 4 * k) for k in range(JT[a][1]))))); is_br = True
        elif op == 0 and fn == 8 and a in TC: s = '{ uint32_t t_ = (uint32_t)R[%d]; %s JALR(t_); return; }' % (rs, ex(a + 4)); is_br = True   # tail call through a pointer (vtable)
        elif op == 0 and fn == 8: raise RuntimeError('jr non-ra at %x' % a)
        elif op == 0 and fn == 9: s = '{ int64_t t_ = R[%d]; R[%d] = 0x%x; %s JALR((uint32_t)t_); }' % (rs, ((ins >> 11) & 31) or 31, a + 8, ex(a + 4)); is_br = True
        elif op == 2: s = '{ %s goto L%x; }' % (ex(a + 4), (a & 0xf0000000) | ((ins & 0x3ffffff) << 2)); is_br = True
        elif op == 3:
            t = (a & 0xf0000000) | ((ins & 0x3ffffff) << 2)
            if t not in known: externs.add(t)
            s = '{ R[31] = 0x%x; %s %s(); }' % (a + 8, ex(a + 4), known.get(t, 'X_%x' % t)); is_br = True
        elif op == 4: s = cond_branch('R[%d] == R[%d]' % (rs, rt)); is_br = True
        elif op == 5: s = cond_branch('R[%d] != R[%d]' % (rs, rt)); is_br = True
        elif op == 6: s = cond_branch('R[%d] <= 0' % rs); is_br = True
        elif op == 7: s = cond_branch('R[%d] > 0' % rs); is_br = True
        elif op == 1 and rt == 0: s = cond_branch('R[%d] < 0' % rs); is_br = True
        elif op == 1 and rt == 1: s = cond_branch('R[%d] >= 0' % rs); is_br = True
        elif op == 1 and rt == 2: s = cond_branch('R[%d] < 0' % rs, True); is_br = True          # bltzl
        elif op == 0x10 and rs == 8: s = cond_branch('1' if rt & 1 else '0', bool(rt & 2)); is_br = True   # bc0t / bc0f(l): CPCOND0 = DMA done (the host DMA completes at once)
        elif op == 1 and rt == 3: s = cond_branch('R[%d] >= 0' % rs, True); is_br = True         # bgezl
        elif op == 0x16: s = cond_branch('R[%d] <= 0' % rs, True); is_br = True                  # blezl
        elif op == 0x17: s = cond_branch('R[%d] > 0' % rs, True); is_br = True                   # bgtzl
        elif op == 0x14: s = cond_branch('R[%d] == R[%d]' % (rs, rt), True); is_br = True
        elif op == 0x15: s = cond_branch('R[%d] != R[%d]' % (rs, rt), True); is_br = True
        elif op == 0x16: s = cond_branch('R[%d] <= 0' % rs, True); is_br = True
        elif op == 0x17: s = cond_branch('R[%d] > 0' % rs, True); is_br = True
        elif op == 0x11 and rs == 8:
            c = {0: '!CC', 1: 'CC', 2: '!CC', 3: 'CC'}[rt & 3]; s = cond_branch(c, (rt & 3) >= 2); is_br = True
        else: s = ex(a)
        lines.append('/*%x*/ %s%s' % (a, ('BBT(0x%x); ' % a) if start in BBT_FN else '', s))
    if not os.environ.get('NOVUSPEC'): lines = vu_post(lines)
    body = '\n'.join(lines)
    idi = idioms.match(W, start, end)   # a library routine the runtime runs natively when it can (idioms.py)
    return 'static void %s(void)\n{\nFN_ENTER(0x%x);%s\n%s\n}\n' % (name, start, (' GEN_IDIOM_%s();' % idi) if idi else '', body)
VUM_RE = re.compile(r'^(/\*[0-9a-f]+\*/ )VUM\(0x([0-9a-f]{8})\);$')
BLANK_RE = re.compile(r'^/\*[0-9a-f]+\*/ $')
def vu_post(lines):
    """COP2 macro ops of one function -> specialized C (model/vuemit.py).
    MAC / status flags: computed only by the last flag-writing op before a cfc2 of MAC / status (backward liveness over the lines; a label
    or branch is treated conservatively: the flags are live at a label if a cfc2 can follow it before the next flag write).
    VU float form: known per (reg, comp) along straight-line code (results of the specialized ops); forgotten at labels and calls, set raw
    by lqc2 / qmtc2."""
    n = len(lines)
    def flag_writer(i):
        m = VUM_RE.match(lines[i]); return bool(m) and vuemit.writes_flags_macro(int(m.group(2), 16))
    reads = lambda i: 'VCFC2(17)' in lines[i] or 'VCFC2(16)' in lines[i]
    other_vum = lambda i: ('VUM(' in lines[i] or 'VUM0(' in lines[i]) and not VUM_RE.match(lines[i])
    # live at each label: scan forward
    live_lab = {}
    for i, ln in enumerate(lines):
        if ln.startswith('L') and ln.endswith(':;'):
            lv = False
            for j in range(i + 1, n):
                if reads(j) or other_vum(j): lv = True; break
                if flag_writer(j): break
            live_lab[ln[1:-2]] = lv
    live = False; fl = {}
    for i in range(n - 1, -1, -1):
        ln = lines[i]
        for t in re.findall(r'goto L([0-9a-f]+);', ln):
            if live_lab.get(t, True): live = True
        if 'return;' in ln and not ln.startswith('L'): pass
        if reads(i) or other_vum(i): live = True
        if VUM_RE.match(ln):
            fl[i] = live
            if flag_writer(i): live = False
    norm = set(); vn = set(); out = []   # vn: (reg, comp) whose VU float form is in the shadow VN (vuemit.mk_reader)
    def vdec(j):   # the VFPU group member on line j (vuemit.vfpu_dec), 'nop', or None
        m = VUM_RE.match(lines[j])
        return vuemit.vfpu_dec(int(m.group(2), 16)) if m and not fl.get(j) else None
    i = -1
    while i + 1 < n:
        i += 1; ln = lines[i]
        if ln.startswith('L') and ln.endswith(':;'): norm = set(); vn = set(); out.append(ln); continue
        m = VUM_RE.match(ln)
        if m and vuemit.VFPU_GROUP and isinstance(vdec(i), dict):
            # a run of VFPU group members (address-only lines and no-effect ops in between): one checked VFPU block (vuemit.vfpu_group)
            run = []; j = i
            while j < n:
                d = vdec(j)
                if isinstance(d, dict): run.append((j, d))
                elif d != 'nop' and not BLANK_RE.match(lines[j]): break
                j += 1
            while run:
                nf, vf = set(norm), set(vn); fb = []
                for _, d in run:
                    wr_ = []; c = vuemit.macro_c(int(VUM_RE.match(lines[_]).group(2), 16), 0, nf, wr_, vf)
                    fb.append(c.replace('vu_in(', 'vu_cl('))   # vu_cl = vu_in, spelled so tools/vu_hoist.py leaves the cold path alone
                    for r, cc, nm in wr_:
                        vf.discard((r, cc))
                        if nm: nf.add((r, cc))
                        else: nf.discard((r, cc))
                g = vuemit.vfpu_group([d for _, d in run], ' '.join(fb))
                if g: break
                run = run[:len(run) // 2]
            stmt, wl = g; last = run[-1][0]
            for j in range(i, last + 1):
                a_ = re.match(r'^(/\*[0-9a-f]+\*/ )', lines[j]).group(1)
                out.append(a_ + stmt if j == i else a_)
            for k, cs_ in wl.items():
                for cc in cs_: r = -1 if k == 'A' else k; norm.add((r, cc)); vn.discard((r, cc))
            i = last; continue
        if m:
            w = int(m.group(2), 16); wr = []
            c = vuemit.macro_c(w, 1 if fl.get(i) else 0, norm, wr, vn)
            if c is None: out.append(ln if fl.get(i) else ln.replace('VUM(', 'VUM0(')); norm = set(); vn = set(); continue
            for r, cc, nm in wr:
                vn.discard((r, cc))
                if nm: norm.add((r, cc))
                else: norm.discard((r, cc))
            out.append(m.group(1) + c); continue
        for r in re.findall(r'VLQC2\((\d+),', ln): norm = {x for x in norm if x[0] != int(r)}; vn = {x for x in vn if x[0] != int(r)}
        for r in re.findall(r'VQMTC2\(\d+, (\d+)\)', ln): norm = {x for x in norm if x[0] != int(r)}; vn = {x for x in vn if x[0] != int(r)}
        if re.search(r'\b(P[0-9a-f]+|X_[0-9a-f]+|O[0-9a-f]+|JALR|VUM0?|VCTC2|VCALLMS)\(', ln): norm = set(); vn = set()
        out.append(ln)
    return out
if __name__ == '__main__':
    out = sys.argv[1]; specs = []
    for sp in sys.argv[2:]:
        if sp.startswith('jt='): x = sp[3:].split(':'); JT[int(x[0], 16)] = (int(x[1], 16), int(x[2])); continue
        if sp.startswith('tc='): TC.add(int(sp[3:], 16)); continue
        n, r = sp.split('='); a, b = r.split('-'); specs.append((n, int(a, 16), int(b, 16)))
    known = {a: n for n, a, b in specs}; body = ''
    protos = ''.join('static void %s(void);\n' % n for n, a, b in specs)
    for n, a, b in specs: body += gen_func(n, a, b, known)
    protos += 'static void JALR(uint32_t);\n'
    body += 'static void ' + os.environ.get('GEN_INIT', 'gen_init') + '(void)   /* jump tables of the original switch statements */\n{\n' + ''.join('  WR32(0x%x, 0x%x);\n' % (b_ + 4 * k, W(b_ + 4 * k)) for (b_, n_) in sorted(set(JT.values())) for k in range(n_)) + '}\n'
    protos += ''.join('static void X_%x(void);\n' % t for t in sorted(externs))
    open(out, 'w').write(protos + body)
    print('wrote', out, len(body))
