"""64-bit -> 32-bit narrowing over the recompiled C (recomp.py output, before regcache.py), exact.  The EE compiler writes bit tests and
sign extensions with doubleword shifts whose final result is a sign-extended 32-bit value:
  dsll32 x, s, a ; dsra32 y, x, b      y = sign-extended bits of s, shifted
  dsll32 x, s, a ; dsrl32 y, x, b      y = bits of s (b >= 1: below 2^31), e.g. b = 31: one bit as 0 / 1
A function with one of them needs 64-bit register locals (regcache.py WIDE): about twice the host instructions and register pressure on
the 32-bit PSP.  Here the dsll32 keeps only the high word of its result in x (the low word of the true value is 0):
  x = (int32_t)((uint32_t)s << a)        and each reader       y = (int32_t)x >> b   /   y = (int32_t)((uint32_t)x >> b)
which is exact when every read of such an x is one of those shifts (b >= 1 for dsrl32), and every definition reaching such a read is
such a dsll32 (same x).  Checked per register with a forward dataflow over the function's lines (fallthrough + goto edges; a call or a
return reads every register; any other read of x rejects the register in that function)."""
import re
AD = r'/\*[0-9a-f]+\*/ '
DSLL_E = r'R\[(\d+)\] = \(int64_t\)\(\(uint64_t\)R\[(\d+)\] << (3[2-9]|[4-5]\d|6[0-3])\);'
CONS_E = (r'R\[(\d+)\] = R\[(\d+)\] >> (3[2-9]|[4-5]\d|6[0-3]);', r'R\[(\d+)\] = \(int64_t\)\(\(uint64_t\)R\[(\d+)\] >> (3[3-9]|[4-5]\d|6[0-3])\);')
SLOT = re.compile(r'^' + AD + r'\{ int c_ = \((.*?)\); (.*?) if \(c_\) goto L[0-9a-f]+; \}$')
PLAIN = re.compile(r'^' + AD + r'([^{].*;)$')
CALLS = re.compile(r'\b(P[0-9a-f]+|S_\w+|X_[0-9a-f]+|O[0-9a-f]+|JALR|VUM0?|VCALLMS|YO_AUTO|VQMFC2|VQMTC2|DSR_RESTORE|M_[A-Z0-9_]+|SYSCALL|BREAK|IRQ_POLL|COP0OP)\(|return|switch')   # MGS2: stage functions S_<stage>_P<addr>, MMI helpers (registers by number), kernel calls
WHY = []   # (register, line index, line, state) of the rejections (debug)
LAB = re.compile(r'^L([0-9a-f]+):;$')
def _stmt(st, d):
    """one statement's relation to R[d]: ('dsll', s, A) / ('cons', e, kind, B) / ('kill',) / ('none',) / ('use',)"""
    m = re.fullmatch(DSLL_E, st)
    if m and m.group(1) == d: return ('dsll', m.group(2), int(m.group(3)))
    for k, p in zip(('sra', 'srl'), CONS_E):
        m = re.fullmatch(p, st)
        if m and m.group(2) == d: return ('cons', m.group(1), k, int(m.group(3)))
    if CALLS.search(st): return ('use',)
    m = re.fullmatch(r'R\[(\d+)\] = ([^;]*);', st)
    if m and m.group(1) == d: return ('use',) if re.search(r'\bR\[%s\]' % d, m.group(2)) else ('kill',)
    return ('use',) if re.search(r'\bR\[%s\]' % d, st) else ('none',)
RET_USE = {2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30, 31}   # read by the caller after a return (EE ABI: results, callee-saved, gp/sp/ra)
# calls: a recompiled function reads / may write its interprocedural sets (ipo_abi below); a host function (X_, written to the ABI) and an
# indirect call (virtual methods, function pointers: compiled C++, the EE ABI) read the argument and callee-saved registers (and t9, the
# target of a jalr) and may write the caller-saved ones.  (The analysis sees v1 read on entry in the animation stream samplers
# SampleQuaternionStream 0x1a0d70 and its callers only along paths where the code before writes it on every real path: the merge of an
# early-out path; their callers here are direct calls, which use the computed sets.)
ABI_IN = set(range(4, 12)) | {16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30, 31}
ABI_OUT = set(range(1, 16)) | {24, 25, 31}
HOST = (ABI_IN, ABI_OUT, set(range(12, 32)), set(range(0, 20)))
JALR_SETS = (ABI_IN | {25}, ABI_OUT, set(range(12, 32)), set(range(0, 20)))
CALLNAME = re.compile(r'\b(P[0-9a-f]+|S_\w+|O[0-9a-f]+|X_[0-9a-f]+|JALR)\(')
# MGS2: kernel calls, the MMI helpers (registers by number) and IRQ_POLL (an interrupt handler saves the registers it uses) read everything
OTHER = re.compile(r'\b(VUM0?|VCALLMS|VQMFC2|VQMTC2|DSR_RESTORE|SYSCALL|BREAK|IRQ_POLL|COP0OP|M_[A-Z0-9_]+)\(|switch')
def call_sets(name, ipo):
    if name.startswith(('P', 'O', 'S_')) and name in ipo: return ipo[name]   # MGS2: stage functions S_<stage>_P<addr>
    return JALR_SETS if not name.startswith('X_') else HOST
