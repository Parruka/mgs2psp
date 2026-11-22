"""VFPU form of the AOT-compiled VU0 micro programs (PSP only, demo/gen_vu.h under VU_VFPU): the VF registers live in the VFPU quad registers
(C000..C730: 32 of them, one per VF register the program uses, plus ACC, a temporary, and the Q and I broadcast quads; VF0 is never held:
its (0, 0, 0, 1) comes from the source prefixes), the VI registers in C locals as in the C forms, and every upper op is one VFPU
instruction with at most a source / destination prefix (broadcast, swizzle, negation, lane mask).  Division, square roots and the MAC
flags go through the FPU / integer unit on extracted lanes (mfv / mtv), exactly as the C fast form computes them.
Like the FPU fast form it is valid when no result overflowed; the exit checks every written quad for Inf / NaN (vcmp ES) and the
program returns 0 untouched when one is found (the caller runs the exact form).  Programs with micro memory stores (SQI), CLIP, FTOI,
ITOF, MR32, the integer load / store ops or more registers than the VFPU holds are not generated (vucv_<hash> absent: the fast C form runs).
The generator mirrors vurecomp.Gen (same upper / lower interface, driven by vurecomp's block layout)."""
import vuemit as V
C = 'xyzw'
class Unsupported(Exception): pass
def quad(q): return 'C%d%d0' % (q >> 2, q & 3)
def single(q, c): return 'S%d%d%d' % (q >> 2, q & 3, c)
class GenV:
    def __init__(s, vf_used, raw_regs=()):
        """vf_used: the VF registers (1..31) the program touches; quads are allotted in that order, then ACC, T, Q, I"""
        s.q = {}
        for r in sorted(vf_used):
            if r: s.q[r] = len(s.q)
        s.qacc = len(s.q); s.qt = s.qacc + 1; s.qq = s.qt + 1; s.qi = s.qq + 1
        if s.qi >= 32: raise Unsupported('too many quads')
        s.vf_used = set(vf_used); s.vf_written = set(); s.vi_used = set(); s.vi_written = set(); s.acc = False; s.qreg = False; s.ireg = False; s.ine = set(); s.wmask = {}; s.accmask = 0; s.raw_regs = set(raw_regs); s.lqi_regs = set()
    # ---- operands ----
    def vi(s, n):
        if not n: return '0u'
        s.vi_used.add(n); return 'vi%d' % n
    def wvi(s, n): s.vi_used.add(n); s.vi_written.add(n); return 'vi%d' % n
    def wvf(s, r, cs=(0, 1, 2, 3), raw=False):
        """raw: a load of bit patterns (LQI / MFIR): not a float result, so not part of the exit check (the exact form keeps it raw too)"""
        s.vf_used.add(r); s.vf_written.add(r)
        if raw: s.lqi_regs.add(r)
        else:
            for c in cs: s.wmask[r] = s.wmask.get(r, 0) | (1 << c)
    def precheck(s, out, r, cs):
        """an operand register that may hold raw loaded data, used by MAX / MINI (the VU float model maps Inf / NaN to +-Fmax there, the VFPU
        cannot): abandon the call when one of its lanes is Inf / NaN"""
        if r and r in s.raw_regs:
            pfx = '' if len(cs) == 4 else ('vpfxs ' + ','.join(C[c] if c in cs else '0' for c in range(4)) + '\\n\\t')
            out.append('  __asm__ goto(".set push\\n\\t.set noreorder\\n\\t%svcmp.q ES, %s, %s\\n\\tnop\\n\\tbvt 4, %%l[bad_]\\n\\tnop\\n\\t.set pop" : : : : bad_);' % (pfx, s.Q(r), s.Q(r)))
    def Q(s, r): return quad(s.q[r])
    def asm(s, out, text): out.append('  __asm__ volatile("%s");' % text.replace('\n', '\\n\\t'))
    def lane_get(s, out, var, r, c):
        """C: var = the bits of VF r lane c (r == 0: the constant)"""
        if r == 0: out.append('  %s = %s;' % (var, '0x3f800000u' if c == 3 else '0u')); return
        out.append('  __asm__ volatile("mfv %%0, %s" : "=r"(%s));' % (single(s.q[r], c), var))
    def lane_set(s, out, r, c, expr):
        out.append('  __asm__ volatile("mtv %%0, %s" : : "r"(%s));' % (single(s.q[r], c), expr))
    def upper(s, w, fl, st, out):
        u = V.dec_upper(w)
        if u is None: return
        opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
        if opn in (V.CLIP, V.FTOI, V.ITOF): raise Unsupported('clip/ftoi/itof')
        cs = [c for c in range(4) if dest & (8 >> c)]
        if fl: out.append('  g_vmac = 0;')
        if not cs:
            if fl: out.append('  vu_setflags();')
            return
        if fs: s.vf_used.add(fs)
        if ft and src != 4 and src != 5: s.vf_used.add(ft)
        mask = '' if len(cs) == 4 else ('vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4)) + '\n')
        # the s operand: VF0 -> constants
        pfs = ''
        if fs == 0: pfs = 'vpfxs 0,0,0,1\n'
        SQ = quad(s.q[fs]) if fs else None
        # the t operand (broadcast / Q / I / VF0)
        pft = ''; TQ = None
        if src in (0, 1, 2, 3):
            if ft == 0: pft = 'vpfxt %s\n' % ','.join(['1' if src == 3 else '0'] * 4)
            else: pft = 'vpfxt %s\n' % ','.join([C[src]] * 4); TQ = quad(s.q[ft])
        elif src == 4: s.qreg = True; pft = 'vpfxt x,x,x,x\n'; TQ = quad(s.qq)
        elif src == 5: s.ireg = True; pft = 'vpfxt x,x,x,x\n'; TQ = quad(s.qi)
        else:
            if ft == 0: pft = 'vpfxt 0,0,0,1\n'
            else: TQ = quad(s.q[ft])
        if TQ is None: TQ = SQ if SQ else quad(s.qt)     # a constant prefix needs some register operand
        if SQ is None: SQ = TQ
        ACC = quad(s.qacc); T = quad(s.qt)
        if u['toacc'] or opn == V.OPMULA: D = ACC; s.acc = True; s.accmask |= sum(1 << c for c in cs)
        elif not u['tgt']:
            if not fl: return                            # writes VF0 without flags: no effect
            D = T = quad(s.qt)                           # VF0 as the target: the result exists only for its flags (the temp quad)
            if opn in (V.MADD, V.MSUB, V.OPMSUB): raise Unsupported('flags-only op needing the temp')
        else:
            s.wvf(u['tgt'], cs); D = quad(s.q[u['tgt']])
        code = ''
        if opn == V.ADD: code = pfs + pft + mask + 'vadd.q %s, %s, %s' % (D, SQ, TQ)
        elif opn == V.SUB: code = pfs + pft + mask + 'vsub.q %s, %s, %s' % (D, SQ, TQ)
        elif opn == V.MUL: code = pfs + pft + mask + 'vmul.q %s, %s, %s' % (D, SQ, TQ)
        elif opn in (V.MAX, V.MINI):
            s.precheck(out, fs, cs); s.precheck(out, ft, [src] if src in (0, 1, 2, 3) else cs) if src != 4 and src != 5 else None
            code = pfs + pft + mask + ('vmax.q' if opn == V.MAX else 'vmin.q') + ' %s, %s, %s' % (D, SQ, TQ)
        elif opn in (V.MADD, V.MSUB):
            s.acc = True
            code = pfs + pft + 'vmul.q %s, %s, %s\n' % (T, SQ, TQ) + mask + ('vadd.q' if opn == V.MADD else 'vsub.q') + ' %s, %s, %s' % (D, ACC, T)
        elif opn in (V.OPMULA, V.OPMSUB):
            if fs == 0 or ft == 0 or src != -1: raise Unsupported('opm with vf0 / bc')
            s.acc = True
            if opn == V.OPMULA: code = 'vpfxs y,z,x,w\nvpfxt z,x,y,w\n' + (mask or 'vpfxd ,,,m\n') + 'vmul.q %s, %s, %s' % (ACC, SQ, TQ)
            else: code = 'vpfxs y,z,x,w\nvpfxt z,x,y,w\nvmul.q %s, %s, %s\n' % (T, SQ, TQ) + (mask or 'vpfxd ,,,m\n') + 'vsub.q %s, %s, %s' % (D, ACC, T)
        elif opn == V.ABS: code = pfs + mask + 'vabs.q %s, %s' % (D, SQ)
        else: raise Unsupported('op %d' % opn)
        s.asm(out, code)
        if fl:
            dq = s.qacc if D == ACC else (s.qt if not u['tgt'] else s.q[u['tgt']])
            for c in cs:
                out.append('  { uint32_t u_; __asm__ volatile("mfv %%0, %s" : "=r"(u_)); vout(vu_f(u_), %d); }' % (single(dq, c), 3 - c))
            out.append('  vu_setflags();')
    def lower(s, w, pc, st, out):
        op = w >> 25; dest = (w >> 21) & 15; ft = (w >> 16) & 31; fs = (w >> 11) & 31; fd = (w >> 6) & 31
        it, is_, id_ = ft & 15, fs & 15, fd & 15
        cs = [c for c in range(4) if dest & (8 >> c)]
        if op in (0x00, 0x01, 0x04, 0x05): raise Unsupported('lq/sq/ilw/isw')
        if op == 0x08:
            imm = ((w >> 21) & 15) << 11 | (w & 0x7ff)
            if it: out.append('  %s = (%s + %du) & 0xffff;' % (s.wvi(it), s.vi(is_), imm))
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
            if fn >= 0x3c:
                if sel == 0 and idx == 12:                                   # MOVE
                    if ft and cs:
                        s.vf_used.add(fs) if fs else None; s.wvf(ft, cs)
                        mask = '' if len(cs) == 4 else ('vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4)) + '\n')
                        if fs == 0: s.asm(out, 'vpfxs 0,0,0,1\n' + mask + 'vmov.q %s, %s' % (s.Q(ft), s.Q(ft)))
                        else: s.asm(out, mask + 'vmov.q %s, %s' % (s.Q(ft), s.Q(fs)))
                    return None
                if sel == 1 and idx == 12:                                   # MR32: t.c = s.(c + 1)
                    if ft and cs:
                        s.wvf(ft, cs)
                        mask = '' if len(cs) == 4 else ('vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4)) + '\n')
                        if fs == 0: s.asm(out, 'vpfxs 0,0,1,0\n' + mask + 'vmov.q %s, %s' % (s.Q(ft), s.Q(ft)))
                        else: s.vf_used.add(fs); s.asm(out, 'vpfxs y,z,w,x\n' + mask + 'vmov.q %s, %s' % (s.Q(ft), s.Q(fs)))
                    return None
                if sel == 0 and idx == 13:                                   # LQI
                    a = s.vi(is_)
                    if ft and cs:
                        s.wvf(ft, cs, raw=True)
                        if len(cs) == 4: out.append('  __asm__ volatile("lv.q %s, 0(%%0)" : : "r"(VUMEM + ((%s & 0xff) << 4)) : "memory");' % (s.Q(ft), a))
                        else:
                            mask = 'vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4))
                            out.append('  __asm__ volatile("lv.q %s, 0(%%0)\\n\\t%s\\n\\tvmov.q %s, %s" : : "r"(VUMEM + ((%s & 0xff) << 4)) : "memory");' % (quad(s.qt), mask, s.Q(ft), quad(s.qt), a))
                    if is_: out.append('  %s = (%s + 1) & 0xffff;' % (s.wvi(is_), a))
                    return None
                if sel == 1 and idx == 13: raise Unsupported('sqi')
                if sel == 0 and idx == 14:                                   # DIV (FPU, VU semantics as the C forms)
                    s.qreg = True
                    out.append('  { uint32_t ua_, ub_;'); s.lane_get(out, 'ua_', fs, fsf); s.lane_get(out, 'ub_', ft, ftf)
                    out.append('    float a_ = vu_in(vu_f(ua_)), b_ = vu_in(vu_f(ub_)); uint32_t q_; VSTAT &= ~0x30u;')
                    out.append('    if (b_ == 0.0f) { if (a_ == 0.0f) VSTAT |= 0x10 | 0x400; else VSTAT |= 0x20 | 0x800; q_ = ((ua_ ^ ub_) & 0x80000000u) | 0x7f7fffffu; } else q_ = vu_u(a_ / b_);')
                    out.append('    __asm__ volatile("mtv %%0, %s" : : "r"(q_)); }' % single(s.qq, 0))
                    return None
                if sel == 1 and idx == 14:                                   # SQRT
                    s.qreg = True
                    out.append('  { uint32_t ub_;'); s.lane_get(out, 'ub_', ft, ftf)
                    out.append('    float b_ = vu_in(vu_f(ub_)); VSTAT &= ~0x30u; if (b_ < 0) VSTAT |= 0x10 | 0x400; uint32_t q_ = vu_u(sqrtf(fabsf(b_)));')
                    out.append('    __asm__ volatile("mtv %%0, %s" : : "r"(q_)); }' % single(s.qq, 0))
                    return None
                if sel == 2 and idx == 14:                                   # RSQRT
                    s.qreg = True
                    out.append('  { uint32_t ua_, ub_;'); s.lane_get(out, 'ua_', fs, fsf); s.lane_get(out, 'ub_', ft, ftf)
                    out.append('    float a_ = vu_in(vu_f(ua_)), t_ = vu_in(vu_f(ub_)), b_ = fabsf(t_); uint32_t q_; VSTAT &= ~0x30u; if (t_ < 0) VSTAT |= 0x10 | 0x400;')
                    out.append('    if (b_ == 0.0f) { VSTAT |= 0x20 | 0x800; q_ = (ua_ & 0x80000000u) | 0x7f7fffffu; } else q_ = vu_u(a_ / sqrtf(b_));')
                    out.append('    __asm__ volatile("mtv %%0, %s" : : "r"(q_)); }' % single(s.qq, 0))
                    return None
                if sel == 3 and idx == 14: return None                       # WAITQ
                if sel == 0 and idx == 15:                                   # MTIR
                    if it:
                        out.append('  { uint32_t u_;'); s.lane_get(out, 'u_', fs, fsf); out.append('    %s = u_ & 0xffff; }' % s.wvi(it))
                    return None
                if sel == 1 and idx == 15:                                   # MFIR
                    if ft and cs:
                        s.wvf(ft, cs, raw=True)
                        for c in cs: s.lane_set(out, ft, c, '(uint32_t)(int32_t)(int16_t)%s' % s.vi(is_))
                    return None
        if w == 0x8000033c: return None
        raise Unsupported('lower op %08x' % w)
    def set_ireg(s, out, lo):
        """the I register (the upper word's immediate): the C local and the broadcast quad"""
        s.ireg = True
        out.append('  ireg_ = 0x%08xu; __asm__ volatile("mtv %%0, %s" : : "r"(ireg_));' % (lo, single(s.qi, 0)))
