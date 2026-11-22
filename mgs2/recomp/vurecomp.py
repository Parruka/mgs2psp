"""VU0 micro program AOT compiler -> demo/gen_vu.h.
Input: the micro memory images the game uploads (MPG) and runs, collected by a -DVU_DUMP build of the headless game (demo_src/vu0.h):
<dir>/<hash>.bin = the 1024 words of micro memory, <dir>/entries.txt = "<hash> <entry pc>" lines.

Each image becomes one C function with the exact semantics of the vu0.h interpreter (vu_run / vu_upper_fl / vu_lower_i): per pair the upper op
runs before the lower op, a branch takes effect after its delay slot, the E bit ends the program after the next pair (vu_tpc = the pair after
that), the I bit makes the lower word the I immediate.  Only the pairs reachable from the recorded entries are emitted, as straight-line code
with gotos, every instruction decoded here into specialized C:
- the VF registers / ACC / Q / I / VI registers the program uses live in C locals for the whole call (loaded at entry, written back at exit);
- the VU float model (vin: a denormal input reads as +-0, an Inf/NaN pattern as +-Fmax; vclamp: the same on every result) is applied to an
  input only when the value is not known to be in that form already: a forward dataflow over the blocks tracks, per register component,
  whether it holds a value produced by an FMAC op / DIV (already clamped) or raw bits (LQI, MFIR, FTOI, entry state);
- MAC / status flags are computed only where a later FMAND/FMEQ/FMOR/FS* can read them (backward liveness; nothing reads them after a micro
  program: the EE reads MAC with cfc2 only right after its own macro ops).
vu_run selects the function by the image hash and entry (vuc_dispatch); anything else falls back to the interpreter.
-DVU_CHECK (demo_src/vu0.h) runs the interpreter on a copy of the state after every compiled call and compares (differential test).
usage: python3 vurecomp.py [dir=/home/claude/work/vu]; MGS2: VU_OUT=<output dir>, VU_ONLY=<hash>[,<hash>...]"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vuvfpu
d = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/work/vu'
entries = {}
only = set(x for x in os.environ.get('VU_ONLY', '').split(',') if x)   # MGS2: VU_ONLY=<hash>,... compiles only those images
for ln in open(os.path.join(d, 'entries.txt')):
    h, pc = ln.split()
    if not only or h in only: entries.setdefault(h, set()).add(int(pc))
OUT = os.environ.get('VU_OUT', '/home/claude/proj/TwoTowers_PSP_viewer/demo')   # MGS2: VU_OUT=<dir> for gen_vu.h / gen_vu_body.h
BR = {0x20, 0x21, 0x28, 0x29, 0x2c, 0x2d, 0x2e, 0x2f}           # B BAL IBEQ IBNE IBLTZ IBGTZ IBLEZ IBGEZ
JR = {0x24, 0x25}
FLAG_READ = {0x14, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c}
C = 'xyzw'
# ---- upper decode (mirrors vu0.h vu_upper_fl) ----
ADD, SUB, MADD, MSUB, MAX, MINI, MUL, OPMSUB, OPMULA, ABS, ITOF, FTOI, CLIP, NOP = range(14)
N1 = [MUL, MAX, MUL, MINI, ADD, MADD, ADD, MADD, SUB, MSUB, SUB, MSUB, ADD, MADD, MUL, MAX, SUB, MSUB, OPMSUB, MINI]
S1 = [4, 5, 5, 5, 4, 4, 5, 5, 4, 4, 5, 5, -1, -1, -1, -1, -1, -1, -1, -1]
N3 = [[ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, MUL, ADD, SUB, ADD, SUB], [ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, ABS, MADD, MSUB, MADD, MSUB],
      [ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, MUL, ADD, SUB, MUL, OPMULA], [ADD, SUB, MADD, MSUB, ITOF, FTOI, MUL, CLIP, MADD, MSUB, NOP, NOP]]
S3 = [[0, 0, 0, 0, -1, -1, 0, 4, 4, 4, -1, -1], [1, 1, 1, 1, -1, -1, 1, -1, 4, 4, -1, -1], [2, 2, 2, 2, -1, -1, 2, 5, 5, 5, -1, -1], [3, 3, 3, 3, -1, -1, 3, -1, 5, 5, -1, -1]]
def dec_upper(w):
    op, dest, ft, fs, fd, sel, idx = w & 0x3f, (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 3, (w >> 6) & 31
    opn, src, toacc, tgt, sh = NOP, -1, 0, fd, 0
    if op < 0x1c: opn, src = op >> 2, sel
    elif op < 0x30: opn, src = N1[op - 0x1c], S1[op - 0x1c]
    elif op >= 0x3c:
        if idx >= 12: return None
        opn, src, toacc = N3[sel][idx], S3[sel][idx], 1
        if opn in (ITOF, FTOI, ABS): toacc, tgt, sh = 0, ft, [0, 4, 12, 15][sel]
        if opn == OPMULA: src = -1
    else: return None
    if opn == NOP: return None
    return dict(opn=opn, src=src, toacc=toacc, tgt=tgt, sh=sh, dest=dest, ft=ft, fs=fs)
def writes_flags(w):
    u = dec_upper(w); return u is not None and u['opn'] != CLIP
def simm11(w): v = w & 0x7ff; return v - 0x800 if v & 0x400 else v
def fhex(u): return 'vu_f(0x%08xu)' % u
def vin_bits(u):
    e = u & 0x7f800000
    if not e: return u & 0x80000000
    if e == 0x7f800000: return (u & 0x80000000) | 0x7f7fffff
    return u

class St:
    """compile-time knowledge: norm[(r, c)] / acc[c] / q: value already in VU float form; I: the I register constant (None = unknown)"""
    def __init__(s, norm=None, acc=None, q=False, I=None):
        s.norm = dict(norm or {}); s.acc = list(acc or [False] * 4); s.q = q; s.I = I
    def copy(s): return St(s.norm, s.acc, s.q, s.I)
    """norm values: True (a result: VU float form), a frozenset of (reg, comp) (the raw value some of these components had on entry, copied or
    unchanged, or a result: the fast form checks those components once at entry and then reads the value as it is; the exact form maps it
    with vu_in at each read; the plain entry state of (r, c) is {(r, c)}),
    the exact form maps it with vu_in at each read), False (anything else: vu_in at each read)"""
    def key(s): return (tuple(sorted((k, v if v is True else tuple(sorted(map(str, v)))) for k, v in s.norm.items() if v)), tuple(s.acc), s.q, s.I)
    def meet(s, o):
        def m(a, b):
            if a == b: return a
            if not a or not b: return False
            return (a if a is not True else frozenset()) | (b if b is not True else frozenset())
        return St({k: m(s.norm.get(k, False), o.norm.get(k, False)) for k in set(s.norm) | set(o.norm)}, [a and b for a, b in zip(s.acc, o.acc)], s.q and o.q, s.I if s.I == o.I else None)
    def isn(s, r, c): return r == 0 or s.norm.get((r, c), False) is True
    def form(s, r, c): return True if r == 0 else s.norm.get((r, c), False)

def fv(r, c):
    if r == 0: return '1.0f' if c == 3 else '0.0f'
    return 'f%d%s' % (r, C[c])

class Gen:
    def __init__(s): s.vf_used = set(); s.vf_written = set(); s.vi_used = set(); s.vi_written = set(); s.ine = set(); s.acc = False; s.q = False; s.ireg = False
    def rd(s, r, c, st):
        if r: s.vf_used.add(r)
        f = st.form(r, c)
        if isinstance(f, frozenset):
            if 'L' in f:
                s.cur_out.append('  VU_LCHK(%s);' % fv(r, c)); f = (f - {'L'}) | {'K'}; st.norm[(r, c)] = f
            s.ine.update(x for x in f if x not in ('L', 'K')); return 'VU_INE(%s)' % fv(r, c) if f else fv(r, c)
        return fv(r, c) if f is True else 'vu_in(%s)' % fv(r, c)
    def wvf(s, r):
        s.vf_used.add(r); s.vf_written.add(r)
    def vi(s, n):
        if not n: return '0u'
        s.vi_used.add(n); return 'vi%d' % n
    def wvi(s, n): s.vi_used.add(n); s.vi_written.add(n); return 'vi%d' % n
    def upper(s, w, fl, st, out):
        s.cur_out = out
        u = dec_upper(w)
        if u is None: return
        opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
        if opn == CLIP:
            out.append('  { float wv_ = fabsf(%s), x_ = %s, y_ = %s, z_ = %s; uint32_t b_ = 0; if (x_ > wv_) b_ |= 1; if (x_ < -wv_) b_ |= 2; if (y_ > wv_) b_ |= 4; '
                       'if (y_ < -wv_) b_ |= 8; if (z_ > wv_) b_ |= 16; if (z_ < -wv_) b_ |= 32; VCLIP = ((VCLIP << 6) | b_) & 0xffffff; }'
                       % (s.rd(ft, 3, st), s.rd(fs, 0, st), s.rd(fs, 1, st), s.rd(fs, 2, st)))
            return
        cs = [c for c in range(4) if dest & (8 >> c)]
        if fl: out.append('  g_vmac = 0;')
        if not cs:
            if fl: out.append('  vu_setflags();')
            return
        tmp = []; code = []
        def A(c): return s.rd(fs, c, st)
        def B(c):
            if src in (0, 1, 2, 3): return s.rd(ft, src, st)
            if src == 4: s.q = True; return 'q_' if st.q else 'vu_in(q_)'
            if src == 5:
                if st.I is not None: return fhex(vin_bits(st.I))
                s.ireg = True; return 'vu_in(vu_f(ireg_))'
            return s.rd(ft, c, st)
        def ACC(c): s.acc = True; return 'acc%d_' % c if st.acc[c] else 'vu_in(acc%d_)' % c
        res = {}
        for c in cs:
            if opn == ADD: e = '%s + %s' % (A(c), B(c))
            elif opn == SUB: e = '%s - %s' % (A(c), B(c))
            elif opn == MADD: e = '%s + %s * %s' % (ACC(c), A(c), B(c))
            elif opn == MSUB: e = '%s - %s * %s' % (ACC(c), A(c), B(c))
            elif opn == MAX: e = 'vu_max(%s, %s)' % (A(c), B(c))
            elif opn == MINI: e = 'vu_min(%s, %s)' % (A(c), B(c))
            elif opn == MUL: e = '%s * %s' % (A(c), B(c))
            elif opn in (OPMSUB, OPMULA):
                p = {0: (1, 2), 1: (2, 0), 2: (0, 1)}.get(c)
                pe = '%s * %s' % (A(p[0]), s.rd(ft, p[1], st)) if p else '0.0f'      # (c == 3: A[0] * Bv[1] in the interpreter's default branch)
                if c == 3: pe = '%s * %s' % (A(0), s.rd(ft, 1, st))
                e = ('%s - (%s)' % (ACC(c), pe)) if opn == OPMSUB else pe
            elif opn == ABS: e = 'fabsf(%s)' % A(c)
            elif opn == ITOF: s.vf_used.add(fs); e = '(float)(int32_t)vu_u(%s) / %d.0f' % (fv(fs, c), 1 << u['sh'])
            elif opn == FTOI: e = None
            if opn == FTOI:
                code.append('  t%d_ = vu_ftoi(%s * %d.0f);' % (c, A(c), 1 << u['sh'])); tmp.append(c); res[c] = 'raw'
            else:
                code.append('  t%d_ = %s;' % (c, ('vu_clf(%s, %d)' % (e, 3 - c)) if fl else ('VU_CLR(%s)' % e))); tmp.append(c); res[c] = 'norm'
        out.append('  { float %s;' % ', '.join('t%d_' % c for c in tmp)); out += ['  ' + x for x in code]
        for c in cs:
            if u['toacc'] or opn == OPMULA:
                s.acc = True; out.append('    acc%d_ = t%d_;' % (c, c)); st.acc[c] = True
            elif u['tgt']:
                s.wvf(u['tgt']); out.append('    %s = t%d_;' % (fv(u['tgt'], c), c)); st.norm[(u['tgt'], c)] = res[c] == 'norm'
        out.append('  }')
        if fl: out.append('  vu_setflags();')
    def lower(s, w, pc, st, out):
        s.cur_out = out
        """returns a C condition string for a branch, else None"""
        op = w >> 25; dest = (w >> 21) & 15; ft = (w >> 16) & 31; fs = (w >> 11) & 31; fd = (w >> 6) & 31
        it, is_, id_ = ft & 15, fs & 15, fd & 15
        if op == 0x08:
            imm = ((w >> 21) & 15) << 11 | (w & 0x7ff)
            if it: out.append('  %s = (%s + %du) & 0xffff;' % (s.wvi(it), s.vi(is_), imm))
            return None
        imm11 = (w & 0x7ff) - (0x800 if w & 0x400 else 0)
        cs0 = [c for c in range(4) if dest & (8 >> c)]
        if op == 0x00:                                                       # LQ (MGS2; as vu0.h, as LQI without the increment)
            if ft and cs0:
                s.wvf(ft); out.append('  { const uint32_t *m_ = (const uint32_t *)(VUMEM + (((%s + (%d)) & 0xff) << 4)); %s }' % (s.vi(is_), imm11, ' '.join('%s = vu_f(m_[%d]);' % (fv(ft, c), c) for c in cs0)))
                for c in cs0: st.norm[(ft, c)] = frozenset({'L'})
            return None
        if op == 0x01:                                                       # SQ (MGS2; as vu0.h, as SQI without the increment)
            if fs: s.vf_used.add(fs)
            if cs0: out.append('  { uint32_t a_ = (%s + (%d)) & 0xffff; uint32_t *m_ = (uint32_t *)(VUMEM + ((a_ & 0xff) << 4)); VU_UNDO(a_); %s }' % (s.vi(it), imm11, ' '.join('m_[%d] = vu_u(%s);' % (c, fv(fs, c)) for c in cs0)))
            return None
        if op == 0x09:                                                       # ISUBIU (MGS2; as vu0.h)
            imm = ((w >> 21) & 15) << 11 | (w & 0x7ff)
            if it: out.append('  %s = (%s - %du) & 0xffff;' % (s.wvi(it), s.vi(is_), imm))
            return None
        if op == 0x1a:
            if it: out.append('  %s = (VMAC & %s) & 0xffff;' % (s.wvi(it), s.vi(is_)))
            return None
        if op == 0x20: return '1'
        if op == 0x28: return '%s == %s' % (s.vi(it), s.vi(is_))
        if op == 0x29: return '%s != %s' % (s.vi(it), s.vi(is_))
        if op == 0x2c: return '(int16_t)%s < 0' % s.vi(is_)
        if op == 0x2d: return '(int16_t)%s > 0' % s.vi(is_)
        if op == 0x2e: return '(int16_t)%s <= 0' % s.vi(is_)
        if op == 0x2f: return '(int16_t)%s >= 0' % s.vi(is_)
        if op >= 0x40:
            fn = w & 0x3f; i5 = (w >> 6) & 31
            if i5 & 16: i5 -= 32
            if fn == 0x30:
                if id_: out.append('  %s = (%s + %s) & 0xffff;' % (s.wvi(id_), s.vi(is_), s.vi(it)))
                return None
            if fn == 0x31:
                if id_: out.append('  %s = (%s - %s) & 0xffff;' % (s.wvi(id_), s.vi(is_), s.vi(it)))
                return None
            if fn == 0x32:
                if it: out.append('  %s = (%s + (%d)) & 0xffff;' % (s.wvi(it), s.vi(is_), i5))
                return None
            if fn == 0x34:
                if id_: out.append('  %s = (%s & %s) & 0xffff;' % (s.wvi(id_), s.vi(is_), s.vi(it)))
                return None
            if fn == 0x35:
                if id_: out.append('  %s = (%s | %s) & 0xffff;' % (s.wvi(id_), s.vi(is_), s.vi(it)))
                return None
            idx, sel, fsf, ftf = (w >> 6) & 31, fn & 3, (w >> 21) & 3, (w >> 23) & 3
            cs = [c for c in range(4) if dest & (8 >> c)]
            if fn >= 0x3c:
                if sel == 0 and idx == 12:                                   # MOVE
                    if ft and cs:
                        vals = [(c, fv(fs, c), True if fs == 0 else st.form(fs, c)) for c in cs]   # a copy keeps the source's form
                        if fs: s.vf_used.add(fs)
                        s.wvf(ft); out.append('  ' + ' '.join('%s = %s;' % (fv(ft, c), v) for c, v, n in vals))
                        for c, v, n in vals: st.norm[(ft, c)] = n
                    return None
                if sel == 1 and idx == 12:                                   # MR32
                    if ft and cs:
                        vals = [(c, fv(fs, (c + 1) & 3), True if fs == 0 else st.form(fs, (c + 1) & 3)) for c in cs]
                        if fs: s.vf_used.add(fs)
                        s.wvf(ft); out.append('  { ' + ' '.join('float m%d_ = %s;' % (c, v) for c, v, n in vals) + ' ' + ' '.join('%s = m%d_;' % (fv(ft, c), c) for c, v, n in vals) + ' }')
                        for c, v, n in vals: st.norm[(ft, c)] = n
                    return None
                if sel == 0 and idx == 13:                                   # LQI
                    a = s.vi(is_)
                    if ft and cs:
                        # raw bits from the micro memory (form {'L'}): the fast form checks a component at its first float read (VU_LCHK: a bad
                        # value abandons the call, the exact form reruns it; lanes never read as floats, e.g. integers, are not checked) and then
                        # reads it as it is ({'K'}); the exact form maps it at each read (VU_INE)
                        s.wvf(ft); out.append('  { const uint32_t *m_ = (const uint32_t *)(VUMEM + ((%s & 0xff) << 4)); %s }' % (a, ' '.join('%s = vu_f(m_[%d]);' % (fv(ft, c), c) for c in cs)))
                        for c in cs: st.norm[(ft, c)] = frozenset({'L'})
                    if is_: out.append('  %s = (%s + 1) & 0xffff;' % (s.wvi(is_), a))
                    return None
                if sel == 1 and idx == 13:                                   # SQI
                    a = s.vi(it)
                    if fs: s.vf_used.add(fs)
                    if cs: out.append('  { uint32_t *m_ = (uint32_t *)(VUMEM + ((%s & 0xff) << 4)); VU_UNDO(%s); %s }' % (a, a, ' '.join('m_[%d] = vu_u(%s);' % (c, fv(fs, c)) for c in cs)))
                    if it: out.append('  %s = (%s + 1) & 0xffff;' % (s.wvi(it), a))
                    return None
                if sel == 0 and idx == 14:                                   # DIV
                    s.q = True
                    if fs: s.vf_used.add(fs)
                    if ft: s.vf_used.add(ft)
                    out.append('  { float a_ = %s, b_ = %s; VSTAT &= ~0x30u; if (b_ == 0.0f) { if (a_ == 0.0f) VSTAT |= 0x10 | 0x400; else VSTAT |= 0x20 | 0x800; '
                               'q_ = vu_f(((vu_u(%s) ^ vu_u(%s)) & 0x80000000u) | 0x7f7fffffu); } else q_ = VU_CLR(a_ / b_); }'
                               % (s.rd(fs, fsf, st), s.rd(ft, ftf, st), fv(fs, fsf), fv(ft, ftf)))
                    st.q = True; return None
                if sel == 1 and idx == 14:                                   # SQRT (MGS2; as vu0.h: Q = sqrt(|t|), no clamp; t < 0 sets I)
                    s.q = True
                    if ft: s.vf_used.add(ft)
                    out.append('  { float b_ = %s; VSTAT &= ~0x30u; if (b_ < 0) VSTAT |= 0x10 | 0x400; q_ = sqrtf(fabsf(b_)); }' % s.rd(ft, ftf, st))
                    st.q = True; return None
                if sel == 2 and idx == 14:                                   # RSQRT (MGS2; as vu0.h)
                    s.q = True
                    if fs: s.vf_used.add(fs)
                    if ft: s.vf_used.add(ft)
                    out.append('  { float a_ = %s, t_ = %s, b_ = fabsf(t_); VSTAT &= ~0x30u; if (t_ < 0) VSTAT |= 0x10 | 0x400; '
                               'if (b_ == 0.0f) { VSTAT |= 0x20 | 0x800; q_ = vu_f((vu_u(%s) & 0x80000000u) | 0x7f7fffffu); } else q_ = VU_CLR(a_ / sqrtf(b_)); }'
                               % (s.rd(fs, fsf, st), s.rd(ft, ftf, st), fv(fs, fsf)))
                    st.q = True; return None
                if sel == 3 and idx == 14: return None                       # WAITQ
                if sel == 0 and idx == 15:                                   # MTIR
                    if fs: s.vf_used.add(fs)
                    if it: out.append('  %s = vu_u(%s) & 0xffff;' % (s.wvi(it), fv(fs, fsf)))
                    return None
                if sel == 1 and idx == 15:                                   # MFIR
                    if ft and cs:
                        s.wvf(ft); out.append('  { float v_ = vu_f((uint32_t)(int32_t)(int16_t)%s); %s }' % (s.vi(is_), ' '.join('%s = v_;' % fv(ft, c) for c in cs)))
                        for c in cs: st.norm[(ft, c)] = (is_ == 0)   # from vi0: +0.0f, already in VU float form (the programs clear registers this way)
                    return None
        if w == 0x8000033c: return None
        raise SystemExit('unsupported lower op %08x at %d' % (w, pc))

out = ['/* generated by model/vurecomp.py: AOT-compiled VU0 micro programs (images collected with -DVU_DUMP); see vu0.h vu_run */',
       '#ifndef GEN_VU_H', '#define GEN_VU_H',
       'static inline float vu_f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }',
       'static inline uint32_t vu_u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }',
       '/* the VU float model on a value held as a float: vin (inputs) and vclamp (results) are the same mapping */',
       'static __attribute__((noinline)) float vu_in_slow(float f) { uint32_t u = vu_u(f), e = u & 0x7f800000u; if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; return vu_f(u); }',
       '/* fast path on the FPU only (no float <-> integer register moves): +-0 and denormals give a signed zero (x * 0), normal finite values pass,',
       '   Inf / NaN patterns take the bit-exact slow path */',
       '#if defined(VU_IN_FPU)   /* the PSP build picks the faster form on the hardware (same results) */',
       'static inline __attribute__((always_inline)) float vu_in(float f) { if (f == 0.0f) return f * 0.0f; float a = fabsf(f); if (a >= 1.17549435e-38f && a <= 3.40282347e38f) return f; return vu_in_slow(f); }',
       '#else   /* the integer test (x86; on the PSP too: measured in the emulator profile, a range test with an out-of-line slow path costs more: the call forces spills) */',
       'static inline __attribute__((always_inline)) float vu_in(float f) { uint32_t u = vu_u(f), e = u & 0x7f800000u; if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; return vu_f(u); }',
       '#endif',
       '/* the result clamp.  PSP: the FPU runs with flush-to-zero (psp_bench: FCR31 FS), so with inputs in VU float form a result is never a',
       '   denormal (measured: a denormal result is flushed, x + 0 returns x unchanged, a negative underflow keeps its sign) and only',
       '   overflow (Inf) and NaN need the mapping to +-Fmax.  Elsewhere the full input mapping (denormal -> signed zero too). */',
       '#if defined(VU_CL_INF)   /* dev: the Inf / NaN half only (equivalent over the 900-frame reference; the full form is kept: a denormal result occurs 0.03% of the time under IEEE) */',
       'static inline __attribute__((always_inline)) float vu_cl(float f) { uint32_t u = vu_u(f); if ((u & 0x7f800000u) == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; return vu_f(u); }',
       '#elif defined(VU_CL_COUNT)   /* dev: counts the denormal results the full form maps to zero */',
       'static unsigned long g_vu_cl_den, g_vu_cl_n; static uint32_t g_vu_cl_last;',
       'static inline float vu_cl(float f) { uint32_t u = vu_u(f); g_vu_cl_n++; if (!(u & 0x7f800000u) && (u & 0x7fffffu)) { g_vu_cl_den++; g_vu_cl_last = u; } return vu_in(f); }',
       '#else',
       '#define vu_cl vu_in',
       '#endif',
       'static inline __attribute__((always_inline)) float vu_clf(float f, int sh) { return vu_f(vout(f, sh)); }   /* + MAC bits (g_vmac) */',
       'static inline __attribute__((always_inline)) float vu_max(float a, float b) { return a > b ? a : b; }',
       'static inline __attribute__((always_inline)) float vu_min(float a, float b) { return a < b ? a : b; }',
       'static inline __attribute__((always_inline)) float vu_ftoi(float v) { return vu_f(v >= 2147483648.0f ? 0x7fffffffu : v <= -2147483648.0f ? 0x80000000u : (uint32_t)(int32_t)v); }']
def emit_fn(nm, h, g, body, ents):
    """one C function of an image (all entries, or one entry when split); same code as the original single-function form"""
    vfu = sorted(x for x in g.vf_used if x); vfw = sorted(x for x in g.vf_written if x); viu = sorted(g.vi_used); viw = sorted(g.vi_written)
    fn = ['static __attribute__((noinline)) int VUCN(%s)(int pc)\n{' % nm, '  VU_FAST_BEGIN(); int bt; (void)bt; vu_calls++;']
    # register components the function never assigns are read from VF at each use (the compiler keeps or reloads them: no entry copy and no
    # stack spill of those, and no store back of an unchanged value); the assigned ones live in locals loaded at entry and stored at the exit
    import re as re_
    asg = {(int(m.group(1)), 'xyzw'.index(m.group(2))) for ln in body for m in re_.finditer(r'\bf(\d+)([xyzw]) = ', ln)}
    ro = {(r, c) for r in vfu for c in range(4) if (r, c) not in asg}
    if ro:
        pat = re_.compile(r'\bf(\d+)([xyzw])\b')
        def sub_ro(m):
            rc = (int(m.group(1)), 'xyzw'.index(m.group(2)))
            return 'vu_f(VF[%d].u[%d])' % rc if rc in ro else m.group(0)
        body = [pat.sub(sub_ro, ln) for ln in body]
    lv = [(r, c) for r in vfu for c in range(4) if (r, c) not in ro]
    if lv: fn.append('  float ' + ', '.join('%s = vu_f(VF[%d].u[%d])' % (fv(r, c), r, c) for r, c in lv) + ';')
    if g.acc: fn.append('  float ' + ', '.join('acc%d_ = vu_f(VACC[%d])' % (c, c) for c in range(4)) + ';')
    if g.q: fn.append('  float q_ = vu_f(VQr);')
    if g.ireg: fn.append('  uint32_t ireg_ = VIreg;')
    if viu: fn.append('  uint32_t ' + ', '.join('vi%d = VIr[%d] & 0xffff' % (n, n) for n in viu) + ';')
    # the fast form's entry check of the components read before written (VU_ECHK1 per value: vu0.h; the exact form maps them at each read)
    if g.ine: fn.append('  { VU_ECHK_BEGIN(); ' + ' '.join('VU_ECHK1(%s);' % (fv(r, c) if (r, c) not in ro else 'vu_f(VF[%d].u[%d])' % (r, c)) for r, c in sorted(g.ine)) + ' VU_ECHK_END(); }')
    fn.append('  switch (pc) {' + ''.join(' case %d: goto L%d;' % (e, e) for e in ents) + ' }\n  return 1;')
    fn += body
    fn.append('X_:')
    syn = [fv(r, c) for r in vfw for c in range(4) if (r, c) not in ro] + (['acc%d_' % c for c in range(4)] if g.acc else []) + (['q_'] if g.q else [])
    fn.append('  ' + ' '.join('VU_SYNC(%s);' % v for v in syn) + ' if (VU_FAST_BAD()) return 0;')
    for r in vfw: fn.append('  ' + ' '.join('VF[%d].u[%d] = vu_u(%s);' % (r, c, fv(r, c)) for c in range(4) if (r, c) not in ro))
    if g.acc: fn.append('  ' + ' '.join('VACC[%d] = vu_u(acc%d_);' % (c, c) for c in range(4)))
    if g.q: fn.append('  VQr = vu_u(q_);')
    if g.ireg: fn.append('  VIreg = ireg_;')
    for n in viw: fn.append('  VIr[%d] = vi%d;' % (n, n))
    fn.append('  return 1;\n}')
    return fn
hdr_n = len(out)
disp = []; vfpu_out = []; vfpu_disp = []
for h in sorted(entries):
    w = struct.unpack('<1024I', open(os.path.join(d, h + '.bin'), 'rb').read())
    lo = [w[2 * i] for i in range(512)]; hi = [w[2 * i + 1] for i in range(512)]
    kind = {}; tgt = {}
    for i in range(512):
        if hi[i] >> 31: kind[i] = 'n'; continue
        op = lo[i] >> 25
        if op in BR: kind[i] = 'b'; tgt[i] = (i + 1 + simm11(lo[i])) & 511
        elif op in JR: kind[i] = 'j'
        else: kind[i] = 'n'
    E = {i for i in range(512) if (hi[i] >> 30) & 1}
    blk = {}; st_ = list(entries[h])
    while st_:
        i = st_.pop()
        if i in blk: continue
        if kind[i] == 'j': raise SystemExit('%s: JR at %d not supported' % (h, i))
        if i in E:
            assert kind[i] == 'n' and kind[i + 1] == 'n', (h, i); blk[i] = ([i, i + 1], []); continue
        if kind[i] == 'b':
            assert kind[i + 1] == 'n' and i + 1 not in E, (h, i)
            nx = [tgt[i]] if (lo[i] >> 25) in (0x20, 0x21) else [tgt[i], i + 2]
            blk[i] = ([i, i + 1], nx); st_ += nx; continue
        blk[i] = ([i], [i + 1]); st_.append(i + 1)
    # MAC liveness (backward)
    rdf = lambda p: kind[p] == 'n' and not (hi[p] >> 31) and (lo[p] >> 25) in FLAG_READ
    wrf = lambda p: writes_flags(hi[p] & 0x07ffffff)
    def scan(b, live_out):
        live = live_out; fl = {}
        for p in reversed(blk[b][0]):
            fl[p] = 1 if wrf(p) and (rdf(p) or live) else 0
            live = False if wrf(p) else (rdf(p) or live)
        return live, fl
    live_in = {b: False for b in blk}; ch = True
    while ch:
        ch = False
        for b in blk:
            v = scan(b, any(live_in[x] for x in blk[b][1]))[0]
            if v != live_in[b]: live_in[b] = v; ch = True
    # one block: code + out state
    def emit_block(b, st, g):
        st = st.copy(); code = ['L%d:' % b]; fl = scan(b, any(live_in[x] for x in blk[b][1]))[1]
        def pair(p):
            g.upper(hi[p] & 0x07ffffff, fl[p], st, code)
            if hi[p] >> 31:
                if hasattr(g, 'set_ireg'): g.set_ireg(code, lo[p])
                else: g.ireg = True; code.append('  ireg_ = 0x%08xu;' % lo[p])
                st.I = lo[p]; return None
            return g.lower(lo[p], p, st, code)
        if kind[b] == 'b':
            cond = pair(b); code.append('  bt = (%s);' % cond)
            pair(b + 1)
            code.append('  goto L%d;' % tgt[b] if len(blk[b][1]) == 1 else '  if (bt) goto L%d; goto L%d;' % (tgt[b], b + 2))
        else:
            pair(b)
            if b in E: pair(b + 1); code.append('  vu_tpc = %d; goto X_;' % (b + 2))
            else: code.append('  goto L%d;' % (b + 1))
        return code, st
    # forward dataflow of the value-form knowledge
    ins = {}; ent = St({(r, c): frozenset({(r, c)}) for r in range(1, 32) for c in range(4)}); ent.I = None
    for e in entries[h]: ins[e] = ent.copy()
    work = sorted(entries[h]); g0 = Gen()
    while work:
        b = work.pop()
        _, o = emit_block(b, ins[b], g0)
        for s_ in blk[b][1]:
            n = o if s_ not in ins else ins[s_].meet(o)
            if s_ not in ins or n.key() != ins[s_].key(): ins[s_] = n; work.append(s_)
    # per-entry functions (VU_SPLIT, default on): when the entries reach mostly disjoint code, each entry gets its own function holding only
    # the registers its paths use, checking only the ones they read first, and writing back only the ones they write (the same semantics:
    # the all-entries form loads every register the program uses and writes back the unchanged ones too); smaller code, fewer spills
    def reach(e):
        seen = set(); st2 = [e]
        while st2:
            b = st2.pop()
            if b in seen: continue
            seen.add(b); st2 += blk[b][1]
        return seen
    rsets = {e: reach(e) for e in entries[h]}
    split = not os.environ.get('VU_NOSPLIT') and len(rsets) > 1 and sum(len(v) for v in rsets.values()) <= 1.25 * len(blk)
    parts = [(e, rsets[e]) for e in sorted(entries[h])] if split else [(None, set(blk))]
    def ins_for(e):
        if e is None: return ins
        i2 = {e: ent.copy()}; wk = [e]; g1 = Gen()
        while wk:
            b = wk.pop(); _, o = emit_block(b, i2[b], g1)
            for s_ in blk[b][1]:
                n = o if s_ not in i2 else i2[s_].meet(o)
                if s_ not in i2 or n.key() != i2[s_].key(): i2[s_] = n; wk.append(s_)
        return i2
    for e_, bset in parts:
        nm = h if e_ is None else '%s_e%d' % (h, e_)
        ins_e = ins_for(e_)
        g = Gen(); body = []
        for b in sorted(bset): body += emit_block(b, ins_e[b], g)[0]
        out += emit_fn(nm, h, g, body, [e_] if e_ is not None else sorted(entries[h]))
    disp.append((h, [e_ for e_, _ in parts]))
    g = Gen(); body = []
    for b in sorted(blk): body += emit_block(b, ins[b], g)[0]
    vfu = sorted(x for x in g.vf_used if x); vfw = sorted(x for x in g.vf_written if x); viu = sorted(g.vi_used); viw = sorted(g.vi_written)
    # (vfu: the registers of the whole program, for the VFPU form below)
    # ---- VFPU form (PSP; vuvfpu.py) ----
    vcode = None
    try:
        gv = vuvfpu.GenV(set(vfu)); vbody = []
        for b in sorted(blk): vbody += emit_block(b, ins[b], gv)[0]
        gv = vuvfpu.GenV(set(vfu), gv.lqi_regs); vbody = []      # second pass: the registers loaded raw are known
        for b in sorted(blk): vbody += emit_block(b, ins[b], gv)[0]
        if gv.vf_used - set(vfu) - {0}: raise vuvfpu.Unsupported('register set changed')
        vviu = sorted(gv.vi_used); vviw = sorted(gv.vi_written); vvfw = sorted(x for x in gv.vf_written if x)
        vf_ = ['static int vucv_%s(int pc)\n{' % h, '  VU_FAST_BEGIN(); int bt; (void)bt; vu_calls++;']
        if gv.ireg: vf_.append('  uint32_t ireg_ = VIreg;')
        if vviu: vf_.append('  uint32_t ' + ', '.join('vi%d = VIr[%d] & 0xffff' % (n, n) for n in vviu) + ';')
        if gv.ine: vf_.append('  VU_ECHK(' + ' | '.join('vu_ebad(VF[%d].u[%d])' % rc for rc in sorted(gv.ine)) + ');')
        ld = ''.join('lv.q %s, %d(%%0)\\n\\t' % (vuvfpu.quad(gv.q[r]), 16 * r) for r in vfu)
        vf_.append('  __asm__ volatile("%s" : : "r"(VF) : "memory");' % ld.rstrip('\\n\\t'))
        if gv.acc: vf_.append('  __asm__ volatile("lv.q %s, 0(%%0)" : : "r"(VACC) : "memory");' % vuvfpu.quad(gv.qacc))
        if gv.qreg: vf_.append('  __asm__ volatile("mtv %%0, %s" : : "r"(VQr));' % vuvfpu.single(gv.qq, 0))
        if gv.ireg: vf_.append('  __asm__ volatile("mtv %%0, %s" : : "r"(VIreg));' % vuvfpu.single(gv.qi, 0))
        vf_.append('  switch (pc) {' + ''.join(' case %d: goto L%d;' % (e, e) for e in sorted(entries[h])) + ' }\n  return 1;')
        vf_ += vbody
        vf_.append('X_:')
        # the exit check: only the lanes the program writes (the others may hold non-float data, exactly as in the exact form)
        chk = [(vuvfpu.quad(gv.q[r]), gv.wmask[r]) for r in vvfw if gv.wmask.get(r)] + ([(vuvfpu.quad(gv.qacc), gv.accmask)] if gv.acc else []) + ([(vuvfpu.quad(gv.qq), 1)] if gv.qreg else [])
        for i_, (q, m_) in enumerate(chk):
            pfx = '' if m_ == 15 else ('vpfxs ' + ','.join('xyzw'[c] if m_ & (1 << c) else '0' for c in range(4)) + '\\n\\t')
            vf_.append('  g_vf_chk = %d; __asm__ goto(".set push\\n\\t.set noreorder\\n\\t%svcmp.q ES, %s, %s\\n\\tnop\\n\\tbvt 4, %%l[bad_]\\n\\tnop\\n\\t.set pop" : : : : bad_);' % (i_, pfx, q, q))
        vf_.append('  if (VU_FAST_BAD()) { g_vf_chk = -1; VU_VFPU_BADREPORT(0x%su, pc, g_vf_chk, 0); return 0; }' % h)
        vf_.append('  if (0) { bad_: { static VQ bq_[%d]; __asm__ volatile("%s" : : "r"(bq_) : "memory"); VU_VFPU_BADREPORT(0x%su, pc, g_vf_chk, &bq_[g_vf_chk]); } vu_fast_undo(); return 0; }' %
                   (len(chk), '\\n\\t'.join('sv.q %s, %d(%%0)' % (q, 16 * i_) for i_, (q, m_) in enumerate(chk)), h))
        st_ = ''.join('sv.q %s, %d(%%0)\\n\\t' % (vuvfpu.quad(gv.q[r]), 16 * r) for r in vvfw)
        if vvfw: vf_.append('  __asm__ volatile("%s" : : "r"(VF) : "memory");' % st_.rstrip('\\n\\t'))
        if gv.acc: vf_.append('  __asm__ volatile("sv.q %s, 0(%%0)" : : "r"(VACC) : "memory");' % vuvfpu.quad(gv.qacc))
        if gv.qreg: vf_.append('  __asm__ volatile("mfv %%0, %s" : "=r"(VQr));' % vuvfpu.single(gv.qq, 0))
        if gv.ireg: vf_.append('  VIreg = ireg_;')
        for n in vviw: vf_.append('  VIr[%d] = vi%d;' % (n, n))
        vf_.append('  return 1;\n}')
        vcode = vf_
        print('%s: VFPU form, %d quads' % (h, gv.qi + 1))
    except vuvfpu.Unsupported as e:
        print('%s: no VFPU form (%s)' % (h, e))
    if vcode: vfpu_out += ['#ifdef VU_VFPU'] + vcode + ['#endif']; vfpu_disp.append(h)
    print('%s: %d blocks, VF regs %d, VI regs %d' % (h, len(blk), len(vfu), len(viu)))
body = out[hdr_n:]; out = out[:hdr_n]
vfpu_out.append('#ifdef VU_VFPU\nstatic int vucv_dispatch(uint32_t hash, int pc)\n{\n  switch (hash) {\n' +
                ''.join('  case 0x%su: return vucv_%s(pc);\n' % (h, h) for h in vfpu_disp) + '  }\n  return 0;\n}\n#endif')
def disp_case(h, es):
    if es == [None]: return '  case 0x%su: return VUCN(%s)(pc);\n' % (h, h)
    return '  case 0x%su: switch (pc) {%s } return 0;\n' % (h, ''.join(' case %d: return VUCN(%s_e%d)(pc);' % (e, h, e) for e in es))
body.append('static int VUCN(dispatch)(uint32_t hash, int pc)\n{\n  switch (hash) {\n' + ''.join(disp_case(h, es) for h, es in disp) + '  }\n  return 0;\n}')
out.append('static int vuc_entry(uint32_t hash, int pc)\n{\n  switch (hash) {\n' +
           ''.join('  case 0x%su: switch (pc) { %s return 1; } return 0;\n' % (h, ' '.join('case %d:' % e for e in sorted(entries[h]))) for h, es in disp) +
           '  }\n  return 0;\n}')
out += ['/* exact form: the VU float model on every result */',
        '#define VUCN(n) vuc_##n', '#define VU_CLR(x) vu_in(x)', '#define VU_UNDO(a)', '#define VU_FAST_BEGIN()', '#define VU_FAST_BAD() 0', '#define VU_SYNC(x)', '#define VU_INE(x) vu_in(x)', '#define VU_ECHK(...)', '#define VU_ECHK_BEGIN()', '#define VU_ECHK1(x)', '#define VU_ECHK_END()', '#define VU_LCHK(u)',
        '#include "gen_vu_body.h"',
        '#undef VUCN', '#undef VU_CLR', '#undef VU_UNDO', '#undef VU_FAST_BEGIN', '#undef VU_FAST_BAD', '#undef VU_SYNC', '#undef VU_INE', '#undef VU_ECHK', '#undef VU_ECHK_BEGIN', '#undef VU_ECHK1', '#undef VU_ECHK_END', '#undef VU_LCHK',
        '#ifdef VU_FAST',
        '/* fast form (vu0.h VU_FAST): plain float results, valid when the FPU raised no overflow / underflow (then every result was a normal',
        '   value or an exact zero, which the VU float model maps to itself); otherwise it returns 0 with the VU state as on entry (micro memory stores undone,',
        '   status / MAC / clip restored) and the caller runs the exact form */',
        '#define VUCN(n) vucf_##n', '#define VU_CLR(x) (x)', '#define VU_UNDO(a) vu_undo_q(a)', '#define VU_FAST_BEGIN() vu_fast_begin()', '#define VU_FAST_BAD() vu_fast_bad()', '#define VU_SYNC(x) VU_FSYNC(x)', '#define VU_INE(x) (x)', '#define VU_ECHK(...) do { if (__VA_ARGS__) return vu_fast_abort(); } while (0)', '#define VU_LCHK(x) do { if (__builtin_expect(vu_ebad(vu_u(x)), 0)) return vu_fast_fail(); } while (0)', '#if defined(__psp__)   /* PSP: the check on the FPU flags (vu0.h vu_fchk: x * 1 underflows for a denormal, x * 0 and an ordered compare are invalid for Inf / NaN), no integer copies of the values */\n#define VU_ECHK_BEGIN() const float one_ = 1.0f, zero_ = 0.0f\n#define VU_ECHK1(x) vu_fchk((x), one_, zero_)\n#define VU_ECHK_END() do { if (vu_fcr_get() & 0x48u) return vu_fast_abort(); } while (0)\n#else\n#define VU_ECHK_BEGIN() uint32_t eb_ = 0\n#define VU_ECHK1(x) (eb_ |= vu_ebad(vu_u(x)))\n#define VU_ECHK_END() do { if (eb_) return vu_fast_abort(); } while (0)\n#endif',
        '#include "gen_vu_body.h"',
        '#undef VUCN', '#undef VU_CLR', '#undef VU_UNDO', '#undef VU_FAST_BEGIN', '#undef VU_FAST_BAD', '#undef VU_SYNC', '#undef VU_INE', '#undef VU_ECHK', '#undef VU_ECHK_BEGIN', '#undef VU_ECHK1', '#undef VU_ECHK_END', '#undef VU_LCHK',
        '#endif',
        '/* VFPU form (PSP, vu0.h VU_VFPU): the micro programs on the VFPU (model/vuvfpu.py); the FPU fast form\'s entry / exit checks */',
        '#define VU_FAST_BEGIN() vu_fast_begin()', '#define VU_FAST_BAD() vu_fast_bad()', '#define VU_ECHK(...) do { if (__VA_ARGS__) return vu_fast_abort(); } while (0)'] + vfpu_out + [
        '#undef VU_FAST_BEGIN', '#undef VU_FAST_BAD', '#undef VU_ECHK',
        '#endif']
open(os.path.join(OUT, 'gen_vu_body.h'), 'w').write('/* generated by model/vurecomp.py: the AOT-compiled micro programs, included twice by gen_vu.h (exact / fast form) */\n' + '\n'.join(body) + '\n')
open(os.path.join(OUT, 'gen_vu.h'), 'w').write('\n'.join(out) + '\n')
print('gen_vu.h: %d images (%s)' % (len(disp), ', '.join('%s: %s' % (h, 'one function' if es == [None] else '%d entry functions' % len(es)) for h, es in disp)))
