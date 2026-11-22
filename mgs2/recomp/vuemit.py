"""VU0 upper-op decode (mirrors demo_src/vu0.h vu_upper_fl) and the specialized C of a COP2 macro-mode upper op (EE code: model/recomp.py).
The macro op works on the global VU state (VF / VACC / VQr / VIreg); the VU float model (vin on every input, vclamp on every result) is kept.
Only ops whose MAC / status flags are dead are specialized here (recomp.py: no cfc2 of MAC / status in the function); CLIP and the
lower-type macro ops stay on the generic decoder (VUM0)."""
import os
ADD, SUB, MADD, MSUB, MAX, MINI, MUL, OPMSUB, OPMULA, ABS, ITOF, FTOI, CLIP, NOP = range(14)
VFPU_MACRO = os.environ.get('VU_VFPU_MACRO') == '1'   # measurement prototype only (macro_vfpu)
VFPU_GROUP = os.environ.get('VU_VFPU') == '1'          # runs of macro ops on the VFPU, checked (vfpu_group, recomp.py vu_post)
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
def mk_reader(norm, vn, pre):
    """reads of a VF component: in VU float form already (norm: the result of an earlier op) -> VF; normalized earlier into the shadow VN
    (vn) -> VN; raw (a load) -> the component is normalized into VN first (pre: the statements emitted before the op), so a register
    read several times after one load is clamped once per component instead of once per read"""
    def rd(r, c):
        if r == 0: return '1.0f' if c == 3 else '0.0f'
        if (r, c) in norm: return 'vu_f(VF[%d].u[%d])' % (r, c)
        if (r, c) not in vn:   # only the component read (a lane never read as a float, e.g. an integer w, is not normalized)
            pre.append('VN[%d].u[%d] = vu_u(vu_in(vu_f(VF[%d].u[%d])));' % (r, c, r, c)); vn.add((r, c))
        return 'vu_f(VN[%d].u[%d])' % (r, c)
    return rd
def macro_lower_c(w, norm, wr, vn=None):
    """C for a lower-type macro op (VMOVE / VMR32 / VLQI / VSQI / VDIV / VSQRT / VRSQRT / VWAITQ / VIADDI on the global VU state), or None
    (VCALLMS and the rest stay on the generic VUM)"""
    fn = w & 0x3f; dest = (w >> 21) & 15; ft = (w >> 16) & 31; fs = (w >> 11) & 31; idx = (w >> 6) & 31; sel = fn & 3
    fsf, ftf = (w >> 21) & 3, (w >> 23) & 3; cs = [c for c in range(4) if dest & (8 >> c)]
    vn = vn if vn is not None else set(); pre = []
    rd = mk_reader(norm, vn, pre)
    if fn == 0x32:
        i5 = idx - 32 if idx & 16 else idx
        return ('{ VIr[%d] = (VI16(%d) + (%d)) & 0xffff; }' % (ft, fs, i5)) if ft else ''
    if fn < 0x3c or not 12 <= idx <= 14: return None
    if idx == 12 and sel in (0, 1):                                            # MOVE / MR32
        if not ft or not cs: return ''
        src = [(c, (c + sel) & 3) for c in cs]
        code = ' '.join('uint32_t m%d_ = VF[%d].u[%d];' % (c, fs, sc) for c, sc in src) + ' ' + ' '.join('VF[%d].u[%d] = m%d_;' % (ft, c, c) for c, sc in src)
        for c, sc in src: wr.append((ft, c, fs == 0 or (fs, sc) in norm))
        return '{ ' + code + ' }'
    if idx == 13 and sel == 0:                                                 # LQI
        code = ''
        if ft and cs: code += '{ const uint32_t *m_ = (const uint32_t *)vmem(VI16(%d)); %s } ' % (fs, ' '.join('VF[%d].u[%d] = m_[%d];' % (ft, c, c) for c in cs))
        for c in cs: wr.append((ft, c, False))
        if fs: code += 'VIr[%d] = (VI16(%d) + 1) & 0xffff;' % (fs, fs)
        return '{ ' + code + ' }'
    if idx == 13 and sel == 1:                                                 # SQI
        code = ''
        if cs: code += '{ uint32_t *m_ = (uint32_t *)vmem(VI16(%d)); %s } ' % (ft, ' '.join('m_[%d] = VF[%d].u[%d];' % (c, fs, c) for c in cs))
        if ft: code += 'VIr[%d] = (VI16(%d) + 1) & 0xffff;' % (ft, ft)
        return '{ ' + code + ' }'
    if idx == 14 and sel == 0:                                                 # DIV
        a_, b_ = rd(fs, fsf), rd(ft, ftf)
        return ('{ ' + ' '.join(pre) + ' float a_ = %s, b_ = %s; VSTAT &= ~0x30u; if (b_ == 0.0f) { if (a_ == 0.0f) VSTAT |= 0x10 | 0x400; else VSTAT |= 0x20 | 0x800; '
                'VQr = ((VF[%d].u[%d] ^ VF[%d].u[%d]) & 0x80000000u) | 0x7f7fffffu; } else VQr = vu_u(vu_cl(a_ / b_)); }' % (a_, b_, fs, fsf, ft, ftf))
    if idx == 14 and sel == 1:                                                 # SQRT
        b_ = rd(ft, ftf)
        return '{ ' + ' '.join(pre) + ' float b_ = %s; VSTAT &= ~0x30u; if (b_ < 0) VSTAT |= 0x10 | 0x400; VQr = vu_u(sqrtf(fabsf(b_))); }' % b_
    if idx == 14 and sel == 2:                                                 # RSQRT
        a_, t_ = rd(fs, fsf), rd(ft, ftf)
        return ('{ ' + ' '.join(pre) + ' float a_ = %s, t_ = %s, b_ = fabsf(t_); VSTAT &= ~0x30u; if (t_ < 0) VSTAT |= 0x10 | 0x400; '
                'if (b_ == 0.0f) { VSTAT |= 0x20 | 0x800; VQr = (VF[%d].u[%d] & 0x80000000u) | 0x7f7fffffu; } else VQr = vu_u(vu_cl(a_ / sqrtf(b_))); }' % (a_, t_, fs, fsf))
    if idx == 14 and sel == 3: return ''                                       # WAITQ
    return None
