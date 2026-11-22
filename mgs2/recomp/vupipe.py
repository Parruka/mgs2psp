#!/usr/bin/env python3
"""vupipe.py: AOT compiler for VU0 micro programs with the exact timing of the vu0.h interpreter (vu_run's pipeline model).
usage: python3 vupipe.py <dump dir> <out gen_vu.h> <hash>:<entry>[,<entry>...] ...
Input: the micro memory images and entries a -DVU_DUMP build collects (<dir>/<hash>.bin).  Output: a header for -DMGS2_GEN_VU (vu0.h
includes it in the runtime unit; vu_run calls vuc_dispatch for a compiled image + entry, the interpreter runs everything else).

Why not vurecomp.py (TT): the MGS2 interpreter models the VU pipeline (an upper result and its MAC / status / clip flags land 4 cycles after
issue, DIV / SQRT Q after 7, RSQRT after 13, a read of a VF register in flight stalls) and the game's programs depend on it (the slerp
program 33268338 / 6e214876 tests at pc 11 the MAC flags of pc 7 while pc 8 and 9 are in flight).  The timing of one call depends only on
its path through the branches, so this compiler runs the interpreter's model at compile time along every path and emits, per pair, the
interpreter's operations in its order with the flag commits placed at their cycle:
- per pair: the commits due at its (stalled) cycle in queue order; the upper op into temporaries (ACC written at once, as vu_upper does);
  its MAC value / clip bits into a pending variable; the lower op on the old registers (the I bit loads I); then the upper results;
- a conditional branch forks after its delay slot; paths join where the whole timing state (relative to the cycle), the pending queue's
  shape and the compile-time knowledge below are equal (pending values are renamed to the join's variables);
- registers live in C locals for the call: VF as floats, a component known to hold a VU-form value (an FMAC / DIV result: vin is the
  identity on it) is read without vu_in, any other is clamped once per path into a copy c<r><c> (the raw bits stay in f<r><c> for MOVE /
  SQ / the write-back); ACC, VI, Q, I, CLIP, MAC, status likewise, written back at the end;
- -DVP_FAST (the header's fast form): the result clamps (vout / vu_in on FMAC, DIV, RSQRT results) become the identity and MAC keeps only
  its Z / S bits, valid only in the FPU mode the reference and the PSP run (round toward zero, denormal results flushed); vuc_dispatch
  checks that mode on every call;
- status flags: kept exact with -DVP_EXACT_FLAGS (VU_CHECK_FLAGS then compares them too).  Without it the MAC / status bits of the status
  register are not maintained (the MAC register is): no compiled image reads status (FSEQ / FSAND / FSOR) and the EE never reads COP2
  control register 16 (checked over gen_elf.h and the boot-path stages); the compiler then drops the MAC computations no FMAND / FMEQ /
  FMOR and no final MAC value reads (C dead-code elimination of the pending values).
JR / JALR: the target must be known at compile time: the VI registers some JR / JALR reads are tracked as constants along each path (a
BAL / JALR link, an IADDIU / ISUBIU / IADDI from vi00; any other write forgets the value), and that knowledge is part of the join key, so a
subroutine is compiled once per return address.
Refuses (the image stays on the interpreter): a JR / JALR whose target is not known, an op the interpreter counts as unsupported, a branch in a delay slot of a taken
conditional branch it cannot place, more than MAXPAIRS emitted pairs."""
import sys, os, struct, re
MAXPAIRS = 30000
CN = 'xyzw'