def ipo_abi(bodies, ipo_function):
    """(RIN, ROUT, FIN, FOUT) per function with the unknown callees as above (regcache.ipo_function, fixpoint)"""
    I = {n: (set(), set(), set(), set()) for n in bodies}
    look = lambda n: I[n] if n in I else call_sets(n, I)
    ch = True; r = 0
    while ch and r < 40:
        ch = False; r += 1
        for n, b in bodies.items():
            v = ipo_function(b, look)
            if v != I[n]: I[n] = v; ch = True
    return I
ALLR = frozenset(range(1, 32))
def _must_function(body, look):
    """registers every path from the entry to a return writes (a call adds what its callee must write).  A line's writes count for its
    successors only when they run on every path through it: plain lines and the delay-slot branch form (the slot runs first); a line with
    any other conditional part (branch-likely, an inlined body, a conditional return) counts nothing."""
    import regcache as rc
    lines = body.split('\n'); N = len(lines); lab = {}
    for i, s in enumerate(lines):
        m = re.match(r'^L([0-9a-f]+):;', s)
        if m: lab[m.group(1)] = i
    defin = [None] * N; defin[0] = frozenset(); work = [0]; at_ret = None
    while work:
        i = work.pop(); s = lines[i]
        sure = not re.search(r'\bif \((?!c_\))', s) and not re.search(r'_i\d+:;', s) and not re.search(r'\{ if ', s)
        dr = set(defin[i])
        if sure:
            for e, k, n, _ in rc._events(s):
                if k == 'w': dr.add(n)
                elif k == 'c': dr |= look(n)
        if re.search(r'\breturn;', s):
            at_ret = frozenset(dr) if at_ret is None else at_ret & frozenset(dr)
        uncond = rc.uncond_line(s, '(R[0] == R[0])')   # regcache.py RC_UNCOND_FIX: an inlined body's own branch does not end the line
        nxt = [lab[g] for g in rc.gotos(s) if g in lab] + ([i + 1] if i + 1 < N and not uncond else [])
        o = frozenset(dr)
        for t in nxt:
            if defin[t] is None: defin[t] = o; work.append(t)
            else:
                nd = defin[t] & o
                if nd != defin[t]: defin[t] = nd; work.append(t)
        if i + 1 >= N and not uncond: at_ret = frozenset(dr) if at_ret is None else at_ret & frozenset(dr)   # falling off the end
    return at_ret if at_ret is not None else frozenset()
def must_abi(bodies):
    """MUST[name]: the registers a call of the recompiled function always writes (greatest fixpoint; host and indirect calls: none)"""
    M = {n: ALLR for n in bodies}
    look = lambda n: M.get(n, frozenset()) if n.startswith(('P', 'O')) else frozenset()
    ch = True; r = 0
    while ch and r < 60:
        ch = False; r += 1
        for n, b in bodies.items():
            v = _must_function(b, look) & M[n]
            if v != M[n]: M[n] = v; ch = True
    return M
MUST = {}
def _line(ln, d, ipo=None):
    """the line's relation to R[d] (as _stmt, plus ('call', maywrite)); a branch line with a delay-slot statement reads its condition first"""
    if not ln.strip() or LAB.match(ln): return ('none',)
    if 'RH[' in ln and not CALLS.search(ln): return ('use',) if re.search(r'\bR(H)?\[%s\]' % d, ln) else ('none',)
    if CALLS.search(ln):
        if re.search(r'\bR\[%s\]' % d, ln) or ipo is None or OTHER.search(ln): return ('use',)
        names = CALLNAME.findall(ln)
        if 'return' in CALLNAME.sub('', ln) and (int(d) in RET_USE or ' if (' in ln): return ('use',)
        if re.search(r'\bgoto\b', ln) and names: return ('use',)
        if re.search(r'\bYO_AUTO\(', ln): names = [x for x in names if not x.startswith('X_')]   # the headless game runs the original (g_yo)
        mw = False; mustw = len(names) == 1
        for name in names:
            rin, rout = call_sets(name, ipo)[:2]
            if int(d) in rin: return ('use',)
            mw |= int(d) in rout
            mustw = mustw and int(d) in MUST.get(name, ())
        return ('kill',) if mustw else ('call', mw)
    m = PLAIN.match(ln)
    if m: return _stmt(m.group(1), d)
    m = SLOT.match(ln)
    if m:
        st = _stmt(m.group(2).strip(), d) if m.group(2).strip() else ('none',)
        if re.search(r'\bR\[%s\]' % d, m.group(1)): return ('seq', ('use',), st)   # the condition reads first
        return st
    return ('use',) if re.search(r'\bR\[%s\]' % d, ln) else ('none',)