def macro_vfpu(u, wr):
    """the upper macro op u on the PSP VFPU (recomp with VU_VFPU_MACRO=1; Ffm's consent 2026-10-20: results may differ from the PS2 in
    the last bit), VF / VACC staying in memory (lv.q / sv.q around each op, temporaries C700-C730).  VFPU arithmetic: IEEE single, round to
    nearest, denormal inputs and results flushed to zero; no VU input normalization, no result clamp, no flags (the caller keeps the C form
    when flags are live).  vu_vfpu (vu0.h): the asm on the PSP, and on x86 the same operations in the same order as C with the VFPU's
    rounding (vu_rnadd / vu_rnsub / vu_rnmul), the reference the PSP's state hashes are checked against.  MAX / MINI / ABS keep the exact
    C form (the VFPU's zero-sign and denormal rules for them are not measured).  None: not covered (the C form runs)."""
    opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
    if opn not in (ADD, SUB, MUL, MADD, MSUB, OPMULA, OPMSUB): return None
    cs = [c for c in range(4) if dest & (8 >> c)]
    toacc = u['toacc'] or opn == OPMULA
    if not toacc and not u['tgt']: return ''
    if opn in (OPMULA, OPMSUB) and (fs == 0 or ft == 0 or src != -1 or 3 in cs): return None
    a = []; ld = []
    S, T, A, D, X = 'C700', 'C710', 'C720', 'C730', 'C600'
    pfs = 'vpfxs 0,0,0,1' if fs == 0 else ''
    if fs: ld.append('lv.q %s, %d(%%[vf])' % (S, 16 * fs))
    pft = ''
    if src in (0, 1, 2, 3):
        if ft == 0: pft = 'vpfxt ' + ','.join(['1' if src == 3 else '0'] * 4)
        else: pft = 'vpfxt ' + ','.join(['xyzw'[src]] * 4); ld.append('lv.q %s, %d(%%[vf])' % (T, 16 * ft))
    elif src == 4: pft = 'vpfxt x,x,x,x'; ld.append('lv.s S710, 0(%[q])')
    elif src == 5: pft = 'vpfxt x,x,x,x'; ld.append('lv.s S710, 0(%[i])')
    elif opn != ABS:
        if ft == 0: pft = 'vpfxt 0,0,0,1'
        else: ld.append('lv.q %s, %d(%%[vf])' % (T, 16 * ft))
    dst = '0(%[acc])' if toacc else '%d(%%[vf])' % (16 * u['tgt'])
    mask = '' if len(cs) == 4 else 'vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4))
    if mask or opn in (MADD, MSUB, OPMSUB): pass
    if opn in (MADD, MSUB, OPMSUB): ld.append('lv.q %s, 0(%%[acc])' % A)
    if mask: ld.append('lv.q %s, %s' % (D, dst))
    def P(*xs): return [x for x in xs if x]
    if opn in (ADD, SUB, MUL):
        a += P(pfs, pft, mask, '%s.q %s, %s, %s' % ({ADD: 'vadd', SUB: 'vsub', MUL: 'vmul'}[opn], D, S, T))
    elif opn in (MADD, MSUB):
        a += P(pfs, pft, 'vmul.q %s, %s, %s' % (X, S, T), mask, '%s.q %s, %s, %s' % ('vadd' if opn == MADD else 'vsub', D, A, X))
    elif opn == OPMULA: a += ['vpfxs y,z,x,w', 'vpfxt z,x,y,w', mask or 'vpfxd ,,,m', 'vmul.q %s, %s, %s' % (D, S, T)]
    elif opn == OPMSUB: a += ['vpfxs y,z,x,w', 'vpfxt z,x,y,w', 'vmul.q %s, %s, %s' % (X, S, T), mask or 'vpfxd ,,,m', 'vsub.q %s, %s, %s' % (D, A, X)]
    if opn in (OPMULA, OPMSUB) and not mask: ld.append('lv.q %s, %s' % (D, dst))   # w keeps its old value
    a.append('sv.q %s, %s' % (D, dst))
    for c in cs: wr.append((-1, c, False) if toacc else (u['tgt'], c, False))   # an IEEE result (an overflow is Inf): C readers normalize it
    txt = '\\n\\t'.join(ld + a)
    ops = ', '.join(o for k, o in (('%[vf]', '[vf] "r"(VF)'), ('%[acc]', '[acc] "r"(VACC)'), ('%[q]', '[q] "r"(&VQr)'), ('%[i]', '[i] "r"(&VIreg)')) if k in txt)
    # the x86 form: every lane's result first (a destination can also be a source), then the stores
    def F(r, c): return ('1.0f' if c == 3 else '0.0f') if r == 0 else 'vu_f(VF[%d].u[%d])' % (r, c)
    def Tl(c):
        if src in (0, 1, 2, 3): return F(ft, src)
        if src == 4: return 'vu_f(VQr)'
        if src == 5: return 'vu_f(VIreg)'
        return F(ft, c)
    def ACCl(c): return 'vu_f(VACC[%d])' % c
    ex = []
    for c in cs:
        if opn == ADD: e = 'vu_rnadd(%s, %s)' % (F(fs, c), Tl(c))
        elif opn == SUB: e = 'vu_rnsub(%s, %s)' % (F(fs, c), Tl(c))
        elif opn == MUL: e = 'vu_rnmul(%s, %s)' % (F(fs, c), Tl(c))
        elif opn == MADD: e = 'vu_rnadd(%s, vu_rnmul(%s, %s))' % (ACCl(c), F(fs, c), Tl(c))
        elif opn == MSUB: e = 'vu_rnsub(%s, vu_rnmul(%s, %s))' % (ACCl(c), F(fs, c), Tl(c))
        else:
            pe = 'vu_rnmul(%s, %s)' % (F(fs, (c + 1) % 3), F(ft, (c + 2) % 3))
            e = pe if opn == OPMULA else 'vu_rnsub(%s, %s)' % (ACCl(c), pe)
        ex.append('uint32_t v%d_ = vu_u(%s);' % (c, e))
    dn = 'VACC[%d]' if toacc else 'VF[%d].u[%%d]' % u['tgt']
    cx = ' '.join(ex + ['%s = v%d_;' % (dn % c, c) for c in cs])
    return '{ vu_vfpu(("%s" : : %s : "memory"), ({ %s })); }' % (txt, ops, cx)   # vu_vfpu (vu0.h): regcache.py reads it as a pure call