# ---- decode, ported from vu0.h ----
def up_info(w):                          # vu_up_info: (tgt, wm, rd[2], rm[2], flags 1 MAC/status 2 clip)
    op, dest, ft, fs, fd, sel, idx = w & 0x3f, (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 3, (w >> 6) & 31
    bm = (8, 4, 2, 1)
    if op < 0x1c: return fd, dest, [fs, ft], [dest, bm[sel]], int((op >> 2) != 4 and (op >> 2) != 5)
    if op < 0x30:
        mm = op in (0x1d, 0x1f, 0x2b, 0x2f); vec = op >= 0x28
        rd, rm = [fs, 0], [dest, 0]
        if vec: rd[1] = ft; rm[1] = 14 if op == 0x2e else dest
        if op == 0x2e: rm[0] = 14
        return fd, dest, rd, rm, int(not mm)
    if op < 0x3c or idx >= 12: return 0, 0, [0, 0], [0, 0], 0
    if idx in (4, 5) or (idx == 7 and sel == 1): return ft, dest, [fs, 0], [dest, 0], 0
    if idx == 7 and sel == 3: return 0, 0, [fs, ft], [14, 1], 2
    if idx >= 10 and sel == 3: return 0, 0, [0, 0], [0, 0], 0
    rd, rm = [fs, 0], [14 if (idx == 11 and sel == 2) else dest, 0]
    if idx < 4 or idx == 6: rd[1] = ft; rm[1] = bm[sel]
    elif idx >= 10: rd[1] = ft; rm[1] = 14 if (idx == 11 and sel == 2) else dest
    return 0, 0, rd, rm, 1
def low_q(w):
    if (w >> 25) != 0x40 or (w & 0x3f) < 0x3c or ((w >> 6) & 31) != 14: return 0
    return {0: 7, 1: 7, 2: 13}.get(w & 3, 1)
def low_reads(w):
    op, dest, ft, fs, fsf, ftf = w >> 25, (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31, (w >> 21) & 3, (w >> 23) & 3
    r, m = [0, 0], [0, 0]
    if op == 0x01: return [fs, 0], [dest, 0]
    if op != 0x40 or (w & 0x3f) < 0x3c: return r, m
    idx, sel = (w >> 6) & 31, w & 3
    if idx == 12: return [fs, 0], [15, 0]
    if idx == 13 and (sel & 1): return [fs, 0], [dest, 0]
    if idx == 14 and sel != 3:
        if sel != 1: r[0] = fs; m[0] = 8 >> fsf
        r[1] = ft; m[1] = 8 >> ftf; return r, m
    if (idx == 15 and sel == 0) or (idx == 16 and sel >= 2): r[0] = fs; m[0] = 8 >> fsf
    return r, m
def low_writes(w):
    op, dest, ft = w >> 25, (w >> 21) & 15, (w >> 16) & 31
    if op == 0x00: return ft, dest
    if op != 0x40 or (w & 0x3f) < 0x3c: return 0, 0
    idx, sel = (w >> 6) & 31, w & 3
    if idx == 12 or (idx == 13 and not (sel & 1)) or (idx == 15 and sel == 1) or (idx == 16 and sel <= 1) or (idx == 32 and sel == 0): return ft, dest
    return 0, 0
ADD, SUB, MADD, MSUB, MAX, MINI, MUL, OPMSUB, OPMULA, ABS, ITOF, FTOI, CLIP, NOP = range(14)
N1 = [MUL, MAX, MUL, MINI, ADD, MADD, ADD, MADD, SUB, MSUB, SUB, MSUB, ADD, MADD, MUL, MAX, SUB, MSUB, OPMSUB, MINI]
S1 = [4, 5, 5, 5, 4, 4, 5, 5, 4, 4, 5, 5, -1, -1, -1, -1, -1, -1, -1, -1]
N3 = [[ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, MUL, ADD, SUB, ADD, SUB], [ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, ABS, MADD, MSUB, MADD, MSUB],
      [ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, MUL, ADD, SUB, MUL, OPMULA], [ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, CLIP, MADD, MSUB, NOP, NOP]]
S3 = [[0, 0, 0, 0, -1, -1, 0, 4, 4, 4, -1, -1], [1, 1, 1, 1, -1, -1, 1, -1, 4, 4, -1, -1], [2, 2, 2, 2, -1, -1, 2, 5, 5, 5, -1, -1], [3, 3, 3, 3, -1, -1, 3, -1, 5, 5, -1, -1]]
def dec_upper(w):                        # vu_upper_fl's decode; None = returns before touching anything
    op, dest, ft, fs, fd, sel, idx = w & 0x3f, (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 3, (w >> 6) & 31
    toacc, tgt, sh = 0, fd, 0
    if op < 0x1c: opn, src = op >> 2, sel
    elif op < 0x30: opn, src = N1[op - 0x1c], S1[op - 0x1c]
    elif op >= 0x3c:
        if idx >= 12: return None
        opn, src, toacc = N3[sel][idx], S3[sel][idx], 1
        if opn in (ITOF, FTOI, ABS): toacc, tgt, sh = 0, ft, [0, 4, 12, 15][sel]
        if opn == OPMULA: src = -1
    else: return None
    if opn == NOP: return None
    return dict(opn=opn, src=src, toacc=toacc, tgt=tgt, sh=sh, dest=dest, ft=ft, fs=fs, fl=opn not in (MAX, MINI, ABS, ITOF, FTOI))
def vin_bits(u):
    e = u & 0x7f800000
    if not e: return u & 0x80000000
    if e == 0x7f800000: return (u & 0x80000000) | 0x7f7fffff
    return u
def simm11(w): v = w & 0x7ff; return v - 0x800 if v & 0x400 else v
class Refuse(Exception): pass

class St:
    """one point of one path: the interpreter's control and timing state plus compile-time knowledge"""
    def __init__(s):
        s.pc = 0; s.pend = None; s.endnext = 0; s.cyc = 0; s.qready = 0; s.vfready = {}; s.queue = []   # queue: [t, kind, var a, var b]
        s.norm = set(); s.accn = set(); s.I = None; s.gvm = None   # gvm: the C expression of the last g_vmac value (None: unchanged)
        s.vik = {}                                                    # VI register -> known constant (only the registers a JR / JALR reads)
        s.cln = set(); s.acln = set(); s.qn = False; s.cqn = False   # clamped copies valid (c<r><c>, ca<c>, cq_); Q holds a VU-form value
    def copy(s):
        o = St(); o.pc, o.pend, o.endnext, o.cyc, o.qready = s.pc, s.pend, s.endnext, s.cyc, s.qready
        o.vfready = dict(s.vfready); o.queue = [list(e) for e in s.queue]; o.norm = set(s.norm); o.accn = set(s.accn); o.I = s.I; o.gvm = s.gvm; o.vik = dict(s.vik)
        o.cln = set(s.cln); o.acln = set(s.acln); o.qn = s.qn; o.cqn = s.cqn
        return o
    def key(s, generic=False):
        rel = lambda t: max(0, t - s.cyc)
        k = (s.pc, s.pend, s.endnext, rel(s.qready), tuple(sorted((rc, rel(t)) for rc, t in s.vfready.items() if t > s.cyc)),
             tuple((e[1], rel(e[0])) for e in s.queue), s.gvm is not None, tuple(sorted(s.vik.items())))
        if generic: return k + ('generic',)
        return k + (frozenset(s.norm), frozenset(s.accn), s.I, frozenset(s.cln), frozenset(s.acln), s.qn, s.cqn)

class Fn:
    def __init__(s, prog, name):
        s.prog = prog; s.name = name; s.lines = []; s.nvar = 0; s.ncond = 0; s.nlab = 0; s.memo = {}; s.variants = {}
        s.vf_used = set(); s.vf_written = set(); s.vi_used = set(); s.vi_written = set(); s.acc_w = False; s.pairs = 0
        s.work = []; s.jr_regs = set(); s.cur = None; s.cl_used = set()
        s.rzdiv = False; s.hasdiv = False   # rzdiv: DIV / SQRT / RSQRT through vq_divz / vq_sqrtz / vq_rsqz (an entry whose VFPU form divides, see HDR)
    def var(s): s.nvar += 1; return 'p%d' % s.nvar
    def lab(s): s.nlab += 1; return 'L%d' % s.nlab
    def out(s, x): s.lines.append(x)
    # register access
    def fv(s, r, c):
        if r == 0: return '1.0f' if c == 3 else '0.0f'
        s.vf_used.add(r); return 'f%d%s' % (r, CN[c])
    def rd(s, st, r, c):                   # vin(VF[r].u[c]): clamped once per path into c<r><c> (the raw value stays in f<r><c>)
        if r == 0 or (r, c) in st.norm: return s.fv(r, c)
        v = 'c%d%s' % (r, CN[c])
        if (r, c) not in st.cln: s.out('  %s = vu_in(%s);' % (v, s.fv(r, c))); st.cln.add((r, c)); s.cl_used.add(v)
        return v
    def rq(s, st):                         # vin(Q)
        if st.qn: return 'vu_f(q_)'
        if not st.cqn: s.out('  cq_ = vu_in(vu_f(q_));'); st.cqn = True; s.cl_used.add('cq_')
        return 'cq_'
    def racc(s, st, c):                    # vin(ACC[c])
        s.acc_w = True
        if c in st.accn: return 'a%d' % c
        if c not in st.acln: s.out('  ca%d = vu_in(a%d);' % (c, c)); st.acln.add(c); s.cl_used.add('ca%d' % c)
        return 'ca%d' % c
    def raw(s, r, c):                      # VF[r].u[c] (bits)
        if r == 0: return '0x3f800000u' if c == 3 else '0u'
        return 'vu_u(%s)' % s.fv(r, c)
    def wvf(s, r, c, expr, st, norm):
        if not r: return
        s.vf_used.add(r); s.vf_written.add(r); s.out('  %s = %s;' % (s.fv(r, c), expr))
        st.cln.discard((r, c))
        if norm: st.norm.add((r, c))
        else: st.norm.discard((r, c))
    def vi(s, n):
        if not n: return '0u'
        s.vi_used.add(n); return 'vi%d' % n
    def viset(s, n, expr, const=None):
        if n: s.vi_used.add(n); s.vi_written.add(n); s.out('  vi%d = (%s) & 0xffffu;' % (n, expr))
        if n in s.jr_regs:
            if const is None: s.cur.vik.pop(n, None)
            else: s.cur.vik[n] = const & 0xffff
    def mem(s, a): return '((uint32_t *)(VUMEM + ((((%s) + %du) & 0xffu) << 4)))' % (a, 0)
    # ---- the commits of vu_commit(now) ----
    def commit(s, st, now):
        keep = []
        for e in st.queue:
            t, kind, a, b = e
            if t > now: keep.append(e); continue
            if kind == 0: s.out('  mac_ = %s; VP_ST0(%s);' % (a, a))
            elif kind == 1: s.out('  clip_ = ((clip_ << 6) | %s) & 0xffffffu;' % a)
            else: s.out('  q_ = %s; st_ = (st_ & ~0x30u) | %s | (%s << 6);' % (a, b, b)); st.qn = True; st.cqn = False; s.q_set()   # DIV / SQRT / RSQRT results are VU-form
        st.queue = keep
    # ---- upper op (vu_upper_fl with fl = 1, results held back) ----
    def upper(s, st, w):
        u = dec_upper(w)
        if u is None: return None
        opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
        if u['fl']: st.gvm = '0u'
        if opn == CLIP:
            v = s.var()
            s.out('  { float wv_ = fabsf(%s), x_ = %s, y_ = %s, z_ = %s; uint32_t b_ = 0; if (x_ > wv_) b_ |= 1; if (x_ < -wv_) b_ |= 2; if (y_ > wv_) b_ |= 4; '
                  'if (y_ < -wv_) b_ |= 8; if (z_ > wv_) b_ |= 16; if (z_ < -wv_) b_ |= 32; %s = b_; }' % (s.rd(st, ft, 3), s.rd(st, fs, 0), s.rd(st, fs, 1), s.rd(st, fs, 2), v))
            return ('clip', v)
        cs = [c for c in range(4) if dest & (8 >> c)]
        fl = u['fl']
        mv = s.var() if fl else None
        if not cs:
            if fl: s.out('  %s = 0;' % mv); st.gvm = mv
            return ('mac', mv) if fl else None
        def A(c): return s.rd(st, fs, c)
        def B(c):
            if src in (0, 1, 2, 3): return s.rd(st, ft, src)
            if src == 4: return s.rq(st)
            if src == 5: return ('vu_f(0x%08xu)' % vin_bits(st.I)) if st.I is not None else 'vu_in(vu_f(i_))'
            return s.rd(st, ft, c)
        def AC(c): return s.racc(st, c)
        code = []
        if fl: code.append('uint32_t m_ = 0;')
        res = {}
        for c in cs:
            if opn == ADD: e = '%s + %s' % (A(c), B(c))
            elif opn == SUB: e = '%s - %s' % (A(c), B(c))
            elif opn == MADD: e = '%s + %s * %s' % (AC(c), A(c), B(c))
            elif opn == MSUB: e = '%s - %s * %s' % (AC(c), A(c), B(c))
            elif opn == MAX: e = 'vu_max(%s, %s)' % (A(c), B(c))
            elif opn == MINI: e = 'vu_min(%s, %s)' % (A(c), B(c))
            elif opn == MUL: e = '%s * %s' % (A(c), B(c))
            elif opn in (OPMSUB, OPMULA):
                p = '%s * %s' % ({0: (A(1), s.rd(st, ft, 2)), 1: (A(2), s.rd(st, ft, 0))}.get(c, (A(0), s.rd(st, ft, 1))))
                e = ('%s - %s' % (AC(c), p)) if opn == OPMSUB else p
            elif opn == ABS: e = 'fabsf(%s)' % A(c)
            elif opn == ITOF: e = '(float)(int32_t)%s / %d.0f' % (s.raw(fs, c), 1 << u['sh'])
            if opn == FTOI:
                code.append('t%d_ = vu_ftoi(%s * %d.0f);' % (c, A(c), 1 << u['sh'])); res[c] = False
            else:
                code.append('t%d_ = %s;' % (c, ('vp_out(%s, %d, &m_)' % (e, 3 - c)) if fl else 'vu_out(%s)' % e)); res[c] = True
        if fl: code.append('%s = m_;' % mv); st.gvm = mv
        if u['toacc'] or opn == OPMULA:          # ACC lands at once (vu_upper writes VACC; nothing in the pair reads it but the next pairs)
            s.acc_w = True
            for c in cs: code.append('a%d = t%d_;' % (c, c)); st.accn.add(c); st.acln.discard(c)
            s.out('  { ' + ' '.join(code) + ' }')
            return ('mac', mv) if fl else None
        s.out('  { ' + ' '.join(code) + ' }')
        return ('vf', u['tgt'], cs, res, ('mac', mv) if fl else None)
    # ---- lower op (vu_lower_i), returns ('br', cond C expr or None for always, target) or None ----
    def lower(s, st, w, pc):
        op, dest, ft, fs, fd = w >> 25, (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31
        imm11 = simm11(w); is_, it, id_ = fs & 15, ft & 15, fd & 15
        cs = [c for c in range(4) if dest & (8 >> c)]
        M = lambda a: '((uint32_t *)(VUMEM + (((%s) & 0xffu) << 4)))' % a
        add = lambda a, k: '%s + %du' % (a, k & 0xffffffff) if k >= 0 else '%s - %du' % (a, -k)
        if op == 0x00:
            for c in cs: s.wvf(ft, c, 'vu_f(%s[%d])' % (M(add(s.vi(is_), imm11)), c), st, False)
            return None
        if op == 0x01:
            for c in cs: s.out('  %s[%d] = %s;' % (M(add(s.vi(it), imm11)), c, s.raw(fs, c)))
            return None
        if op == 0x04:
            if cs: s.viset(it, '%s[%d]' % (M(add(s.vi(is_), imm11)), cs[0]))
            return None
        if op == 0x05:
            if cs: s.out('  %s[%d] = %s;' % (M(add(s.vi(is_), imm11)), cs[0], s.vi(it)))
            return None
        imm15 = ((w >> 21) & 15) << 11 | (w & 0x7ff)
        if op == 0x08: s.viset(it, add(s.vi(is_), imm15), imm15 if is_ == 0 else None); return None
        if op == 0x09: s.viset(it, add(s.vi(is_), -imm15), -imm15 if is_ == 0 else None); return None
        c24 = w & 0xffffff; i12 = (w >> 21 & 1) << 11 | (w & 0x7ff)
        if op == 0x10: s.viset(1, '(clip_ & 0xffffffu) == 0x%xu' % c24); return None
        if op == 0x11: s.out('  clip_ = 0x%xu;' % c24); return None
        if op == 0x12: s.viset(1, '(clip_ & 0x%xu) != 0' % c24); return None
        if op == 0x13: s.viset(1, '((clip_ | 0x%xu) & 0xffffffu) == 0xffffffu' % c24); return None
        if op == 0x14: s.viset(it, '(st_ & 0xfffu) == 0x%xu' % i12); s.stat_read = True; return None
        if op == 0x15: s.out('  st_ = (st_ & 0x3fu) | 0x%xu;' % (i12 & 0xfc0)); return None
        if op == 0x16: s.viset(it, 'st_ & 0x%xu' % (i12 & 0xfff)); s.stat_read = True; return None
        if op == 0x17: s.viset(it, '(st_ | 0x%xu) & 0xfffu' % i12); s.stat_read = True; return None
        if op == 0x18: s.viset(it, '(mac_ & 0xffffu) == %s' % s.vi(is_)); return None
        if op == 0x1a: s.viset(it, 'mac_ & %s' % s.vi(is_)); return None
        if op == 0x1b: s.viset(it, 'mac_ | %s' % s.vi(is_)); return None
        if op == 0x1c: s.viset(it, 'clip_ & 0xfffu'); return None                        # FCGET (vu0.h: the clip flags, as the VU manual)
        if op == 0x20: return ('br', None, pc + 1 + imm11)
        if op == 0x21: s.viset(it, '%du' % (pc + 2), pc + 2); return ('br', None, pc + 1 + imm11)
        if op in (0x24, 0x25):                           # vu0.h: JALR links first, then reads VI[is] (an instruction index)
            if op == 0x25: s.viset(it, '%du' % (pc + 2), pc + 2)
            t = 0 if is_ == 0 else st.vik.get(is_)
            if t is None: raise Refuse('JR / JALR at %d: target not known' % pc)
            return ('br', None, t)
        cond = {0x28: '%s == %s' % (s.vi(it), s.vi(is_)), 0x29: '%s != %s' % (s.vi(it), s.vi(is_)), 0x2c: '(int16_t)%s < 0' % s.vi(is_),
                0x2d: '(int16_t)%s > 0' % s.vi(is_), 0x2e: '(int16_t)%s <= 0' % s.vi(is_), 0x2f: '(int16_t)%s >= 0' % s.vi(is_)}.get(op)
        if cond: return ('br', cond, pc + 1 + imm11)
        if op >= 0x40:
            fn = w & 0x3f; i5 = (w >> 6) & 31
            if i5 & 16: i5 -= 32
            if fn == 0x30: s.viset(id_, '%s + %s' % (s.vi(is_), s.vi(it))); return None
            if fn == 0x31: s.viset(id_, '%s - %s' % (s.vi(is_), s.vi(it))); return None
            if fn == 0x32: s.viset(it, add(s.vi(is_), i5), i5 if is_ == 0 else None); return None
            if fn == 0x34: s.viset(id_, '%s & %s' % (s.vi(is_), s.vi(it))); return None
            if fn == 0x35: s.viset(id_, '%s | %s' % (s.vi(is_), s.vi(it))); return None
            idx, sel, fsf, ftf = (w >> 6) & 31, fn & 3, (w >> 21) & 3, (w >> 23) & 3
            if fn >= 0x3c:
                if idx == 12 and sel in (0, 1):                  # MOVE / MR32, in component order (MR32 onto itself reads the new x)
                    for c in cs:
                        sc = c if sel == 0 else (c + 1) & 3
                        s.wvf(ft, c, s.fv(fs, sc), st, fs == 0 or (fs, sc) in st.norm)
                    return None
                if idx == 13:
                    if sel == 0:                                 # LQI
                        for c in cs: s.wvf(ft, c, 'vu_f(%s[%d])' % (M(s.vi(is_)), c), st, False)
                        s.viset(is_, '%s + 1u' % s.vi(is_)); return None
                    if sel == 1:                                 # SQI
                        for c in cs: s.out('  %s[%d] = %s;' % (M(s.vi(it)), c, s.raw(fs, c)))
                        s.viset(it, '%s + 1u' % s.vi(it)); return None
                    if sel == 2:                                 # LQD
                        s.viset(is_, '%s - 1u' % s.vi(is_))
                        for c in cs: s.wvf(ft, c, 'vu_f(%s[%d])' % (M(s.vi(is_)), c), st, False)
                        return None
                    s.viset(it, '%s - 1u' % s.vi(it))            # SQD
                    for c in cs: s.out('  %s[%d] = %s;' % (M(s.vi(it)), c, s.raw(fs, c)))
                    return None
                if idx == 14 and sel != 3:                       # DIV / SQRT / RSQRT: the new Q and D / I bits go to the queue (see pair())
                    qv, dv = s.var(), s.var(); z = s.rzdiv; s.hasdiv = True
                    if sel == 0:
                        s.out('  { float a_ = %s, b_ = %s; if (b_ == 0.0f) { %s = a_ == 0.0f ? 0x10u : 0x20u; %s = ((%s ^ %s) & 0x80000000u) | 0x7f7fffffu; } '
                              'else { %s = 0; %s = vu_u(vu_out(%s)); } st_ |= %s << 6; }' % (s.rd(st, fs, fsf), s.rd(st, ft, ftf), dv, qv, s.raw(fs, fsf), s.raw(ft, ftf), dv, qv,
                                                                                         'vq_divz(a_, b_)' if z else 'a_ / b_', dv))
                    elif sel == 1:
                        s.out('  { float b_ = %s; %s = b_ < 0 ? 0x10u : 0; %s = vu_u(%s(fabsf(b_))); st_ |= %s << 6; }' % (s.rd(st, ft, ftf), dv, qv, 'vq_sqrtz' if z else 'sqrtf', dv))
                    else:
                        s.out('  { float a_ = %s, t_ = %s, b_ = fabsf(t_); %s = t_ < 0 ? 0x10u : 0; if (b_ == 0.0f) { %s |= 0x20u; %s = (%s & 0x80000000u) | 0x7f7fffffu; } '
                              'else %s = vu_u(vu_out(%s)); st_ |= %s << 6; }' % (s.rd(st, fs, fsf), s.rd(st, ft, ftf), dv, dv, qv, s.raw(fs, fsf), qv,
                                                                                 'vq_rsqz(a_, b_)' if z else 'a_ / sqrtf(b_)', dv))
                    return ('q', qv, dv)
                if idx == 14: return None                        # WAITQ (the stall is in pair())
                if idx == 15:
                    if sel == 0: s.viset(it, s.raw(fs, fsf)); return None                       # MTIR
                    if sel == 1:                                                                  # MFIR
                        for c in cs: s.wvf(ft, c, 'vu_f((uint32_t)(int32_t)(int16_t)%s)' % s.vi(is_), st, False)
                        return None
                    if sel == 2:
                        if cs: s.viset(it, '%s[%d]' % (M(s.vi(is_)), cs[0]))                      # ILWR
                        return None
                    if cs: s.out('  %s[%d] = %s;' % (M(s.vi(is_)), cs[0], s.vi(it)))             # ISWR
                    return None
                if idx == 16:                                    # RNEXT / RGET / RINIT / RXOR on the global R
                    if sel == 0: s.out('  { uint32_t r_ = VRr; r_ = (r_ << 1) ^ (((r_ >> 4) ^ (r_ >> 22)) & 1); VRr = (r_ & 0x7fffffu) | 0x3f800000u; }')
                    if sel <= 1:
                        for c in cs: s.wvf(ft, c, 'vu_f(VRr)', st, False)
                        return None
                    s.out('  VRr = 0x3f800000u | ((%s) & 0x7fffffu);' % (s.raw(fs, fsf) if sel == 2 else 'VRr ^ %s' % s.raw(fs, fsf)))
                    return None
        raise Refuse('unsupported lower op %08x at %d' % (w, pc))
    # ---- one pair, as one iteration of vu_run's loop; returns the successor states ----
    def pair(s, st):
        pc = st.pc; lo, hi = s.prog[(pc & 511) * 2], s.prog[(pc & 511) * 2 + 1]
        s.pairs += 1
        if s.pairs > MAXPAIRS: raise Refuse('more than %d pairs' % MAXPAIRS)
        tgt, wm, rd, rm, flags = up_info(hi & 0x07ffffff)
        ib = hi >> 31; lq = low_q(lo) if not ib else 0
        need = st.cyc
        for k in range(2):
            if rd[k]:
                for c in range(4):
                    if rm[k] & (8 >> c): need = max(need, st.vfready.get((rd[k], c), 0))
        if not ib:
            lr, lm = low_reads(lo)
            for k in range(2):
                if lr[k]:
                    for c in range(4):
                        if lm[k] & (8 >> c): need = max(need, st.vfready.get((lr[k], c), 0))
        if lq and st.qready > need: need = st.qready
        st.cyc = cyc = need
        s.out('  /* %d (cycle %d) */' % (pc, cyc))
        s.commit(st, cyc)
        s.cur_lo = None if ib else lo
        up = s.upper(st, hi & 0x07ffffff)
        macv = None
        if up and up[0] == 'mac': macv = up[1]
        if up and up[0] == 'vf' and up[4]: macv = up[4][1]
        if flags == 1:
            if macv is None: raise Refuse('flag op without a MAC value at %d' % pc)
            st.queue.append([cyc + 4, 0, macv, None])
        elif flags == 2: st.queue.append([cyc + 4, 1, up[1], None])
        took = None
        if ib:
            s.set_i(st, lo)
        else:
            lw, lwm = low_writes(lo)
            s.cur = st; r = s.lower(st, lo, pc)
            if r and r[0] == 'q':
                if lq > 1: st.queue.append([cyc + lq, 2, r[1], r[2]]); st.qready = cyc + lq
            elif r: took = r
            if lw:
                for c in range(4):
                    if lwm & (8 >> c): st.vfready[(lw, c)] = cyc + 4
        if up and up[0] == 'vf':
            _, t, cs, res, _ = up
            s.write_up(st, up)
            if t:
                for c in range(4):
                    if wm & (8 >> c): st.vfready[(t, c)] = cyc + 4
        st.cyc += 1
        if st.endnext:
            s.commit(st, 1 << 40)
            if st.gvm is not None: s.out('  gvm_ = %s;' % st.gvm)
            s.out('  tpc_ = %d; goto out_;' % (pc + 1))
            return []
        if (hi >> 30) & 1: st.endnext = 1
        newpend = None                                    # vu_run: npc = pc + 1, or the pending branch; then this pair's branch is pending
        if took is not None:
            if took[1] is None: newpend = ('u', took[2])
            else:                                         # the condition is read at the branch (only this pair's upper ran since)
                s.ncond += 1; cv = 'b%d' % s.ncond; s.out('  %s = %s;' % (cv, took[1])); newpend = ('c', cv, took[2])
        if st.pend is None or st.pend[0] == 'u':
            st.pc = pc + 1 if st.pend is None else st.pend[1]; st.pend = newpend
            return [(st, None)]
        cv, tgt = st.pend[1], st.pend[2]
        tst = st.copy(); tst.pc = tgt; tst.pend = newpend
        st.pc = pc + 1; st.pend = newpend
        return [(tst, cv), (st, None)]
    def q_set(s): pass                               # hooks of the VFPU form (FnV): Q committed, I loaded
    def set_i(s, st, lo): s.out('  i_ = 0x%08xu;' % lo); st.I = lo
    def write_up(s, st, up):                         # the upper op's results, after the lower op
        _, t, cs, res, _ = up
        if t:
            for c in cs: s.wvf(t, c, 't%d_' % c, st, res[c])
    def place(s, st):
        """at a join candidate: jump to an equal state's code, or make this state a join point; returns True when the path ends here"""
        k = st.key()
        nv = s.variants.setdefault(st.pc, set())
        if k not in s.memo and len(nv) >= 6:
            k = st.key(generic=True); st.norm = set(); st.accn = set(); st.I = None; st.cln = set(); st.acln = set(); st.qn = st.cqn = False
        if k in s.memo:
            lab, names, gname = s.memo[k]
            for e, n in zip(st.queue, names):
                s.out('  %s = %s;%s' % (n[0], e[2], (' %s = %s;' % (n[1], e[3])) if n[1] else ''))
            if gname: s.out('  %s = %s;' % (gname, st.gvm))
            s.out('  goto %s;' % lab)
            return True
        nv.add(k)
        names = []
        for e in st.queue:
            a = s.var(); b = s.var() if e[3] else None
            s.out('  %s = %s;%s' % (a, e[2], (' %s = %s;' % (b, e[3])) if b else ''))
            e[2] = a; e[3] = b; names.append((a, b))
        gname = None
        if st.gvm is not None: gname = s.var(); s.out('  %s = %s;' % (gname, st.gvm)); st.gvm = gname
        lab = s.lab(); s.memo[k] = (lab, names, gname); s.out('%s:' % lab)
        return False
    def compile(s, entry, joins):
        st = St(); st.pc = entry
        s.work = [(st, None)]
        while s.work:
            st, lab = s.work.pop()
            if lab: s.out('%s:' % lab)
            first = True
            while True:
                if (st.pc in joins or (first and lab)) and s.place(st): break
                first = False
                nxt = s.pair(st)
                if not nxt: break
                if len(nxt) == 2:
                    (tst, cond), (fst, _) = nxt
                    l = s.lab(); s.out('  if (%s) goto %s;' % (cond, l)); s.work.append((tst, l)); st = fst
                    joins.add(tst.pc); joins.add(fst.pc)
                    continue
                st = nxt[0][0]

# ---- VFPU form (VP_VFPU=<entries>): the PSP's vector unit runs the upper ops ----
VQ_NVF, VQ_I, VQ_Q, VQ_P, VQ_T, VQ_ACC = 26, 26, 28, 29, 30, 31    # quads 0..25: VF registers in order of first use; 27: lane copies
def vqn(i): return 'C%d%d0' % (i >> 2, i & 3)
def vsn(i, c): return 'S%d%d%d' % (i >> 2, i & 3, c)
def vmask(cs): return '' if len(cs) == 4 else 'vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4))
VMOV_RAW = os.environ.get('VP_VMOV_RAW') == '1'
def clip_table():
    t = []
    for i in range(64):
        g, l = i & 7, i >> 3
        t.append((g & 1) | (l & 1) << 1 | (g & 2) << 1 | (l & 2) << 2 | (g & 4) << 2 | (l & 4) << 3)
    return t
class FnV(Fn):
    """the VFPU form of one entry (vupv_<hash>_<entry>; Ffm's consent 2026-10-20: VU0 vector math on the VFPU, last-bit rounding may
    differ).  The same paths, pairs, flag commits and joins as the C form (Fn); the VF registers the program uses live in VFPU quads
    for the call (loaded at entry, stored at the end), ACC, the product temporary, the upper op's result temporary and the Q / I
    broadcast lanes in quads of their own; VI, Q, I, clip, MAC and status stay in C locals as in the C form.  Arithmetic: IEEE single,
    round to nearest, denormals flushed (hwprobe.c probes 1-5): vadd / vsub / vmul, MADD as vmul then vadd (two roundings, as the VU
    model), broadcasts / VF0 / OPMULA's lane rotation by source prefixes, lane masks by the destination prefix.
    The VU model reads Inf / NaN patterns as +-max and clamps results to +-max; the VFPU does neither, so every place where such a value
    could become visible is checked and the call returns 0 with nothing written (the caller runs the C form): the lanes it computed
    (vcmp ES at the end; lanes only ever bit-copied are exact), CLIP inputs, the lanes DIV / SQRT / RSQRT / MTIR read (before use), and MAC values (bit 16 set when a lane
    is NaN / Inf; FMAND / FMEQ / FMOR and the final MAC / g_vmac check it).  NaN / Inf propagate through add / sub / mul, so a value
    that is overwritten unread cannot matter.  Bit copies (MOVE / MR32 of a lane that may hold raw data, LQ) go through lv / mfv / mtv,
    exact for every pattern.  x86 (the reference): the same operations in the same order on a local copy of the quads with
    vu_rnadd / vu_rnsub / vu_rnmul (vu0.h).  MAX / MINI: lane by lane in C (v_maxmin).  After a failed check the C form runs with the
    VFPU's rounding (VPF), so only lanes holding NaN / Inf / huge values depend on which form ran (v15 on the PSP: a garbage lane that
    differed from x86 flipped the check and the other lanes then differed in the last bit).  DIV / SQRT / RSQRT: computed in C with the
    game's rounding (toward zero) in the VFPU form, and the C form of such an entry computes them the same way under VPF too (rzdiv:
    vq_divz / vq_sqrtz / vq_rsqz switch the rounding for the one operation), so Q is the same whichever form ran.  Only with
    VP_VFPU_DIV=1; by default they are refused (the v16-v19 headers).  Measured 2026-10-20 (speed_notes §28): the slerp 33268338 gets a
    form, but a garbage w lane overflows to Inf on the VFPU (the VU saturates to +-max) in 40% of its calls, which then run both forms.
    Refuses: ABS / ITOF / FTOI, CLIP of a lane not computed in the call, stores
    (SQ / SQI / SQD / ISW / ISWR), the random-number ops, more than 26 VF registers."""
    def __init__(s, prog, name):
        Fn.__init__(s, prog, name.replace('vup_', 'vupv_', 1))
        s.qm = {}; s.wm = {}; s.cm = {}; s.useq = s.usei = s.useacc = False; s.D = None   # wm: lanes written; cm: lanes written with a computed value
    def q(s, r):
        if r not in s.qm:
            if len(s.qm) >= VQ_NVF: raise Refuse('VFPU form: more than %d VF registers' % VQ_NVF)
            s.qm[r] = len(s.qm)
        s.vf_used.add(r); return s.qm[r]
    def rd(s, *a): raise Refuse('VFPU form: internal (rd)')
    def raw(s, *a): raise Refuse('VFPU form: internal (raw)')
    def wvf(s, *a): raise Refuse('VFPU form: internal (wvf)')
    def asm(s, lines, outs='', ins='', clob=''):
        s.out('  VPA(__asm__ volatile("%s" : "+m"(vpv_m_)%s%s%s));' % ('\\n\\t'.join(l for l in lines if l), (', ' + outs) if outs else '',
              (' : ' + ins) if ins or clob else '', (' : ' + clob) if clob else ''))
    def vbook(s, st, r, c, norm):
        s.vf_used.add(r); s.vf_written.add(r); s.wm[r] = s.wm.get(r, 0) | (1 << c); st.cln.discard((r, c))
        if norm: st.norm.add((r, c)); s.cm[r] = s.cm.get(r, 0) | (1 << c)
        else: st.norm.discard((r, c))
    def xv(s, r, k): return ('1.0f' if k == 3 else '0.0f') if r == 0 else 'vq_[%d].f[%d]' % (s.q(r), k)
    def xb(s, r, k): return ('0x3f800000u' if k == 3 else '0u') if r == 0 else 'vq_[%d].u[%d]' % (s.q(r), k)
    def opnd(s, r, comps):
        """(register, prefix fields, needs no prefix) of VF r read with source lanes comps"""
        if r == 0: return vqn(VQ_T), ['1' if k == 3 else '0' for k in comps], False
        return vqn(s.q(r)), ['xyzw'[k] for k in comps], comps == [0, 1, 2, 3]
    def lo_touches(s, r):
        lo = s.cur_lo
        if lo is None: return False
        lr, _ = low_reads(lo); lw, _ = low_writes(lo)
        return r in lr or r == lw
    def check(s, lines, xl):
        """lines: vcmp ES instructions; xl: the x86 bit patterns of the same lanes.  Any NaN / Inf: the call is abandoned"""
        a = ['.set push', '.set noreorder', 'move %0, $0']
        for l in lines: a += [l, 'bvtl 4, 1f', 'ori %0, %0, 1', '1:']
        a.append('.set pop')
        s.out('  VPA(__asm__ volatile("%s" : "=&r"(vck_), "+m"(vpv_m_)));' % '\\n\\t'.join(a))
        s.out('  VPC(vck_ = !(%s););' % ' && '.join('vu_fin(%s)' % x for x in xl))
        s.out('  if (__builtin_expect(vck_, 0)) goto vfail_;')
    def lane(s, st, r, c, var):
        """C: var = the bits of VF r lane c, checked (NaN / Inf: abandon); returns the VU-form float expression of it"""
        if r == 0: s.out('  %s = %s;' % (var, '0x3f800000u' if c == 3 else '0u')); return 'vu_f(%s)' % var
        s.out('  VPA(__asm__("mfv %%0, %s" : "=r"(%s) : "m"(vpv_m_)));' % (vsn(s.q(r), c), var))
        s.out('  VPC(%s = %s;);' % (var, s.xb(r, c)))
        s.out('  if (__builtin_expect(VPES(%s), 0)) goto vfail_;' % var)
        return ('vu_f(%s)' if (r, c) in st.norm else 'vu_in(vu_f(%s))') % var
    def q_set(s):
        s.asm(['mtv %%1, %s' % vsn(VQ_Q, 0)], ins='"r"(q_)'); s.out('  VPC(vq_[%d].u[0] = q_;);' % VQ_Q)
    def set_i(s, st, lo):
        s.out('  i_ = 0x%08xu;' % lo); st.I = lo
        s.asm(['mtv %%1, %s' % vsn(VQ_I, 0)], ins='"r"(0x%08xu)' % vin_bits(lo)); s.out('  VPC(vq_[%d].u[0] = 0x%08xu;);' % (VQ_I, vin_bits(lo)))
    # ---- upper op ----
    def upper(s, st, w):
        u = dec_upper(w)
        if u is None: return None
        opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
        if u['fl']: st.gvm = '0u'
        if opn == CLIP: return s.v_clip(st, fs, ft)
        cs = [c for c in range(4) if dest & (8 >> c)]
        fl = u['fl']; mv = s.var() if fl else None
        if not cs:
            if fl: s.out('  %s = 0;' % mv); st.gvm = mv
            return ('mac', mv) if fl else None
        if opn in (MAX, MINI): return s.v_maxmin(st, u, cs)
        if opn not in (ADD, SUB, MADD, MSUB, MUL, OPMSUB, OPMULA): raise Refuse('VFPU form: upper op %d' % opn)
        toacc = u['toacc'] or opn == OPMULA; tgt = u['tgt']
        if not toacc and not tgt and not fl: s.D = None; return ('vf', 0, cs, {c: True for c in cs}, None)
        if opn in (OPMSUB, OPMULA): sc, tc = [1, 2, 0, 0], [2, 0, 1, 1]
        else: sc, tc = [0, 1, 2, 3], ([src] * 4 if src in (0, 1, 2, 3) else [0, 1, 2, 3])
        S, ps, sid = s.opnd(fs, sc)
        if src in (4, 5) and opn not in (OPMSUB, OPMULA):
            k = VQ_Q if src == 4 else VQ_I
            if src == 4: s.useq = True
            else: s.usei = True
            T, pt, tid = vqn(k), ['x'] * 4, False
            tx = lambda c, k=k: 'vq_[%d].f[0]' % k
        else:
            T, pt, tid = s.opnd(ft, tc)
            tx = lambda c: s.xv(ft, tc[c])
        sx = lambda c: s.xv(fs, sc[c])
        if toacc: D = VQ_ACC; s.useacc = True
        elif not tgt: D = VQ_T
        else: D = VQ_T if s.lo_touches(tgt) else s.q(tgt)
        s.D = D
        pre = ([] if sid else ['vpfxs ' + ','.join(ps)]) + ([] if tid else ['vpfxt ' + ','.join(pt)])
        m = vmask(cs); Dn = vqn(D)
        if opn in (ADD, SUB, MUL, OPMULA):
            op = {ADD: 'vadd', SUB: 'vsub', MUL: 'vmul', OPMULA: 'vmul'}[opn]; f = {ADD: 'vu_rnadd', SUB: 'vu_rnsub', MUL: 'vu_rnmul', OPMULA: 'vu_rnmul'}[opn]
            s.asm(pre + [m, '%s.q %s, %s, %s' % (op, Dn, S, T)])
            ex = ['%s(%s, %s)' % (f, sx(c), tx(c)) for c in cs]
        else:                                                    # MADD / MSUB / OPMSUB: the product, then ACC +- it
            s.useacc = True; f = 'vu_rnadd' if opn == MADD else 'vu_rnsub'
            s.asm(pre + ['vmul.q %s, %s, %s' % (vqn(VQ_P), S, T), m, '%s.q %s, %s, %s' % ('vadd' if opn == MADD else 'vsub', Dn, vqn(VQ_ACC), vqn(VQ_P))])
            ex = ['%s(vq_[%d].f[%d], vu_rnmul(%s, %s))' % (f, VQ_ACC, c, sx(c), tx(c)) for c in cs]
        s.out('  VPC({ %s %s });' % (' '.join('float r%d_ = %s;' % (c, e) for c, e in zip(cs, ex)), ' '.join('vq_[%d].f[%d] = r%d_;' % (D, c, c) for c in cs)))
        if fl: s.v_mac(D, cs, mv); st.gvm = mv
        if toacc:
            s.acc_w = True; s.wm['A'] = s.wm.get('A', 0) | sum(1 << c for c in cs)
            for c in cs: st.accn.add(c); st.acln.discard(c)
            return ('mac', mv) if fl else None
        return ('vf', tgt, cs, {c: True for c in cs}, ('mac', mv) if fl else None)
    def v_maxmin(s, st, u, cs):
        """MAX / MINI lane by lane in C on the VU-form inputs (the C form's vu_max / vu_min: a > b ? a : b, so +0 / -0 give b), each input lane
        read through mfv with its NaN / Inf check (MAX does not propagate a NaN, so its inputs are checked here), the results back by mtv"""
        opn, src, ft, fs, tgt = u['opn'], u['src'], u['ft'], u['fs'], u['tgt']
        if not tgt: s.D = None; return ('vf', 0, cs, {c: True for c in cs}, None)
        D = VQ_T if s.lo_touches(tgt) else s.q(tgt); s.D = D
        f = 'vu_max' if opn == MAX else 'vu_min'
        for i, c in enumerate(cs):
            a = s.lane(st, fs, c, 'ua_')
            if src in (0, 1, 2, 3): b = s.lane(st, ft, src, 'ub_')
            elif src == 4: b = 'vu_in(vu_f(q_))'
            elif src == 5: b = ('vu_f(0x%08xu)' % vin_bits(st.I)) if st.I is not None else 'vu_in(vu_f(i_))'
            else: b = s.lane(st, ft, c, 'ub_')
            s.out('  m%d_ = vu_u(vu_out(%s(%s, %s)));' % (i, f, a, b))
        s.asm(['mtv %%%d, %s' % (i + 1, vsn(D, c)) for i, c in enumerate(cs)], ins=', '.join('"r"(m%d_)' % i for i in range(len(cs))))
        s.out('  VPC(%s);' % ' '.join('vq_[%d].u[%d] = m%d_;' % (D, c, i) for i, c in enumerate(cs)))
        return ('vf', tgt, cs, {c: True for c in cs}, None)
    def write_up(s, st, up):
        _, t, cs, res, _ = up
        if not t: return
        if s.D == VQ_T:
            Q = s.q(t)
            s.asm([vmask(cs), 'vmov.q %s, %s' % (vqn(Q), vqn(VQ_T))])
            s.out('  VPC(%s);' % ' '.join('vq_[%d].u[%d] = vq_[%d].u[%d];' % (Q, c, VQ_T, c) for c in cs))
        for c in cs: s.vbook(st, t, c, True)
    def v_mac(s, D, cs, mv):
        """the MAC value of the lanes cs of quad D: Z / S bits (the fast form's), bit 16 when a lane is NaN / Inf"""
        s.out('  VPA(__asm__("%s" : %s : "m"(vpv_m_)));' % ('\\n\\t'.join('mfv %%%d, %s' % (i, vsn(D, c)) for i, c in enumerate(cs)),
                                                           ', '.join('"=r"(m%d_)' % i for i in range(len(cs)))))
        s.out('  VPC(%s);' % ' '.join('m%d_ = vq_[%d].u[%d];' % (i, D, c) for i, c in enumerate(cs)))
        s.out('  %s = %s;' % (mv, ' | '.join('VPM(m%d_, %d)' % (i, 3 - c) for i, c in enumerate(cs))))
    def v_clip(s, st, fs, ft):
        if not fs or not ft: raise Refuse('VFPU form: CLIP with VF0')
        for c in range(3):
            if (fs, c) not in st.norm: raise Refuse('VFPU form: CLIP of a lane not computed in the call (vf%d)' % fs)
        if (ft, 3) not in st.norm: raise Refuse('VFPU form: CLIP of a lane not computed in the call (vf%d)' % ft)
        S, T = vqn(s.q(fs)), vqn(s.q(ft))
        if fs == ft: s.check(['vcmp.q ES, %s, %s' % (S, S)], [s.xb(fs, c) for c in range(4)])
        else: s.check(['vcmp.t ES, %s, %s' % (S, S), 'vcmp.s ES, %s, %s' % (vsn(s.q(ft), 3), vsn(s.q(ft), 3))], [s.xb(fs, c) for c in range(3)] + [s.xb(ft, 3)])
        P = vqn(VQ_P)                                            # |w| in every lane of the product quad, then whole-quad compares (bits 0-2 kept):
        s.asm(['vpfxs |w|,|w|,|w|,|w|', 'vmov.q %s, %s' % (P, T), 'vcmp.q GT, %s, %s' % (S, P), 'vsync', 'mfvc %1, $131',   # no prefix selects lane w of
               'vpfxt -x,-y,-z,-w', 'vcmp.q LT, %s, %s' % (S, P), 'vsync', 'mfvc %2, $131'], outs='"=r"(cg_), "=r"(cl_)')   # a .t operand (PPSSPP reads 0)
        w = 'fabsf(%s)' % s.xv(ft, 3)
        s.out('  VPC(cg_ = %s; cl_ = %s;);' % (' | '.join('(uint32_t)(%s > %s) << %d' % (s.xv(fs, c), w, c) for c in range(3)),
                                              ' | '.join('(uint32_t)(%s < -%s) << %d' % (s.xv(fs, c), w, c) for c in range(3))))
        v = s.var(); s.out('  %s = vpv_clipt[(cg_ & 7u) | (cl_ & 7u) << 3];' % v)
        return ('clip', v)
    # ---- lower op ----
    def v_lq(s, st, ft, cs, a):
        if not ft or not cs: return
        Q = s.q(ft)
        txt = ['lv.q %s, 0(%%1)' % vqn(Q)] if len(cs) == 4 else ['lv.s %s, %d(%%1)' % (vsn(Q, c), 4 * c) for c in cs]
        s.asm(txt, ins='"r"(VUMEM + (((%s) & 0xffu) << 4))' % a, clob='"memory"')
        s.out('  VPC({ const uint32_t *l_ = (const uint32_t *)(VUMEM + (((%s) & 0xffu) << 4)); %s });' % (a, ' '.join('vq_[%d].u[%d] = l_[%d];' % (Q, c, c) for c in cs)))
        for c in cs: s.vbook(st, ft, c, False)
    def v_move(s, st, ft, fs, cs, mr32):
        if not ft or not cs: return
        Q = s.q(ft); src = {}
        for c in cs:
            sc = (c + 1) & 3 if mr32 else c
            src[c] = src[sc] if (mr32 and fs == ft and sc in cs and sc < c) else (fs, sc)
        norm = {c: src[c][0] == 0 or src[c] in st.norm for c in cs}
        vm = [c for c in cs if norm[c] or VMOV_RAW]; rw = [c for c in cs if c not in vm]
        pre = []
        if rw:                                                    # raw lanes: bit copies through general registers
            s.out('  VPA(__asm__ volatile("%s" : %s, "+m"(vpv_m_)));' % ('\\n\\t'.join('mfv %%%d, %s' % (i, vsn(s.q(src[c][0]), src[c][1])) for i, c in enumerate(rw)),
                                                                         ', '.join('"=r"(m%d_)' % i for i in range(len(rw)))))
        if vm:
            comps = [src[c][1] if c in vm else 0 for c in range(4)]
            S, ps, sid = s.opnd(fs, comps)
            s.asm(([] if sid else ['vpfxs ' + ','.join(ps)]) + [vmask(vm), 'vmov.q %s, %s' % (vqn(Q), S)])
        if rw: s.asm(['mtv %%%d, %s' % (i + 1, vsn(Q, c)) for i, c in enumerate(rw)], ins=', '.join('"r"(m%d_)' % i for i in range(len(rw))))
        s.out('  VPC({ %s %s });' % (' '.join('uint32_t r%d_ = %s;' % (c, s.xb(*src[c])) for c in cs), ' '.join('vq_[%d].u[%d] = r%d_;' % (Q, c, c) for c in cs)))
        for c in cs: s.vbook(st, ft, c, norm[c])
    def lower(s, st, w, pc):
        op, dest, ft, fs = w >> 25, (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31
        imm11 = simm11(w); is_, it = fs & 15, ft & 15
        cs = [c for c in range(4) if dest & (8 >> c)]
        add = lambda a, k: '%s + %du' % (a, k & 0xffffffff) if k >= 0 else '%s - %du' % (a, -k)
        if op == 0x00: s.v_lq(st, ft, cs, add(s.vi(is_), imm11)); return None
        if op in (0x01, 0x05): raise Refuse('VFPU form: store at %d' % pc)
        if op in (0x18, 0x1a, 0x1b): s.out('  if (__builtin_expect(mac_ >> 16, 0)) goto vfail_;')
        if op >= 0x40 and (w & 0x3f) >= 0x3c:
            idx, sel, fsf, ftf = (w >> 6) & 31, w & 3, (w >> 21) & 3, (w >> 23) & 3
            if idx == 12 and sel in (0, 1): s.v_move(st, ft, fs, cs, sel == 1); return None
            if idx == 13:
                if sel == 0: s.v_lq(st, ft, cs, s.vi(is_)); s.viset(is_, '%s + 1u' % s.vi(is_)); return None
                if sel == 2: s.viset(is_, '%s - 1u' % s.vi(is_)); s.v_lq(st, ft, cs, s.vi(is_)); return None
                raise Refuse('VFPU form: store at %d' % pc)
            if idx == 14 and sel != 3:
                if os.environ.get('VP_VFPU_DIV') != '1':   # default (v16-v19 headers): refused; VP_VFPU_DIV=1 allows it, with the rzdiv C form
                    raise Refuse('VFPU form: DIV / SQRT / RSQRT at %d (the fallback rounds like the VFPU, the division in C would not)' % pc)
                qv, dv = s.var(), s.var(); s.hasdiv = True
                if sel == 0:
                    a = s.lane(st, fs, fsf, 'ua_'); b = s.lane(st, ft, ftf, 'ub_')
                    s.out('  { float a_ = %s, b_ = %s; if (b_ == 0.0f) { %s = a_ == 0.0f ? 0x10u : 0x20u; %s = ((ua_ ^ ub_) & 0x80000000u) | 0x7f7fffffu; } '
                          'else { %s = 0; %s = vu_u(vu_out(a_ / b_)); } st_ |= %s << 6; }' % (a, b, dv, qv, dv, qv, dv))
                elif sel == 1:
                    b = s.lane(st, ft, ftf, 'ub_')
                    s.out('  { float b_ = %s; %s = b_ < 0 ? 0x10u : 0; %s = vu_u(sqrtf(fabsf(b_))); st_ |= %s << 6; }' % (b, dv, qv, dv))
                else:
                    a = s.lane(st, fs, fsf, 'ua_'); b = s.lane(st, ft, ftf, 'ub_')
                    s.out('  { float a_ = %s, t_ = %s, b_ = fabsf(t_); %s = t_ < 0 ? 0x10u : 0; if (b_ == 0.0f) { %s |= 0x20u; %s = (ua_ & 0x80000000u) | 0x7f7fffffu; } '
                          'else %s = vu_u(vu_out(a_ / sqrtf(b_))); st_ |= %s << 6; }' % (a, b, dv, dv, qv, qv, dv))
                return ('q', qv, dv)
            if idx == 14: return None
            if idx == 15:
                if sel == 0: s.lane(st, fs, fsf, 'ua_'); s.viset(it, 'ua_'); return None
                if sel == 1:
                    if ft and cs:
                        Q = s.q(ft)
                        s.asm(['mtv %%1, %s' % vsn(Q, c) for c in cs], ins='"r"((uint32_t)(int32_t)(int16_t)%s)' % s.vi(is_))
                        s.out('  VPC(%s);' % ' '.join('vq_[%d].u[%d] = (uint32_t)(int32_t)(int16_t)%s;' % (Q, c, s.vi(is_)) for c in cs))
                        for c in cs: s.vbook(st, ft, c, False)
                    return None
                if sel == 3: raise Refuse('VFPU form: store at %d' % pc)
            if idx == 16: raise Refuse('VFPU form: random-number op at %d' % pc)
        return Fn.lower(s, st, w, pc)

def compile_entry_v(prog, h, entry):
    f = FnV(prog, 'vup_%s_%d' % (h, entry)); f.stat_read = False
    joins = set()
    for pc in range(512):
        lo, hi = prog[pc * 2], prog[pc * 2 + 1]
        if hi >> 31: continue
        op = lo >> 25
        if op in (0x20, 0x21, 0x28, 0x29, 0x2c, 0x2d, 0x2e, 0x2f): joins.add(pc + 1 + simm11(lo)); joins.add(pc + 2)
        if op in (0x24, 0x25): f.jr_regs.add((lo >> 11) & 15); joins.add(pc + 2)
    f.compile(entry, joins)
    return f

def emit_v(f):
    """the VFPU form's function: entry loads, the body, the end check and stores; vfail_: nothing was written, returns 0"""
    o = ['static int %s(void)' % f.name, '{']
    for n in sorted(f.vi_used): o.append('  uint32_t vi%d = VIr[%d] & 0xffffu;' % (n, n))
    o.append('  uint32_t q_ = VQr, i_ = VIreg, clip_ = VCLIP, mac_ = VMAC, st_ = VSTAT, gvm_ = g_vmac; int tpc_;')
    if f.nvar: o.append('  uint32_t %s;' % ', '.join('p%d' % i for i in range(1, f.nvar + 1)))
    o.append('  uint32_t m0_, m1_, m2_, m3_, ua_, ub_, cg_, cl_, vck_; int vpv_m_ = 0;' + (' int %s;' % ', '.join('b%d' % i for i in range(1, f.ncond + 1)) if f.ncond else ''))
    o.append('  VPC(VQF vq_[32];)')
    regs = sorted(f.qm.items(), key=lambda kv: kv[1])
    ld = ['lv.q %s, %d(%%1)' % (vqn(qi), 16 * r) for r, qi in regs]; ins = ['"r"(VF)', '"r"(VACC)']
    if f.useacc: ld.append('lv.q %s, 0(%%2)' % vqn(VQ_ACC))
    if f.useq: ins.append('"r"(vu_u(vu_in(vu_f(q_))))'); ld.append('mtv %%%d, %s' % (len(ins), vsn(VQ_Q, 0)))
    if f.usei: ins.append('"r"(vu_u(vu_in(vu_f(i_))))'); ld.append('mtv %%%d, %s' % (len(ins), vsn(VQ_I, 0)))
    o.append('  VPA(__asm__ volatile("%s" : "+m"(vpv_m_) : %s : "memory"));' % ('\\n\\t'.join(ld), ', '.join(ins)))
    xl = ['memcpy(vq_[%d].u, VF[%d].u, 16);' % (qi, r) for r, qi in regs]
    if f.useacc: xl.append('memcpy(vq_[%d].u, VACC, 16);' % VQ_ACC)
    if f.useq: xl.append('vq_[%d].f[0] = vu_in(vu_f(q_));' % VQ_Q)
    if f.usei: xl.append('vq_[%d].f[0] = vu_in(vu_f(i_));' % VQ_I)
    o.append('  VPC(%s)' % ' '.join(xl))
    o += f.lines
    o.append(' out_:')
    keys = [(r, f.qm[r]) for r in sorted(k for k in f.wm if k != 'A')] + ([('A', VQ_ACC)] if 'A' in f.wm else [])
    a = ['.set push', '.set noreorder', 'move %0, $0']; xc = []
    for k, qi in keys:                                   # only lanes that ever held a computed value: bit copies (LQ, raw MOVE / MFIR) are exact
        W = [c for c in range(4) if (f.wm[k] if k == 'A' else f.cm.get(k, 0)) >> c & 1]; Q = vqn(qi)
        if not W: continue
        if W == [0, 1, 2, 3]: a.append('vcmp.q ES, %s, %s' % (Q, Q))
        elif W == [0, 1, 2]: a.append('vcmp.t ES, %s, %s' % (Q, Q))
        elif W == [0, 1]: a.append('vcmp.p ES, %s, %s' % (Q, Q))
        elif W == [0]: a.append('vcmp.s ES, %s, %s' % (vsn(qi, 0), vsn(qi, 0)))
        else: a += ['vpfxs ' + ','.join('xyzw'[c] if c in W else '0' for c in range(4)), 'vcmp.q ES, %s, %s' % (Q, Q)]
        a += ['bvtl 4, 1f', 'ori %0, %0, 1', '1:']
        xc += ['vq_[%d].u[%d]' % (qi, c) for c in W]
    a.append('.set pop')
    if xc:
        o.append('  VPA(__asm__ volatile("%s" : "=&r"(vck_), "+m"(vpv_m_)));' % '\\n\\t'.join(a))
        o.append('  VPC(vck_ = !(%s););' % ' && '.join('vu_fin(%s)' % x for x in xc))
    else: o.append('  vck_ = 0;')
    o.append('  if (__builtin_expect(vck_ | ((mac_ | gvm_) >> 16), 0)) goto vfail_;')
    st = ['sv.q %s, %d(%%1)' % (vqn(qi), 16 * k) for k, qi in keys if k != 'A'] + (['sv.q %s, 0(%%2)' % vqn(VQ_ACC)] if 'A' in f.wm else [])
    if st: o.append('  VPA(__asm__ volatile("%s" : "+m"(vpv_m_) : "r"(VF), "r"(VACC) : "memory"));' % '\\n\\t'.join(st))
    o.append('  VPC(%s)' % ' '.join(('memcpy(VACC, vq_[%d].u, 16);' % qi) if k == 'A' else ('memcpy(VF[%d].u, vq_[%d].u, 16);' % (k, qi)) for k, qi in keys))
    for n in sorted(f.vi_written): o.append('  VIr[%d] = vi%d;' % (n, n))
    o.append('  VQr = q_; VIreg = i_; VCLIP = clip_; VMAC = mac_; VSTAT = st_; g_vmac = gvm_; vu_tpc = tpc_; return 1;')
    o.append(' vfail_:')
    o.append('  g_vgrp_fb++; return 0;')
    o.append('}')
    return o

HDR_V = r"""/* VFPU form of compiled programs (recomp/vupipe.py FnV, -DVP_VFPU): vupv_<hash>_<entry>, tried first; 0 = nothing written, the C form runs */
#if defined(__psp__)
#define VPA(...) __VA_ARGS__
#define VPC(...)
#else
#define VPA(...)
#define VPC(...) __VA_ARGS__
#endif
#if defined(VP_VFPU) && defined(VPV_STATS) && !defined(__psp__)   /* dev: calls of the VFPU forms and how many returned 1, printed at exit */
static long vpv_calls, vpv_ok;
__attribute__((destructor)) static void vpv_report(void) { fprintf(stderr, "vpv: %%ld VFPU-form calls, %%ld completed, %%ld fell back\n", vpv_calls, vpv_ok, vpv_calls - vpv_ok); }
#define VPV(f) (vpv_calls++, f() ? (vpv_ok++, 1) : 0) ||
#elif defined(VP_VFPU)
#define VPV(f) f() ||
#else
#define VPV(f)
#endif
#ifdef VP_VFPU   /* the C form after a VFPU form returned 0 runs with the VFPU's rounding (vu0.h VU_FB_ON), so the lanes that hold ordinary values
                    %s */
#define VPF(f) ({ uint32_t fm_; int r_; VU_FB_ON(fm_); r_ = f(); VU_FB_OFF(fm_); r_; })
#else
#define VPF(f) f()
#endif
#define VPES(u) ((((u) >> 23) & 0xffu) == 0xffu)
#define VPM(u, sh) (((uint32_t)(((u) << 1) == 0) << (sh)) | (((u) >> 31) << ((sh) + 4)) | ((uint32_t)VPES(u) << 16))
static const uint8_t vpv_clipt[64] = { %s };
"""

def compile_entry(prog, h, entry, rzdiv=False):
    f = Fn(prog, 'vup_%s_%d' % (h, entry)); f.stat_read = False; f.rzdiv = rzdiv
    joins = set()
    for pc in range(512):                                      # branch targets and the pairs after delay slots join paths
        lo, hi = prog[pc * 2], prog[pc * 2 + 1]
        if hi >> 31: continue
        op = lo >> 25
        if op in (0x20, 0x21, 0x28, 0x29, 0x2c, 0x2d, 0x2e, 0x2f): joins.add(pc + 1 + simm11(lo)); joins.add(pc + 2)
        if op in (0x24, 0x25): f.jr_regs.add((lo >> 11) & 15); joins.add(pc + 2)    # (a JR's targets: the link points, joined above)
    f.compile(entry, joins)
    return f

def mem_form(f):
    """VP_MEM=all | min=<lanes> | <name>,...: VF and ACC read and written in place (VFM / VAM) instead of C locals loaded at entry and
    stored at out_.  Every exit goes through out_, nothing in a compiled program reads the VU state behind its locals, and no other code
    runs during the call, so the order of the reads and writes is the same; only the PSP code changes: GCC spilled a program with many
    live lanes (vup_33268338_0: 88 VF lanes, all loaded and copied to the stack at entry, a third of its instructions stack traffic)."""
    m = os.environ.get('VP_MEM', '')
    if not m: return False
    if m == 'all': return True
    if m.startswith('min='): return 4 * len(f.vf_used) >= int(m[4:])
    return f.name in m.split(',')

def emit(f):
    o = ['static int %s(void)' % f.name, '{']
    vf = sorted(f.vf_used); mem = mem_form(f)
    if not mem:
        for r in vf: o.append('  float %s;' % ', '.join('f%d%s = vu_f(VF[%d].u[%d])' % (r, CN[c], r, c) for c in range(4)))
        if f.acc_w: o.append('  float a0 = vu_f(VACC[0]), a1 = vu_f(VACC[1]), a2 = vu_f(VACC[2]), a3 = vu_f(VACC[3]);')
    for n in sorted(f.vi_used): o.append('  uint32_t vi%d = VIr[%d] & 0xffffu;' % (n, n))
    o.append('  uint32_t q_ = VQr, i_ = VIreg, clip_ = VCLIP, mac_ = VMAC, st_ = VSTAT, gvm_ = g_vmac; int tpc_;')
    if f.nvar: o.append('  uint32_t %s;' % ', '.join('p%d' % i for i in range(1, f.nvar + 1)))
    if f.cl_used: o.append('  float %s;' % ', '.join(sorted(f.cl_used)))
    o.append('  float t0_, t1_, t2_, t3_;' + (' int %s;' % ', '.join('b%d' % i for i in range(1, f.ncond + 1)) if f.ncond else ''))
    if mem:
        fre, are = re.compile(r'\bf(\d+)([xyzw])\b'), re.compile(r'\ba([0-3])\b')
        o += [are.sub(r'VAM(\1)', fre.sub(lambda m: 'VFM(%s,%d)' % (m.group(1), CN.index(m.group(2))), l)) for l in f.lines]
        o.append(' out_:')
    else:
        o += f.lines
        o.append(' out_:')
        for r in sorted(f.vf_written): o.append('  ' + ' '.join('VF[%d].u[%d] = vu_u(f%d%s);' % (r, c, r, CN[c]) for c in range(4)))
        if f.acc_w: o.append('  VACC[0] = vu_u(a0); VACC[1] = vu_u(a1); VACC[2] = vu_u(a2); VACC[3] = vu_u(a3);')
    for n in sorted(f.vi_written): o.append('  VIr[%d] = vi%d;' % (n, n))
    o.append('  VQr = q_; VIreg = i_; VCLIP = clip_; VMAC = mac_; VSTAT = st_; g_vmac = gvm_; vu_tpc = tpc_; return 1;')
    o.append('}')
    return o

HDR = r'''/* generated by recomp/vupipe.py: VU0 micro programs compiled with the vu0.h interpreter's pipeline timing (see vupipe.py) */
#ifndef GEN_VU_H
#define GEN_VU_H
static inline float vu_f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t vu_u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline __attribute__((always_inline)) float vu_in(float f) { uint32_t u = vu_u(f), e = u & 0x7f800000u; if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; return vu_f(u); }
#define vu_cl vu_in
typedef union { uint32_t u[4]; float f[4]; } __attribute__((aligned(16))) VQF;   /* VP_MEM: a VF / ACC lane read and written in place */
#define VFM(r, c) (((VQF *)VF)[r].f[c])
#define VAM(c) (((float *)VACC)[c])
static inline __attribute__((always_inline)) float vu_clf(float f, int sh) { return vu_f(vout(f, sh)); }   /* + MAC bits (g_vmac) */
static inline __attribute__((always_inline)) float vu_max(float a, float b) { return a > b ? a : b; }
static inline __attribute__((always_inline)) float vu_min(float a, float b) { return a < b ? a : b; }
static inline __attribute__((always_inline)) float vu_ftoi(float v) { return vu_f(v >= 2147483648.0f ? 0x7fffffffu : v <= -2147483648.0f ? 0x80000000u : (uint32_t)(int32_t)v); }
#ifdef VP_FAST
/* fast form (-DVP_FAST): a result computed from VU-form inputs (finite, no denormal: every input is clamped by vu_in or is an earlier
   result) needs no clamp when the FPU rounds toward zero and flushes denormal results: an overflow gives +-Fmax (round toward zero), an
   underflow the signed zero, so the value is already the one vout gives and its MAC bits are only Z and S (vout's U / O never fire on
   such a value).  That FPU mode is the reference's (x86 MXCSR RZ + FTZ, MGS2_FPU_RZ) and the PSP's (FCR31 0x01000001; Allegrex RZ and FS
   measured on hardware in the TT port, TT_PSP_port_handoff.md 3.3); vuc_dispatch checks it on every call and leaves the call to the
   interpreter otherwise. */
#define vu_out(f) (f)
static inline __attribute__((always_inline)) float vp_out(float f, int sh, uint32_t *m)
{
  uint32_t u = vu_u(f);
  *m |= (uint32_t)((u << 1) == 0) << sh | (u >> 31) << (sh + 4);
  return f;
}
#if defined(__psp__)
static inline int vp_fast_env(void) { uint32_t r; __asm__ volatile("cfc1 %0, $31" : "=r"(r)); return (r & 0x01000003u) == 0x01000001u; }
#else
static inline int vp_fast_env(void) { return (_mm_getcsr() & 0xe000u) == 0xe000u; }
#endif
#else
#define vu_out vu_in
/* vout (vu0.h) into a local MAC value */
static inline __attribute__((always_inline)) float vp_out(float f, int sh, uint32_t *m)
{
  uint32_t u = vu_u(f), e = u & 0x7f800000u;
  if (!e) { *m |= 1u << sh; if (u & 0x7fffffu) *m |= 0x100u << sh; u &= 0x80000000u; }
  else if (e == 0x7f800000u) { *m |= 0x1000u << sh; u = (u & 0x80000000u) | 0x7f7fffffu; }
  if (u & 0x80000000u) *m |= 0x10u << sh;
  return vu_f(u);
}
#endif
static inline __attribute__((always_inline)) uint32_t vp_cur(uint32_t m) { return ((m & 0xfu) != 0) | ((m & 0xf0u) != 0) << 1 | ((m & 0xf00u) != 0) << 2 | ((m & 0xf000u) != 0) << 3; }
#ifdef VP_EXACT_FLAGS   /* the status register as the interpreter keeps it (vu_commit kind 0) */
#define VP_ST0(m) do { uint32_t c_ = vp_cur(m); st_ = (st_ & 0xff0u) | c_ | (c_ << 6); } while (0)
#else                   /* status Z / S / U / O and their sticky bits not maintained: nothing reads them (vupipe.py) */
#define VP_ST0(m) do { } while (0)
#endif
'''

HDR_RZ = r"""/* DIV / SQRT / RSQRT in the C form of an entry whose VFPU form divides (vupipe.py rzdiv): the VFPU form computes them in C in the game's
   FPU mode (round toward zero, flush), and the C form runs after a failed VFPU form under VPF (round to nearest), so there these switch
   to the game's rounding for the one operation (PSP FCR31 rounding mode 1, x86 MXCSR rounding toward zero; flush bits kept) and Q is the
   same whichever form ran.  The empty asm statements keep the operation between the mode switches. */
#if defined(VP_VFPU) && defined(__psp__)
static inline __attribute__((always_inline)) uint32_t vq_rz_on(void)
{ uint32_t m; __asm__ volatile("nop\n\tcfc1 %0, $31" : "=r"(m)); __asm__ volatile("ctc1 %0, $31\n\tnop\n\tnop" : : "r"((m & ~3u) | 1u)); return m; }
#define vq_rz_off(m) __asm__ volatile("ctc1 %0, $31\n\tnop\n\tnop" : : "r"(m))
#define VQ_KEEP(x) __asm__ volatile("" : "+f"(x))
#elif defined(VP_VFPU)
static inline __attribute__((always_inline)) uint32_t vq_rz_on(void) { uint32_t m = _mm_getcsr(); _mm_setcsr((m & ~0x6000u) | 0x6000u); return m; }
#define vq_rz_off(m) _mm_setcsr(m)
#define VQ_KEEP(x) __asm__ volatile("" : "+x"(x))
#endif
#ifdef VP_VFPU
static inline __attribute__((always_inline)) float vq_divz(float a, float b) { uint32_t m = vq_rz_on(); float r; VQ_KEEP(a); VQ_KEEP(b); r = a / b; VQ_KEEP(r); vq_rz_off(m); return r; }
static inline __attribute__((always_inline)) float vq_sqrtz(float b) { uint32_t m = vq_rz_on(); float r; VQ_KEEP(b); r = sqrtf(b); VQ_KEEP(r); vq_rz_off(m); return r; }
static inline __attribute__((always_inline)) float vq_rsqz(float a, float b) { uint32_t m = vq_rz_on(); float r; VQ_KEEP(a); VQ_KEEP(b); r = a / sqrtf(b); VQ_KEEP(r); vq_rz_off(m); return r; }
#else
#define vq_divz(a, b) ((a) / (b))
#define vq_sqrtz(b) sqrtf(b)
#define vq_rsqz(a, b) ((a) / sqrtf(b))
#endif
"""

def main():
    d, outp = sys.argv[1], sys.argv[2]
    fns, disp = [], {}
    # VP_VFPU=all | <hash>:<entry>,...: also the VFPU form (FnV) of those entries, tried first by vuc_dispatch under -DVP_VFPU
    vsel = os.environ.get('VP_VFPU', ''); vfns = []; vdisp = {}
    for spec in sys.argv[3:]:
        h, es = spec.split(':')
        prog = struct.unpack('<1024I', open(os.path.join(d, h + '.bin'), 'rb').read())
        for e in map(int, es.split(',')):
            try:
                f = compile_entry(prog, h, e)
            except Refuse as x:
                print('%s entry %d: not compiled: %s' % (h, e, x)); continue
            if f.stat_read: print('%s entry %d: reads status flags: build with -DVP_EXACT_FLAGS' % (h, e))
            print('%s entry %d: %d pairs emitted, %d join points' % (h, e, f.pairs, len(f.memo)))
            fns.append(f); disp.setdefault(h, []).append(e)
            if vsel == 'all' or '%s:%d' % (h, e) in vsel.split(','):
                try:
                    fv = compile_entry_v(prog, h, e)
                    print('%s entry %d: VFPU form, %d VF registers in quads, %d pairs' % (h, e, len(fv.qm), fv.pairs)); vfns.append(fv); vdisp[(h, e)] = fv.name
                    if fv.hasdiv:                       # its C form divides with the game's rounding even under VPF (see HDR vq_divz)
                        fns[-1] = compile_entry(prog, h, e, rzdiv=True); print('%s entry %d: C form with the division in the game\'s rounding' % (h, e))
                except Refuse as x:
                    print('%s entry %d: no VFPU form: %s' % (h, e, x))
    o = [HDR + (HDR_RZ if any(f.rzdiv for f in fns) else '')]; same = {}; alias = {}                  # an entry whose compiled body equals an earlier one's (the same code in another image) reuses it
    # VP_KEEP=<earlier gen_vu.h>: the functions it did not define go to <out>_tail.h (boot.c includes it after its last function) and get a
    # prototype here, so every function of the runtime unit keeps its definition number and its PGO profile (GCC matches gcda records by
    # that number); only vuc_entry / vuc_dispatch change
    keep = set(re.findall(r'^static int (vup_\w+)\(void\)$', open(os.environ['VP_KEEP']).read(), re.M)) if os.environ.get('VP_KEEP') else None
    tail = []
    for f in fns:
        body = emit(f); k = '\n'.join(body[1:])
        if k in same: alias[f.name] = same[k]; print('%s: same code as %s' % (f.name, same[k])); continue
        same[k] = f.name
        if keep is not None and f.name not in keep: o.append('static int %s(void);   /* defined in the tail header */' % f.name); tail += body
        else: o += body
    o.append(HDR_V % ("come out the same whichever form ran; DIV / SQRT / RSQRT, which both forms compute in C, use the game's rounding in both (vq_divz)"
                      if any(f.rzdiv for f in fns) else "come out the same whichever form ran; the VFPU forms have no DIV / SQRT / RSQRT (FnV), the only ops the C form would round otherwise",
                      ', '.join(str(x) for x in clip_table()))); samev = {}
    for f in vfns:
        body = emit_v(f); k = '\n'.join(body[1:])
        if k in samev:
            for he, n in vdisp.items():
                if n == f.name: vdisp[he] = samev[k]
            print('%s: same code as %s' % (f.name, samev[k])); continue
        samev[k] = f.name
        if keep is not None: o.append('static int %s(void);   /* defined in the tail header */' % f.name); tail += body
        else: o += body
    if keep is not None:
        tp = os.path.abspath(outp[:-2] + '_tail.h')
        o.append('#define MGS2_GEN_VU_TAIL "%s"   /* boot.c includes it after its last function */' % tp)
        open(tp, 'w').write('/* generated by recomp/vupipe.py (VP_KEEP): micro programs compiled after the PGO profile was taken */\n' + '\n'.join(tail) + '\n')
        print('%d functions in %s' % (sum(1 for x in tail if x.startswith('static int vup_')), tp))
    o.append('static int vuc_entry(uint32_t hash, int pc)\n{\n  switch (hash) {')
    for h, es in disp.items(): o.append('  case 0x%su: switch (pc) { %s } return 0;' % (h, ' '.join('case %d: return 1;' % e for e in es)))
    o.append('  }\n  return 0;\n}')
    o.append('static int vuc_dispatch(uint32_t hash, int pc)\n{\n#ifdef VP_FAST\n  if (!vp_fast_env()) return 0;\n#endif\n  switch (hash) {')
    def call(h, e):
        c = alias.get('vup_%s_%d' % (h, e), 'vup_%s_%d' % (h, e))
        return ('VPV(%s) VPF(%s)' % (vdisp[(h, e)], c)) if (h, e) in vdisp else c + '()'
    for h, es in disp.items(): o.append('  case 0x%su: switch (pc) { %s } return 0;' % (h, ' '.join('case %d: return %s;' % (e, call(h, e)) for e in es)))
    o.append('  }\n  return 0;\n}')
    o.append('#ifdef VU_FAST\nstatic int vucf_dispatch(uint32_t hash, int pc) { (void)hash; (void)pc; return 0; }\n#endif')
    o.append('#ifdef VU_VFPU\nstatic int vucv_dispatch(uint32_t hash, int pc) { (void)hash; (void)pc; return 0; }\n#endif')
    o.append('#endif')
    open(outp, 'w').write('\n'.join(o) + '\n')

if __name__ == '__main__': main()