def _succ(ln, i, N, lab):
    t = [lab[g] for g in re.findall(r'goto L([0-9a-f]+);', ln) if g in lab]
    if re.search(r'goto L[0-9a-f]+;', ln) and re.search(r'goto \*|goto [^L]', ln): t.append(None)
    top = re.sub(r'\{ /\* inlined .*? Lend_i\d+:; \}', '', ln)   # an inlined body's own branches and labels stay inside it
    always = re.match(r'^' + AD + r'\{ int c_ = \(R\[0\] == R\[0\]\);', top) or (re.search(r'\breturn;', top) and ' if (' not in top and 'switch' not in top) or re.fullmatch(AD + r'goto L[0-9a-f]+;', top)
    return t + ([i + 1] if i + 1 < N and not always else [])
def narrow(body, ipo=None):
    L = body.split('\n'); N = len(L)
    regs = sorted({m.group(1) for ln in L for m in re.finditer(DSLL_E, ln)})
    if not regs: return body, 0
    lab = {}
    for i, ln in enumerate(L):
        m = LAB.match(ln)
        if m: lab[m.group(1)] = i
    succ = [_succ(ln, i, N, lab) for i, ln in enumerate(L)]
    n = 0
    for d in regs:
        cl = [_line(ln, d, ipo) for ln in L]
        def step(c, s):
            """the state after an action (None: the register cannot be narrowed in this function)"""
            if c[0] == 'seq':
                s = step(c[1], s)
                return None if s is None else step(c[2], s)
            if c[0] == 'dsll': return None if c[1] == d and 'S' in s else frozenset(['S'])   # dsll32 x, x: reads x first
            if c[0] == 'kill': return frozenset(['N'])
            if c[0] == 'cons':
                if 'S' in s and (len(s) != 1 or (c[2] == 'srl' and c[3] == 32)): return None
                return frozenset(['N']) if c[1] == d else s
            if c[0] == 'call': return (s | frozenset(['N'])) if c[1] else s
            if c[0] == 'use': return None if 'S' in s else s
            return s
        IN = [None] * N; IN[0] = frozenset(['N']); work = [0]; bad = False
        while work:
            i = work.pop(); out = step(cl[i], IN[i])
            if out is None: bad = True; WHY.append((d, i, L[i], IN[i])); break
            for t in succ[i]:
                if t is None: bad = True; break   # a jump the analysis cannot follow
                nt = out if IN[t] is None else IN[t] | out
                if nt != IN[t]: IN[t] = nt; work.append(t)
            if bad: break
        if bad: continue
        for i in range(N):
            if IN[i] is None: continue                                 # unreachable lines (dead delay slots) stay as they are
            c = cl[i]
            if c[0] == 'seq': c = c[2]
            if c[0] == 'dsll':
                s, A = c[1], c[2]
                L[i] = L[i].replace('R[%s] = (int64_t)((uint64_t)R[%s] << %d);' % (d, s, A), 'R[%s] = (int32_t)((uint32_t)R[%s] << %d);   /* dsll32: the high word (narrow.py) */' % (d, s, A - 32), 1); n += 1
            elif c[0] == 'cons' and 'S' in IN[i]:
                e, k, B = c[1], c[2], c[3]
                if k == 'sra': L[i] = L[i].replace('R[%s] = R[%s] >> %d;' % (e, d, B), 'R[%s] = (int32_t)R[%s] >> %d;' % (e, d, B - 32), 1)
                else: L[i] = L[i].replace('R[%s] = (int64_t)((uint64_t)R[%s] >> %d);' % (e, d, B), 'R[%s] = (int32_t)((uint32_t)R[%s] >> %d);' % (e, d, B - 32), 1)
    return '\n'.join(L), n