def vfpu_dec(w):
    """the macro op word w as a VFPU group member (vfpu_group): the decoded op (plus cs: the lanes written, acc: writes ACC), 'nop' when it
    has no effect without flags (no lane, or a VF0 target), None when it stays on the C form (MAX / MINI / ABS / ITOF / FTOI / CLIP, the
    lower-type ops, the OPMULA / OPMSUB forms macro_vfpu leaves out)"""
    fn = w & 0x3f
    if fn == 0x32 or fn in (0x38, 0x39) or 0x30 <= fn <= 0x35 or (fn >= 0x3c and 12 <= ((w >> 6) & 31) <= 16): return None
    u = dec_upper(w & 0x01ffffff)
    if u is None: return 'nop'
    opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
    if opn not in (ADD, SUB, MUL, MADD, MSUB, OPMULA, OPMSUB): return None
    cs = [c for c in range(4) if dest & (8 >> c)]
    acc = bool(u['toacc'] or opn == OPMULA)
    if not cs or (not acc and not u['tgt']): return 'nop'
    if opn in (OPMULA, OPMSUB) and (fs == 0 or ft == 0 or src != -1 or 3 in cs): return None
    return dict(u, cs=cs, acc=acc)
def vfpu_x86(u):
    """the x86 form of one group member: the VFPU's arithmetic (vu0.h vu_rnadd / vu_rnsub / vu_rnmul: IEEE single, round to nearest,
    denormals flushed) on the raw register values, every lane's result first (a destination can also be a source), then the stores"""
    opn, src, ft, fs, cs = u['opn'], u['src'], u['ft'], u['fs'], u['cs']
    def F(r, c): return ('1.0f' if c == 3 else '0.0f') if r == 0 else 'vu_f(VF[%d].u[%d])' % (r, c)
    def Tl(c):
        if src in (0, 1, 2, 3): return F(ft, src)
        if src == 4: return 'vu_f(VQr)'
        if src == 5: return 'vu_f(VIreg)'
        return F(ft, c)
    def ACCl(c): return 'vu_f(VACC[%d])' % c
    ex = []
    for c in cs:
        if opn == ADD: e = 'vu_rnadd(%s, %s)' % (F(fs, c), Tl(c))
        elif opn == SUB: e = 'vu_rnsub(%s, %s)' % (F(fs, c), Tl(c))
        elif opn == MUL: e = 'vu_rnmul(%s, %s)' % (F(fs, c), Tl(c))
        elif opn == MADD: e = 'vu_rnadd(%s, vu_rnmul(%s, %s))' % (ACCl(c), F(fs, c), Tl(c))
        elif opn == MSUB: e = 'vu_rnsub(%s, vu_rnmul(%s, %s))' % (ACCl(c), F(fs, c), Tl(c))
        else:
            pe = 'vu_rnmul(%s, %s)' % (F(fs, (c + 1) % 3), F(ft, (c + 2) % 3))
            e = pe if opn == OPMULA else 'vu_rnsub(%s, %s)' % (ACCl(c), pe)
        ex.append('uint32_t v%d_ = vu_u(%s);' % (c, e))
    dn = 'VACC[%d]' if u['acc'] else 'VF[%d].u[%%d]' % u['tgt']
    return '{ ' + ' '.join(ex + ['%s = v%d_;' % (dn % c, c) for c in cs]) + ' }'
VG_SLOTS = 24   # VFPU column quads C000-C530 for the group's registers; C700 (a constant operand's register), C710 (Q), C720 (I), C730 (products)
def vfpu_group(us, fb):
    """one run of group members us (vfpu_dec dicts, program order, flags dead) as the statement vu_vgrp (vu0.h), or None when it needs
    more than VG_SLOTS registers.  fb: the C forms of the same ops (the exact VU0 model), the fallback.
    PSP: one asm block: the registers the run reads (VF / ACC in memory) into VFPU registers, the ops on them (no memory traffic in between),
    then the check (vcmp ES, a branch on the condition code sets a general register): a written lane that is NaN or infinite means an input the VU
    reads as a huge number (exponent 255) or an overflow, where the VFPU and the VU differ beyond rounding; then nothing has been stored
    yet and the C forms run instead.  Otherwise a second asm block stores the results (the VFPU registers keep their values between the
    two: nothing else uses them and no call comes between): finite and flushed, so already in VU float form.  (Not one asm goto: with
    hot / cold partitioning GCC moves the fallback out of the branch's range.)  NaN / infinity propagate through add / sub / mul, so a
    written value that a later op of the run reads lane by lane into a checked result needs no check of its own.
    x86 (the reference the PSP's hashes are checked against): the same ops in the same order with the VFPU's rounding (vfpu_x86), the same
    check, and on a failed check the registers restored and the C forms run.
    Returns (statement, written): written = {key: lanes}, key a VF number or 'A' (ACC)."""
    keys = []; valid = set(); loads = []; body = []; wl = {}; lastw = {}; idr = []; useq = usei = False
    def slot(k): i = keys.index(k); return 'C%d%d0' % (i // 4, i % 4)
    def need(k):
        if k not in keys: keys.append(k)
        if k not in valid: loads.append(k); valid.add(k)
    for j, u in enumerate(us):
        opn, src, ft, fs, cs = u['opn'], u['src'], u['ft'], u['fs'], u['cs']
        D = 'A' if u['acc'] else u['tgt']
        rd = []                                   # the keys this op reads lane by lane (lane c of the source -> lane c of the result)
        if fs:
            need(fs)
            if opn in (ADD, SUB, MUL, MADD, MSUB): rd.append(fs)
        if src in (0, 1, 2, 3) and ft: need(ft)
        elif src == -1 and ft:
            need(ft)
            if opn in (ADD, SUB, MUL, MADD, MSUB): rd.append(ft)
        elif src == 4: useq = True
        elif src == 5: usei = True
        if opn in (MADD, MSUB, OPMSUB): need('A'); rd.append('A')
        if len(cs) < 4: need(D)                   # the lanes not written keep their value
        elif D not in keys: keys.append(D); valid.add(D)
        else: valid.add(D)
        if len(keys) > VG_SLOTS: return None
        S = slot(fs) if fs else 'C700'
        pfs = '' if fs else 'vpfxs 0,0,0,1'
        if src in (0, 1, 2, 3):
            if ft: T, pft = slot(ft), 'vpfxt ' + ','.join(['xyzw'[src]] * 4)
            else: T, pft = 'C700', 'vpfxt ' + ','.join(['1' if src == 3 else '0'] * 4)
        elif src == 4: T, pft = 'C710', 'vpfxt x,x,x,x'
        elif src == 5: T, pft = 'C720', 'vpfxt x,x,x,x'
        elif ft: T, pft = slot(ft), ''
        else: T, pft = 'C700', 'vpfxt 0,0,0,1'
        Dn = slot(D); mask = '' if len(cs) == 4 else 'vpfxd ' + ','.join('' if c in cs else 'm' for c in range(4))
        def P(*xs): return [x for x in xs if x]
        if opn in (ADD, SUB, MUL): body += P(pfs, pft, mask, '%s.q %s, %s, %s' % ({ADD: 'vadd', SUB: 'vsub', MUL: 'vmul'}[opn], Dn, S, T))
        elif opn in (MADD, MSUB): body += P(pfs, pft, 'vmul.q C730, %s, %s' % (S, T), mask, '%s.q %s, %s, C730' % ('vadd' if opn == MADD else 'vsub', Dn, slot('A')))
        elif opn == OPMULA: body += ['vpfxs y,z,x,w', 'vpfxt z,x,y,w', mask, 'vmul.q %s, %s, %s' % (Dn, S, T)]
        else: body += ['vpfxs y,z,x,w', 'vpfxt z,x,y,w', 'vmul.q C730, %s, %s' % (S, T), mask, 'vsub.q %s, %s, C730' % (Dn, slot('A'))]
        wl.setdefault(D, set()).update(cs); lastw[D] = j; idr.append((rd, D, set(cs)))
    # the checks: a key is covered when a later op whose result is its target's final value reads it lane by lane in all its written lanes
    chk = []; cov = set()
    for k in sorted(wl, key=lambda k: -lastw[k]):
        for j2 in range(lastw[k] + 1, len(us)):
            rd, D2, cs2 = idr[j2]
            if lastw[D2] == j2 and k in rd and wl[k] <= cs2: cov.add(k); break
        else: chk.append(k)
    def mem(k): return '0(%[acc])' if k == 'A' else '%d(%%[vf])' % (16 * k)
    a = ['lv.q %s, %s' % (slot(k), mem(k)) for k in loads]
    if useq: a.append('lv.s S710, 0(%[q])')
    if usei: a.append('lv.s S720, 0(%[i])')
    a += body
    a.insert(0, 'move %[cc], $0')
    for n_, k in enumerate(chk):              # the check: bit 4 of the VFPU condition code (any lane compared is NaN / infinite) ORed into cc
        W = sorted(wl[k]); s = slot(k)
        if W == [0, 1, 2, 3]: a.append('vcmp.q ES, %s, %s' % (s, s))
        elif W == [0, 1, 2]: a.append('vcmp.t ES, %s, %s' % (s, s))
        elif W == [0, 1]: a.append('vcmp.p ES, %s, %s' % (s, s))
        elif W == [0]: a.append('vcmp.s ES, S%s, S%s' % (s[1:], s[1:]))
        else: a += ['vpfxs ' + ','.join('xyzw'[c] if c in W else '0' for c in range(4)), 'vcmp.q ES, %s, %s' % (s, s)]
        # read with a branch on the code (bvtl: the delay slot runs only when taken), not mfvc $131: v12 on the PSP read the code before an
        # in-flight vcmp wrote it (inferred: fewer fallbacks than the emulator, left the reference at vb 16000; hwprobe.c probe 7)
        a += ['bvtl 4, 1f', 'ori %[cc], %[cc], 0x10', '1:']
    st = ['sv.q %s, %s' % (slot(k), mem(k)) for k in sorted(wl, key=keys.index)]
    def opl(txt): return ', '.join(o for n, o in (('%[vf]', '[vf] "r"(VF)'), ('%[acc]', '[acc] "r"(VACC)'), ('%[q]', '[q] "r"(&VQr)'), ('%[i]', '[i] "r"(&VIreg)')) if n in txt)
    ta, tb = '\\n\\t'.join(['.set push', '.set noreorder'] + a + ['.set pop']), '\\n\\t'.join(st)
    def reg(k): return 'VACC' if k == 'A' else 'VF[%d].u' % k
    def sv(k): return 'vu_sv[%d]' % (32 if k == 'A' else k)
    X = '({ %s %s })' % (' '.join('memcpy(%s, %s, 16);' % (sv(k), reg(k)) for k in wl), ' '.join(vfpu_x86(u) for u in us))
    K = '(%s)' % ' && '.join('vu_fin(%s[%d])' % (reg(k), c) for k in chk for c in sorted(wl[k]))
    R = '({ %s })' % ' '.join('memcpy(%s, %s, 16);' % (reg(k), sv(k)) for k in wl)
    return ('vu_vgrp(("%s" : [cc] "=&r"(vcc_) : %s : "memory"), ("%s" : : %s : "memory"), %s, %s, %s, ({ %s }));'
            % (ta, opl(ta), tb, opl(tb), X, K, R, fb)), wl
def macro_c(w, fl=0, norm=None, wr=None, vn=None):
    """C for the macro op word w (fl: compute MAC / status as vu_upper_fl(w, 1)), or None (generic VUM0 / VUM).
    norm: set of (reg, comp) known to hold a value already in VU float form (vin is the identity: the result of an earlier op);
    wr (out): list of (reg, comp, normalized) the op writes (reg -1 = ACC)"""
    norm = norm if norm is not None else set()
    if wr is None: wr = []
    fn = w & 0x3f
    vn = vn if vn is not None else set(); pre = []
    if fn == 0x32 or (fn >= 0x3c and 12 <= ((w >> 6) & 31) <= 14): return macro_lower_c(w, norm, wr, vn)
    if fn in (0x38, 0x39) or 0x30 <= fn <= 0x35: return None
    if fn >= 0x3c and 12 <= ((w >> 6) & 31) <= 16: return None
    u = dec_upper(w & 0x01ffffff)
    if u is None: return ''                      # NOP / unused slot: no effect
    opn, src, dest, ft, fs = u['opn'], u['src'], u['dest'], u['ft'], u['fs']
    if opn == CLIP: return None
    cs = [c for c in range(4) if dest & (8 >> c)]
    if not cs: return '{ g_vmac = 0; vu_setflags(); }' if fl else ''
    if not fl and VFPU_MACRO:
        c = macro_vfpu(u, wr)
        if c is not None: return c
    V = mk_reader(norm, vn, pre)
    def A(c): return V(fs, c)
    def B(c):
        if src in (0, 1, 2, 3): return V(ft, src)
        if src == 4: return 'vu_in(vu_f(VQr))'
        if src == 5: return 'vu_in(vu_f(VIreg))'
        return V(ft, c)
    def ACC(c): return 'vu_f(VACC[%d])' % c if (-1, c) in norm else 'vu_in(vu_f(VACC[%d]))' % c
    code = []
    for c in cs:
        if opn == ADD: e = '%s + %s' % (A(c), B(c))
        elif opn == SUB: e = '%s - %s' % (A(c), B(c))
        elif opn == MADD: e = '%s + %s * %s' % (ACC(c), A(c), B(c))
        elif opn == MSUB: e = '%s - %s * %s' % (ACC(c), A(c), B(c))
        elif opn == MAX: e = 'vu_max(%s, %s)' % (A(c), B(c))
        elif opn == MINI: e = 'vu_min(%s, %s)' % (A(c), B(c))
        elif opn == MUL: e = '%s * %s' % (A(c), B(c))
        elif opn in (OPMSUB, OPMULA):
            p = {0: (1, 2), 1: (2, 0), 2: (0, 1), 3: (0, 1)}[c]
            pe = '%s * %s' % (A(p[0]), V(ft, p[1]))
            e = ('%s - (%s)' % (ACC(c), pe)) if opn == OPMSUB else pe
        elif opn == ABS: e = 'fabsf(%s)' % A(c)
        elif opn == ITOF: e = '(float)(int32_t)VF[%d].u[%d] / %d.0f' % (fs, c, 1 << u['sh'])
        if opn == FTOI: code.append('uint32_t t%d_ = vu_u(vu_ftoi(%s * %d.0f));' % (c, A(c), 1 << u['sh']))
        elif fl: code.append('uint32_t t%d_ = vout(%s, %d);' % (c, e, 3 - c))
        else: code.append('float x%d_ = %s;' % (c, e))
    # the result clamp of the lanes without live flags: VU_MCLn (demo_src/vu0.h): on the PSP the hardware flush (FCR31 FS) gives the
    # VU float model's signed zero for an underflow and only an overflow (the sticky O flag, one check per op) needs the per-lane clamp
    xs = [c for c in cs if not fl and opn != FTOI]
    if xs:
        code.append('VU_MCL%d(%s);' % (len(xs), ', '.join('x%d_' % c for c in xs)))
        code += ['uint32_t t%d_ = vu_u(x%d_);' % (c, c) for c in xs]
    for c in cs:
        if u['toacc'] or opn == OPMULA: code.append('VACC[%d] = t%d_;' % (c, c)); wr.append((-1, c, opn != FTOI))
        elif u['tgt']: code.append('VF[%d].u[%d] = t%d_;' % (u['tgt'], c, c)); wr.append((u['tgt'], c, opn != FTOI))
    if fl: code = ['g_vmac = 0;'] + code + ['vu_setflags();']
    return '{ ' + ' '.join(pre + code) + ' }'
def writes_flags_macro(w):
    """the macro op word writes MAC / status (an upper FMAC op other than CLIP; lower-type macro ops do not)"""
    fn = w & 0x3f
    if fn in (0x38, 0x39) or 0x30 <= fn <= 0x35: return False
    if fn >= 0x3c and 12 <= ((w >> 6) & 31) <= 16: return False
    u = dec_upper(w & 0x01ffffff)
    return u is not None and u['opn'] != CLIP
