"""Post-pass over recompiled C (recomp.py output): the guest GPRs of each function live in C locals (r<n>) instead of the global register
file R[], so the host compiler keeps them in host registers.  The global R[] is the interface at the function boundary:
  entry   every register the function uses is loaded from R[] (dead loads are dropped by the compiler)
  call    (any call of another recompiled / host function) the registers the function writes are stored to R[] before the call (the
          callee-saved s0-s7 / fp too: a callee saves them in its stack frame, and that memory is part of the guest state), and the
          caller-saved registers are reloaded after it (s0-s7, fp, gp, sp keep their value across a call: the EE ABI)
  return  the registers the function writes are stored to R[]
R[0] becomes the local r0 = 0 (only dead code writes it).  Helpers that take a register number (VQMFC2 / VQMTC2) get that register stored / reloaded around them.
usage: python3 regcache.py file.h   (in place)"""
import re, sys, os
CALLEE = {16, 17, 18, 19, 20, 21, 22, 23, 30}
NOREL = CALLEE | {28, 29}            # unchanged by a call
# calls that do not touch R[] (memory / FPU / VU helpers, libm)
PURE = {'RD8', 'RD16', 'RD32', 'RD64', 'WR8', 'WR16', 'WR32', 'WR64', 'RDF', 'WRF', 'MM', 'I2F', 'F2I', 'F2INT', 'FDIV', 'fabsf', 'sqrtf', 'fminf', 'fmaxf',
        'vu_f', 'vu_u', 'vu_in', 'vu_cl', 'vout', 'vu_max', 'vu_min', 'vu_setflags', 'vu_ftoi', 'VUM', 'VUM0', 'VLQC2', 'VSQC2', 'VCFC2', 'VCTC2', 'HWR32',
        'HWW32', 'YO_AUTO', 'FN_ENTER', 'RCLD', 'RCMV', 'vmem', 'VI16', 'sqrtf', 'fabsf', 'if', 'while', 'for', 'switch', 'return', 'sizeof', 'memcpy', 'BBT', 'VQMFC2', 'VQMTC2', 'DSR_SAVE', 'DSR_RESTORE', 'VU_MCL1', 'VU_MCL2', 'VU_MCL3', 'VU_MCL4', 'VU_MO_',
        'GLDL', 'GLDR', 'GSDL', 'GSDR', 'GLWL', 'GLWR', 'GSWL', 'GSWR', 'COP0R', 'NCHK'}
# COP0R (mfc0: Status, Count, perf counters) touches no guest register.  A non-pure call whose value is used would be wrapped as
# (stores, CALL(..), reloads): the comma expression's value is the last reload, not the call's (process() refuses that).
# MGS2: IRQ_POLL (a loop back edge) may deliver an interrupt or switch threads.  The kernel saves and restores the register file around
# that, so the locals stay valid, but the handler's stack frame saves the registers it uses: those must be the exact values (the guest
# memory hash), so IRQ_POLL is a call that reads every register and writes none (POLL_SETS: dirty locals stored before, nothing reloaded).  The MMI helpers M_<op>(rd, rs, rt) (shifts: M_<op>(rd, rt, sa)) work on R[] / RH[] by register number: their
# source registers are stored to R[] before them and rd is reloaded after (mmi_args).
MMI_SHIFT = ('PSLLH', 'PSRLH', 'PSRAH', 'PSLLW', 'PSRLW', 'PSRAW')
MMI_RE = re.compile(r'\bM_([A-Z0-9_]+)\((\d+), (\d+), (\d+)\);')
def mmi_args(m):
    """(written register, [read registers]) of an M_ helper call"""
    op, d, a, b = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
    rd = [a] if op in MMI_SHIFT else [a, b]
    return d, [x for x in rd if x]
FN = re.compile(r'^static void ([A-Za-z_]\w*)\(void\)\n\{\n(.*?)\n\}$', re.S | re.M)
CALL = re.compile(r'\b([A-Za-z_]\w*)\(')
def _calls(text):
    """spans (start, end, name) of the top-level non-pure calls in text (the arguments of a call are scanned for nested calls too)"""
    out = []; i = 0
    while True:
        m = CALL.search(text, i)
        if not m: return out
        name = m.group(1)
        if name in PURE or name.startswith(('vu_', 'vuc', 'M_')): i = m.end(); continue
        d = 1; j = m.end()
        while d:
            c = text[j]; d += (c == '(') - (c == ')'); j += 1
        out.append((m.start(), j, name)); i = j
WRITE = re.compile(r'\br(\d+) = ')
# 64-bit constructs: a function containing one keeps 64-bit locals; any other function holds only sign-extended 32-bit values in its
# registers (every EE instruction it runs produces one from such inputs), so its locals are int32_t (NARROW): half the host instructions
# of the 64-bit emulation on a 32-bit host.  The values it receives (arguments, callee-saved registers, return values) are assumed
# sign-extended too (checked: the guest memory hash of the 900-frame run is unchanged).
#   lq / sq / ld / sd of a register to anything but the stack ($sp-based: the function's own spills and callee-saved saves), dsll / dsrl /
#   dsra by 32+ (and their variable forms), daddu / dsubu of two registers, lwu, the 128-bit COP2 moves
WIDE = [re.compile(x) for x in (
    r'\(uint64_t\)R\[\d+\] (<<|>>)', r'\bR\[\d+\] >> (3[2-9]|[4-6]\d)\b', r'\bR\[(\d+)\] = R\[(?!0\])\d+\] [+-] R\[(?!0\])\d+\];',
    r'\bR\[\d+\] = RD32\(', r'\bRD64\(\(uint32_t\)\(R\[(?!29\])', r'\bWR64\(\(uint32_t\)\(R\[(?!29\])', r'qa_ = \(\(uint32_t\)\(R\[(?!29\])',
    r'\bVQMFC2\(', r'\bVQMTC2\(', r'\bRH\[\d+\] = (?!\(int64_t\)RD64\(qa_)', r'\bM_[A-Z]', r'\bG[LS]D[LR]\(')]
# per-register widths (RC_PERREG=1): in a function with one of the 64-bit constructs above only the registers whose upper half can
# matter get int64_t locals, the rest int32_t.  A register is 64-bit when an instruction can give it a value other than a sign-extended
# 32-bit one (64-bit shifts left / logical right, daddu / dsubu of two registers, lwu, ld / lq not from the stack, qmfc2, the MMI and
# unaligned doubleword helpers, mfhi / mflo after an mthi / mtlo, any form not listed below), or when an instruction reads its upper half
# (sd / sq not to the stack, qmtc2, the MMI and unaligned helpers, logical right and 32+ arithmetic shifts, shifts left by less than 32,
# daddu / dsubu, mthi / mtlo, the 128-bit MMI moves, and the compares: branches, slt / sltu, movz / movn); both spread through the moves and the 64-bit logical / add ops (or / and / xor /
# nor, ori / xori, daddiu, dsra, movz / movn) in both directions, and through a stack slot (sd / sq at $sp + k, ld / lq of it; any slot
# when the frame's address is taken and a 64-bit register is stored through another base, a callee-saved restore excepted).  The other
# registers hold a sign-extended value wherever their upper half is read (a value received from outside the function, e.g. a zero-extended
# 0xffffffff, can reach a compare, which the 32-bit functions' NARROW assumption allows but a 64-bit function, exact in the default mode,
# must keep).
ASSIGN = re.compile(r'\bR\[(\d+)\] = ([^;]*);')
NARROW_E = re.compile(r'^(\(int64_t\)\(int32_t\)|\(int32_t\)|\(int16_t\)|\(int8_t\)|RD8\(|RD16\(|-?\d+$|0x[0-9a-f]+$|\(R\[\d+\] < |'
                      r'\(\(uint64_t\)R\[\d+\] < |\(int64_t\)SAR$|R\[\d+\] & \d+$)')
PROP_E = re.compile(r'^(R\[(\d+)\]|R\[(\d+)\] [|^] -?\d+|R\[(\d+)\] \+ -?\d+|R\[(\d+)\] [&|^] R\[(\d+)\]|~\(R\[(\d+)\] \| R\[(\d+)\]\)|'
                    r'R\[(\d+)\] >> -?\d+|R\[(\d+)\] >> \(R\[\d+\] & 63\)|R\[(\d+)\] [+-] R\[0\]|R\[0\] [+-] R\[(\d+)\])$')
WIDE_E = re.compile(r'^(\(int64_t\)\(\(uint64_t\)R\[\d+\] (<<|>>)|R\[(?!0\])\d+\] [+-] R\[(?!0\])\d+\]$|RD32\(|HWR32\(|G[LS][DW][LR]\(|lo_$|hi_$)')
CONS_P = [re.compile(x) for x in (r'\(uint64_t\)R\[(\d+)\] >>', r'\(uint64_t\)R\[(\d+)\] << (?:\d|[12]\d|3[01]|\(R)\b', r'\bR\[(\d+)\] >> (?:3[2-9]|[4-6]\d|\(R)',
          r'\bG[LS][DW][LR]\(R\[(\d+)\]', r'\b(?:HI|LO|HI1|LO1) = R\[(\d+)\]', r'\b(?:lo_|hi_) = [^,;]*?R\[(\d+)\]', r'\b(?:lo_|hi_) = [^,;]*?R\[\d+\][^,;]*?R\[(\d+)\]',
          r'\bR\[\d+\] = R\[(?!0\])(\d+)\] [+-] R\[(?!0\])\d+\];', r'\bR\[\d+\] = R\[(?!0\])\d+\] [+-] R\[(?!0\])(\d+)\];', r'\bVQMTC2\((\d+),',
          r'\bWR64\(\(+uint32_t\)\(R\[(?!29\])\d+\] \+ -?\d+\)\)?(?: & ~15u)?, R\[(\d+)\]\)',
          r'\bR\[(\d+)\] (?:==|!=|<=|>=|<|>) ', r' (?:==|!=|<=|>=|<|>) (?:\(uint64_t\))?R\[(\d+)\]')]
ST64 = re.compile(r'\bWR64\(\(+uint32_t\)\(R\[(\d+)\] \+ (-?\d+)\)\)?(?: & ~15u)?, R\[(\d+)\]\)')
WIDE_STATS = {}
def wide_regs(body):
    """the registers of a 64-bit function that need int64_t locals (see above)"""
    W = set(); props = []; stk_ld = []; stk_st = {}; alias_st = set(); hilo_w = bool(re.search(r'\b(?:HI|LO|HI1|LO1) = R\[', body))
    taken = bool(re.search(r'\bR\[(?!29\])\d+\] = (?:\(int64_t\)\(int32_t\)\()?R\[29\] [+|-]', body))
    for ln in body.split('\n'):
        if ln.startswith('FN_ENTER('): continue
        for p in CONS_P:
            for m in p.finditer(ln):
                if m.group(1) != '0': W.add(int(m.group(1)))
        for m in re.finditer(r'\bVQMFC2\((\d+),', ln): W.add(int(m.group(1)))
        for m in MMI_RE.finditer(ln):
            d, rd = mmi_args(m); W.add(d); W.update(rd)
        for m in ST64.finditer(ln):
            if m.group(1) == '29': stk_st.setdefault(int(m.group(2)), set()).add(int(m.group(3)))
            else: alias_st.add(int(m.group(3)))
        qb = re.search(r'qa_ = \(\(uint32_t\)\(R\[(\d+)\] \+ (-?\d+)\)', ln)
        for m in ASSIGN.finditer(ln):
            d, e = int(m.group(1)), m.group(2).strip()
            if d == 0: continue
            if e.startswith('(int64_t)RD64('):
                ld = re.match(r'\(int64_t\)RD64\(\(uint32_t\)\(R\[(\d+)\] \+ (-?\d+)\)\)$', e)
                if not ld and qb and e == '(int64_t)RD64(qa_)': ld = qb
                if ld and ld.group(1) == '29': stk_ld.append((d, int(ld.group(2))))
                else: W.add(d)
                continue
            if e in ('HI', 'LO', 'HI1', 'LO1'):
                if hilo_w: W.add(d)
                continue
            if WIDE_E.match(e): W.add(d); continue
            pm = PROP_E.match(e)
            if pm: props.append((d, [int(x) for x in pm.groups()[1:] if x is not None and x != '0'])); continue
            if NARROW_E.match(e): continue
            W.add(d); k = re.sub(r'\d+', 'N', e)[:60]; WIDE_STATS[k] = WIDE_STATS.get(k, 0) + 1
    while True:
        n = len(W)
        for d, src in props:
            if d in W or any(x in W for x in src): W.add(d); W.update(src)
        anyw = taken and bool(alias_st & W)
        for d, k in stk_ld:
            st = stk_st.get(k, set())
            if st & W or (anyw and st != {d}): W.add(d)
            if d in W and st != {d}: W.update(st)
        if len(W) == n: return W - {0}
NARROW_IDX = [0]
def is_wide(body):
    import os
    if os.environ.get('RC_WIDE'): return True
    if os.environ.get('RC_NARROW'):
        lo, hi = (int(x) for x in os.environ['RC_NARROW'].split(','))
        if not (lo <= NARROW_IDX[0] < hi): return True
    return any(w.search(body) for w in WIDE)
# dead save removal (RC_DSR): the callee-saved registers of a function (s0-s7, fp) are kept in the caller's C locals across a call, so the
# callee's own save / restore of them (sq / lq, sd / ld to its stack frame) is dead work writing dead stack (the state hash of the port
# excludes the stack region).  Dropped when the function references the save slot only in those statements and never uses the 128-bit
# half of the register otherwise.  A register the function reads before writing (non-ABI input) is listed in IN[name]: its callers store
# it before the call; the other callee-saved registers are no longer stored before direct calls.
# RC_UNCOND_FIX=1 (2026-10-24): an inlined body's own unconditional branch ({ int c_ = (R[0] == R[0]); ...; if (c_) goto L<a>_i<k>; })
# made its whole line count as one that never falls through (no edge to the next line in the flows and the liveness: reads after
# it were missed); the test now looks at the line with its inlined bodies blanked, and a goto matches only a whole label name.
UNCOND_FIX = bool(os.environ.get('RC_UNCOND_FIX') or os.environ.get('RC_SOUND'))
INL_RE = re.compile(r'\{ /\* inlined \w+ \*/')
def blank_inl(s):
    """s with each inlined body ({ /* inlined P.. */ ... }) replaced by spaces"""
    if 'inlined' not in s: return s
    out = list(s)
    for m in INL_RE.finditer(s):
        d = 0; j = m.start()
        while True:
            c = s[j]; d += (c == '{') - (c == '}'); j += 1
            if d == 0: break
        out[m.start():j] = ' ' * (j - m.start())
    return ''.join(out)
def gotos(s):
    """the label names (hex) of the function-level gotos of line s"""
    return re.findall(r'goto L([0-9a-f]+);', s) if UNCOND_FIX else re.findall(r'goto L([0-9a-f]+)', s)
def uncond_line(s, zero):
    """line s never falls through: an unconditional branch of its own (zero: '(R[0] == R[0])' or '(r0 == r0)') or a return outside an if"""
    b = blank_inl(s) if UNCOND_FIX else s
    return (zero in b and 'goto' in b) or ('return;' in s and 'if (' not in s)
DSR_IN = {}; DSR_ANY = set()
SQ = re.compile(r'WR64\(\(\(uint32_t\)\(R\[29\] \+ (-?\d+)\)\) & ~15u, R\[(\d+)\]\); WR64\(\(\(\(uint32_t\)\(R\[29\] \+ \1\)\) & ~15u\) \+ 8, RH\[\2\]\);')
SD = re.compile(r'WR64\(\(uint32_t\)\(R\[29\] \+ (-?\d+)\), R\[(\d+)\]\);')
LQ = re.compile(r'\{ uint32_t qa_ = \(\(uint32_t\)\(R\[29\] \+ (-?\d+)\)\) & ~15u; if \((\d+)\) \{ R\[\2\] = \(int64_t\)RD64\(qa_\); RH\[\2\] = \(int64_t\)RD64\(qa_ \+ 8\); \} \}')
LD = re.compile(r'R\[(\d+)\] = \(int64_t\)RD64\(\(uint32_t\)\(R\[29\] \+ (-?\d+)\)\);')
def dsr(body):
    """drops the save / restore of the callee-saved registers (see above); the save and the restores may sit inside a line with other
    statements (a call's delay slot).  Returns the body and the number of saves dropped."""
    import os
    if not os.environ.get('RC_DSR'): return body, 0
    lines = body.split('\n'); saves = {}
    for i, ln in enumerate(lines):
        for m in list(SQ.finditer(ln)) + list(SD.finditer(ln)):
            k, off = int(m.group(2)), int(m.group(1))
            if k in CALLEE: saves.setdefault(k, []).append((i, off, m.group(0)))
    n = 0; cuts = []   # (line, substring) to remove
    for k, sv in saves.items():
        if len(sv) != 1: continue
        i, off, txt = sv[0]
        slot = 'R[29] + %d)' % off
        rest = []; ok = True
        for j, ln in enumerate(lines):
            c = ln.count(slot)
            if j == i: c -= txt.count(slot)
            if c == 0: continue
            found = 0
            for m in list(LQ.finditer(ln)) + list(LD.finditer(ln)):
                rk, roff = (int(m.group(2)), int(m.group(1))) if m.re is LQ else (int(m.group(1)), int(m.group(2)))
                if rk == k and roff == off: rest.append((j, m.group(0))); found += m.group(0).count(slot)
            if found != c: ok = False; break
        if not ok or not rest: continue
        # a callee-save restore is followed by the return with no further use of the register (a spill / reload of a live value is not)
        rdk = re.compile(r'\bR\[%d\](?! = )' % k)
        for j, rt in rest:
            after = lines[j][lines[j].index(rt) + len(rt):]
            if rdk.search(after): ok = False; break
            jj = j + 1
            while jj < len(lines) and 'return;' not in lines[jj]:
                if rdk.search(lines[jj]) or re.match(r'^L[0-9a-f]+:;', lines[jj]): ok = False; break
                jj += 1
            if not ok or jj >= len(lines): ok = False; break
            if rdk.search(lines[jj][:lines[jj].index('return;')]): ok = False; break
        if not ok: continue
        if any(('RH[%d]' % k) in ln for j, ln in enumerate(lines) if j != i and not any(r[0] == j for r in rest)): continue
        cuts.append((i, txt)); n += 1
        for j, rt in rest: cuts.append((j, rt))
    # the save becomes a copy of the register's 64 + 64 bits into C locals, each restore a copy back (guest.h DSR_SAVE / DSR_RESTORE):
    # the value of R[k] across a call stays exactly what the callee found, as the ABI promises, without the stack traffic
    saved = []
    for j, t in cuts:
        k = int((SQ.search(t) or SD.search(t) or LQ.search(t) or LD.search(t)).group(2 if (SQ.search(t) or SD.search(t) or LQ.search(t)) else 1))
        if (SQ.search(t) or SD.search(t)): lines[j] = lines[j].replace(t, '', 1); saved.append(k)
        else: lines[j] = lines[j].replace(t, 'DSR_RESTORE(%d);' % k, 1)
    if saved:   # the copies at the top of the function (the entry values), in the function's scope
        at = 1 if lines and lines[0].startswith('FN_ENTER(') else 0
        lines[at:at] = ['  DSR_SAVE(%d);' % k for k in sorted(set(saved))]
    return '\n'.join(lines), n
def dsr_in(body, lookup=None):
    """callee-saved registers the function may read before writing (forward dataflow over its lines, after the dead saves are gone); a
    call reads what its callee reads (lookup(name) -> set; unknown callees read every callee-saved register), so the sets are
    interprocedural once iterated to a fixpoint (run())"""
    lines = body.split('\n'); N = len(lines); lab = {}
    for i, s in enumerate(lines):
        m = re.match(r'^L([0-9a-f]+):;', s)
        if m: lab[m.group(1)] = i
    rd = []; wr = []
    for s in lines:
        w = {int(x) for x in re.findall(r'\bR\[(\d+)\] = ', s)} & CALLEE
        r = {int(x) for x in re.findall(r'\bR\[(\d+)\](?! = )', s)} & CALLEE
        if lookup:
            for a, b, name in _calls(s): r |= lookup(name)
        rd.append(r); wr.append(w)
    # defined-on-entry sets (must analysis): a read of an undefined register makes it IN; a register both read and written on one line
    # counts as read (the order within the line is unknown); every line falls through (conservative)
    defin = [None] * N; work = [0]; defin[0] = set(); IN = set()
    while work:
        i = work.pop(); d = defin[i]
        IN |= rd[i] - d
        o = d | wr[i]; s = lines[i]
        uncond = uncond_line(s, '(R[0] == R[0])')   # no fallthrough
        nxt = [lab[g] for g in gotos(s) if g in lab] + ([i + 1] if i + 1 < N and not uncond else [])
        for t in nxt:
            if defin[t] is None: defin[t] = set(o); work.append(t)
            elif not o >= defin[t]:
                nd = defin[t] & o
                if nd != defin[t]: defin[t] = nd; work.append(t)
    return IN
# interprocedural register traffic (RC_NOIPO=1 disables): for every function, the registers it may read before writing (IN: what a
# caller must store to R[] / F[] before calling it) and the caller-saved registers it may write (OUT: what a caller must reload after),
# both transitive through its direct calls; an indirect call or a host function counts as reading and writing everything.  A call then
# stores only the dirty registers in IN and reloads only the locals in OUT, and the registers it neither stored nor reloaded stay dirty.
ALLR = set(range(1, 32)); ALLF = set(range(32))
POLL_SETS = (ALLR, set(), ALLF, set())   # IRQ_POLL: (RIN, ROUT, FIN, FOUT)
# RC_POLL_R / RC_POLL_F (lean_plan.md 1b): the registers a poll reads, as comma lists.  A poll can run an interrupt handler or an alarm
# (runtime/kernel.h k_call_handler / k_alarm: the whole register file is copied to a host KCtx, a0-a2, sp and ra are set, the handler runs,
# the copy is restored) or switch threads (the register file saved in the thread's host KCtx); so a register's value at a poll reaches the
# guest's memory only through a handler reading it before writing it.  The sets: the union of IN over the handlers the game registers
# (tools/pollset.py), less a0-a2, sp and ra.
if os.environ.get('RC_POLL_R') is not None or os.environ.get('RC_POLL_F') is not None:
    POLL_SETS = tuple({int(x) for x in os.environ.get(k, '').split(',') if x.strip()} for k in ('RC_POLL_R',)) + (set(),) + \
                tuple({int(x) for x in os.environ.get(k, '').split(',') if x.strip()} for k in ('RC_POLL_F',)) + (set(),)
# RC_SPECIAL=1 (lean_plan.md 1b, dev 2026-10-24): host calls whose register traffic is known from the runtime, instead of "everything".
# SYSCALL (boot/boot.c SYSCALL, runtime/kernel.h k_syscall, runtime/sif.h sif_syscall): reads v1 (the number) and a0-a3 and writes v0,
# nothing else of the calling thread (a thread switch or a handler run in between saves and restores the register file in a host KCtx);
# k_take_interrupts / k_resched at its start can run an interrupt handler or an alarm, which reads what a poll reads (POLL_SETS); R[31]
# only in a stderr line.  COP0OP (ei / di: k_status and the poll kick; interrupts are taken at polls) and COP0W (performance counters)
# touch no register.  JSIF: the SIF command dispatcher's two table calls (P175cd0 at 0x175d98 / 0x175dd4, JALR in recomp.py's output,
# renamed JSIF in the work file): the union of the handlers' sets and the intersection of their MOUT.  The handlers: sceSifInitCmd's
# two system ones (P1757d8 writes 0x175780 / 0x175760 into the table), the six sceSifAddCmdHandler (P175ac0) calls with constant
# handlers in the ELF (0x1761a8, 0x176468, 0x176658, 0x176260 from P175ea8, 0x176f60 from P1773b0, 0x163bd0 from P163c10) and the user
# table's two through P15e248 (0x15dd40 from P15dda8, 0x15e600 from P15e6e0); no stage code calls these (checked on v26's headers).
SPECIAL = bool(os.environ.get('RC_SPECIAL'))
SIF_TARGETS = ('P175780', 'P175760', 'P1761a8', 'P176468', 'P176658', 'P176260', 'P176f60', 'P163bd0', 'P15e600', 'P15dd40')
def special(name, get):
    """RC_SPECIAL: the sets of a known host call as (RIN, ROUT, FIN, FOUT) (with RC_SOUND also MOUT, FMOUT), else None; get(f) -> f's
    current sets"""
    if not SPECIAL: return None
    r = _special(name, get)
    return r if r is None or SOUND else r[:4]
def _special(name, get):
    if name == 'SYSCALL': return ({3, 4, 5, 6, 7} | set(POLL_SETS[0]), {2}, set(POLL_SETS[2]), set(), set(), set())
    if name in ('COP0OP', 'COP0W'): return (set(), set(), set(), set(), set(), set())
    if name == 'JSIF':
        ts = [get(t) for t in SIF_TARGETS]
        if any(t is None for t in ts): return None
        u = tuple(None if any(t[k] is None for t in ts) else set().union(*[t[k] for t in ts]) for k in range(4))
        m = tuple(set.intersection(*[set(t[k]) for t in ts]) if all(len(t) > k and t[k] is not None for t in ts) else set() for k in (4, 5))
        return u + m
    return None
IPO = {}   # name -> (RIN, ROUT, FIN, FOUT), with RC_SOUND also (MOUT, FMOUT)
# RC_XUNIT=1 (lean_plan.md 1e): a call uses the sets of every function analysed in this unit, the stage functions S_<stage>_P<addr>
# included, and of the functions of other units read with RC_IPO_LOAD (the ELF's, for a stage's calls into the ELF; XIPO); without it
# only this unit's P / O names have sets and every other call reads and writes everything.  RC_IPO_SAVE=<file>: this unit's sets (json).
XIPO = {}
XUNIT = bool(os.environ.get('RC_XUNIT'))
def _events(s):
    """(pos, kind, n) of a raw line: 'r' read / 'w' write of R[n] ('fr' / 'fw' for F[n]) at the end of their statement, 'c' call (end, name)"""
    ev = []
    for m in re.finditer(r'\bR\[(\d+)\]( = )?', s):
        n = int(m.group(1))
        if n == 0: continue
        e = s.find(';', m.start()); e = len(s) if e < 0 else e
        ev.append((e, 'w' if m.group(2) else 'r', n, m.start()))
    for m in re.finditer(r'\bF\[(\d+)\]( = )?', s):
        e = s.find(';', m.start()); e = len(s) if e < 0 else e
        ev.append((e, 'fw' if m.group(2) else 'fr', int(m.group(1)), m.start()))
    for m in re.finditer(r'\bVQMFC2\((\d+),', s): ev.append((m.end(), 'w', int(m.group(1)), m.start()))
    for m in re.finditer(r'\bVQMTC2\((\d+),', s): ev.append((m.start(), 'r', int(m.group(1)), m.start()))
    for m in MMI_RE.finditer(s):
        d, rr = mmi_args(m)
        for x in rr: ev.append((m.start(), 'r', x, m.start()))
        if d: ev.append((m.end(), 'w', d, m.start()))
    for a, b, name in _calls(s): ev.append((b, 'c', name, a))
    # reads of a statement before its write; a call after the delay-slot statements that precede it textually
    ev.sort(key=lambda x: (x[0], 0 if x[1] in ('r', 'fr') else 1 if x[1] in ('w', 'fw') else 2, x[3]))
    return ev
def ipo_function(body, look):
    """(RIN, ROUT, FIN, FOUT) of one body given look(name) -> the callee's sets"""
    lines = body.split('\n'); N = len(lines); lab = {}
    for i, s in enumerate(lines):
        m = re.match(r'^L([0-9a-f]+):;', s)
        if m: lab[m.group(1)] = i
    evs = [_events(s) for s in lines]
    ROUT = set(); FOUT = set()
    for ev in evs:
        for e, k, n, _ in ev:
            if k == 'w': ROUT.add(n)
            elif k == 'fw': FOUT.add(n)
            elif k == 'c': a, b, c, d = look(n); ROUT |= b; FOUT |= d
    ROUT -= NOREL; FOUT -= FNOREL
    RIN = set(); FIN = set(); defin = [None] * N; work = [0]; defin[0] = (frozenset(), frozenset())
    while work:
        i = work.pop(); dr, df = set(defin[i][0]), set(defin[i][1]); s = lines[i]
        for e, k, n, _ in evs[i]:
            if k == 'r': RIN |= {n} - dr
            elif k == 'w': dr.add(n)
            elif k == 'fr': FIN |= {n} - df
            elif k == 'fw': df.add(n)
            else: a, b, c, d = look(n); RIN |= a - dr; FIN |= c - df; dr |= b; df |= d
        uncond = uncond_line(s, '(R[0] == R[0])')
        nxt = [lab[g] for g in gotos(s) if g in lab] + ([i + 1] if i + 1 < N and not uncond else [])
        o = (frozenset(dr), frozenset(df))
        for t in nxt:
            if defin[t] is None: defin[t] = o; work.append(t)
            else:
                nd = (defin[t][0] & o[0], defin[t][1] & o[1])
                if nd != defin[t]: defin[t] = nd; work.append(t)
    return RIN, ROUT, FIN, FOUT
# RC_SOUND=1 (lean_plan.md §5, 2026-10-24; implies RC_UNCOND_FIX): the interprocedural sets with must-writes.  ipo_function counts every
# write it sees, and every register a callee MAY write, as made for what follows, so a conditional write (movn / movz, a branch-likely's
# delay slot, an inlined body past its first branch, one switch case for the next) or a callee that writes a register on some paths only
# can hide a later read from IN; and a call site stores only the dirty registers in IN, then reloads OUT, so a register in OUT that the
# callee leaves unwritten on some path is reloaded from a stale R[].  With RC_SOUND: a write counts as made at a later point of its line
# only if every conditional part holding the write also holds that point (the parts: an if's body unless its condition is a nonzero
# constant, its else part, a switch block and each case of it); an inlined body is one call of its function (whose own flow sees its
# labels at line starts); a call adds the callee's MOUT, the registers it writes on every path to a return (a greatest fixpoint, computed
# first); a call site stores the dirty registers in RIN | (ROUT - MOUT); and in the liveness of the locals a conditional write kills
# nothing.  IPO entries become (RIN, ROUT, FIN, FOUT, MOUT, FMOUT).  Sets without MOUT (narrow.py's ABI sets) count as writing nothing
# for sure; with RC_XUNIT, sets read with RC_IPO_LOAD must have been saved with RC_SOUND too (checked when they are read).
SOUND = bool(os.environ.get('RC_SOUND'))
# RC_RETSCR (lean_plan.md 1c, dev): a return does not store the scratch registers it holds dirty (at, a0-a3, t0-t9, ra; f1-f19), and
# they no longer count as read there (liveness).  'drop': no store; 'poison': R[n] = RETPOISON(n, <function>) / F[n] = RETPOISONF(n,
# <function>) in place of the store (the test: a value 1c leaves stale must never be read).  The macros come from the build (-include).
RETSCR = os.environ.get('RC_RETSCR', '')
SCRATCH = {1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31}
FSCRATCH = set(range(1, 20))
def _match(t, i, o, c):
    """the index after the bracket closing the one at t[i]"""
    d = 0
    while True:
        ch = t[i]; d += (ch == o) - (ch == c); i += 1
        if d == 0: return i
def cond_regions(s, inl=True, cases=False):
    """the spans of line s that run on some paths only: the body of an if whose condition is not a nonzero constant (a block or one
    statement) and its else part, a switch's block (cases: and each case of it, which is entered by its own label), and (inl) an
    inlined body from its first label or goto on"""
    t = re.sub(r'/\*.*?\*/', lambda m: ' ' * len(m.group(0)), s); out = []
    for m in re.finditer(r'\b(if|switch) \(', t):
        j = _match(t, m.end() - 1, '(', ')'); k = j
        while k < len(t) and t[k] == ' ': k += 1
        if k >= len(t): continue
        e = _match(t, k, '{', '}') if t[k] == '{' else t.index(';', k) + 1
        cond = t[m.end():j - 1].strip()
        if not (m.group(1) == 'if' and re.fullmatch(r'\d+', cond) and int(cond)): out.append((k, e))
        if cases and m.group(1) == 'switch' and t[k] == '{':
            labs = []; d = 0
            for q in range(k + 1, e - 1):
                ch = t[q]
                if ch == '{': d += 1
                elif ch == '}': d -= 1
                elif d == 0 and (t.startswith('case ', q) or t.startswith('default:', q)) and not (t[q - 1].isalnum() or t[q - 1] == '_'): labs.append(q)
            out += list(zip(labs, labs[1:] + [e - 1]))
        q = e
        while q < len(t) and t[q] == ' ': q += 1
        if m.group(1) == 'if' and re.match(r'else\b', t[q:]):
            q += 4
            while t[q] == ' ': q += 1
            out.append((q, _match(t, q, '{', '}') if t[q] == '{' else t.index(';', q) + 1))
    if inl:
        for m in re.finditer(r'\{ /\* inlined \w+ \*/', s):
            e = _match(t, m.start(), '{', '}'); g = re.search(r'\bgoto\b|\bL\w+:;', t[m.end():e])
            if g: out.append((m.end() + g.start(), e))
    return out
_SOUND_INFO = {}
def _sound_lines(body):
    """per line: the events in order [(kind, n, regions)] (_events' plus 'g' = a goto to line n, 'x' = a return; regions: the
    conditional spans holding it) and whether the line can fall through.  An inlined body ({ /* inlined P.. */ ... }, a leaf with its
    own labels and branches) is one call of its function: its events are replaced by a 'c' event, so its effect is the function's own
    sets (RIN, MOUT, ...), which its own flow computes with its labels at line starts."""
    if body in _SOUND_INFO: return _SOUND_INFO[body]
    lines = body.split('\n'); lab = {}
    for i, s in enumerate(lines):
        m = re.match(r'^L([0-9a-f]+):;', s)
        if m: lab[m.group(1)] = i
    info = []
    for s in lines:
        inl = [(m.start(), _match(s, m.start(), '{', '}'), m.group(1)) for m in re.finditer(r'\{ /\* inlined (\w+) \*/', s)] if 'inlined' in s else []
        out_ = lambda p: not any(a <= p < b for a, b, _ in inl)
        regs = cond_regions(s, False, True) if ('if (' in s or 'switch (' in s) else []
        rs = lambda p: frozenset(k for k, (a, b) in enumerate(regs) if a <= p < b)
        ev = [(e, 0 if k in ('r', 'fr') else 1 if k in ('w', 'fw') else 2, k, n, rs(st)) for e, k, n, st in _events(s) if out_(st)]
        ev += [(b, 2, 'c', name, rs(a)) for a, b, name in inl]
        ev += [(m.start(), 3, 'g', lab[m.group(1)], rs(m.start())) for m in re.finditer(r'\bgoto L([0-9a-f]+);', s) if m.group(1) in lab and out_(m.start())]
        ev += [(m.start(), 3, 'x', 0, rs(m.start())) for m in re.finditer(r'\breturn;', s) if out_(m.start())]
        ev.sort(key=lambda x: (x[0], x[1]))
        sb = blank_inl(s)
        top_exit = any(k in ('g', 'x') and not r for _, _, k, _, r in ev) or ('(R[0] == R[0])' in sb and 'goto' in sb)
        info.append(([(k, n, r) for _, _, k, n, r in ev], top_exit))
    _SOUND_INFO[body] = (info, len(lines))
    return _SOUND_INFO[body]
def _sound_flow(body, look, must, keep=False):
    """one body's forward flow.  must: (MOUT, FMOUT) only, callees' from look(name)[4:6] (missing: none); else (RIN, ROUT, FIN, FOUT,
    MOUT, FMOUT) with look(name) the callees' sets"""
    info, N = _sound_lines(body)
    def sets(n):
        t = look(n); return t[0], t[1], t[2], t[3], (t[4] if len(t) > 4 else set()), (t[5] if len(t) > 5 else set())
    ROUT = set(); FOUT = set()
    if not must:
        for evs, _ in info:
            for k, n, r in evs:
                if k == 'w': ROUT.add(n)
                elif k == 'fw': FOUT.add(n)
                elif k == 'c': t = sets(n); ROUT |= t[1]; FOUT |= t[3]
        ROUT -= NOREL; FOUT -= FNOREL
    RIN = set(); FIN = set(); mo = [None, None]
    defin = [None] * N; work = [0] if N else []
    if N: defin[0] = (frozenset(), frozenset())
    def merge(t, o):
        if defin[t] is None: defin[t] = o; work.append(t)
        else:
            nd = (defin[t][0] & o[0], defin[t][1] & o[1])
            if nd != defin[t]: defin[t] = nd; work.append(t)
    def ret(dr, df):
        mo[0] = frozenset(dr) if mo[0] is None else mo[0] & dr; mo[1] = frozenset(df) if mo[1] is None else mo[1] & df
    while work:
        i = work.pop(); d0r, d0f = defin[i]; evs, top_exit = info[i]; wl = []
        def made(r):
            dr = set(d0r); df = set(d0f)
            for rr, a, b in wl:
                if rr <= r: dr |= a; df |= b
            return dr, df
        for k, n, r in evs:
            if k == 'r':
                if not must and n not in d0r and n not in made(r)[0]: RIN.add(n)
            elif k == 'fr':
                if not must and n not in d0f and n not in made(r)[1]: FIN.add(n)
            elif k == 'w': wl.append((r, {n}, frozenset()))
            elif k == 'fw': wl.append((r, frozenset(), {n}))
            elif k == 'c':
                t = sets(n)
                if not must: dr, df = made(r); RIN |= t[0] - dr; FIN |= t[2] - df
                wl.append((r, t[4], t[5]))
            elif k == 'g': dr, df = made(r); merge(n, (frozenset(dr), frozenset(df)))
            elif k == 'x': ret(*made(r))
        if not top_exit:
            dr, df = made(frozenset())
            if i + 1 < N: merge(i + 1, (frozenset(dr), frozenset(df)))
            else: ret(dr, df)                          # falling off the end
    MOUT = set(mo[0]) - (set() if keep else NOREL) if mo[0] is not None else set()   # no return seen: nothing for sure
    FMOUT = set(mo[1]) - (set() if keep else FNOREL) if mo[1] is not None else set()
    return (MOUT, FMOUT) if must else (RIN, ROUT, FIN, FOUT, MOUT, FMOUT)
def ipo_function_sound(body, look): return _sound_flow(body, look, False)
def ipo_run_sound(bodies):
    """MOUT / FMOUT first (greatest fixpoint: the unit's functions start at everything; others write nothing for sure), then the rest
    with them fixed (least fixpoint)"""
    empty = (set(), set(), set(), set(), set(), set())
    M = {name: (ALLR - NOREL, ALLF - FNOREL) for name in bodies}
    known = lambda name: name in M if XUNIT else (name in M and name.startswith(('P', 'O')))
    def lookm(name):
        sp = special(name, lambda f: (None,) * 4 + M[f] if f in M else None)
        if sp is not None: return sp
        if known(name): return (None,) * 4 + M[name]
        x = XIPO.get(name) if XUNIT else None
        return (None,) * 4 + (set(x[4]), set(x[5])) if x is not None and len(x) > 5 else empty
    rounds = 0; changed = True
    while changed and rounds < 60:
        changed = False; rounds += 1
        for name, b in bodies.items():
            v = _sound_flow(b, lookm, True); v = (v[0] & M[name][0], v[1] & M[name][1])
            if v != M[name]: M[name] = v; changed = True
    full = (ALLR, ALLR - NOREL, ALLF, ALLF - FNOREL, set(), set())
    for name in bodies: IPO[name] = (set(), set(), set(), set()) + M[name]
    if XUNIT: look = lambda name: special(name, IPO.get) or (POLL_SETS if name == 'IRQ_POLL' else IPO[name] if name in IPO else XIPO.get(name, full))
    else: look = lambda name: special(name, IPO.get) or (POLL_SETS if name == 'IRQ_POLL' else IPO.get(name, full) if name.startswith(('P', 'O')) else full)
    r2 = 0; changed = True
    while changed and r2 < 40:
        changed = False; r2 += 1
        for name, b in bodies.items():
            v = _sound_flow(b, look, False); v = v[:4] + M[name]
            if v != IPO[name]: IPO[name] = v; changed = True
    return rounds + r2
def must_abi_sound(bodies):
    """narrow.py's MUST under RC_SOUND: the registers a call of each function writes on every path to a return, callee-saved restores
    included (greatest fixpoint; host and indirect calls: none)"""
    M = {name: (set(ALLR), set(ALLF)) for name in bodies}
    lookm = lambda name: (None,) * 4 + M[name] if name in M and name.startswith(('P', 'O')) else (set(),) * 6
    changed = True; rounds = 0
    while changed and rounds < 60:
        changed = False; rounds += 1
        for name, b in bodies.items():
            v = _sound_flow(b, lookm, True, True); v = (v[0] & M[name][0], v[1] & M[name][1])
            if v != M[name]: M[name] = v; changed = True
    return {name: frozenset(v[0]) for name, v in M.items()}
def ipo_run(bodies):
    if SOUND: return ipo_run_sound(bodies)
    full = (ALLR, ALLR - NOREL, ALLF, ALLF - FNOREL)
    for name in bodies: IPO[name] = (set(), set(), set(), set())
    if XUNIT: look = lambda name: special(name, IPO.get) or (POLL_SETS if name == 'IRQ_POLL' else IPO[name] if name in IPO else XIPO.get(name, full))
    else: look = lambda name: special(name, IPO.get) or (POLL_SETS if name == 'IRQ_POLL' else IPO.get(name, full) if name.startswith(('P', 'O')) else full)
    rounds = 0; changed = True
    while changed and rounds < 40:
        changed = False; rounds += 1
        for name, b in bodies.items():
            v = ipo_function(b, look)
            if v != IPO[name]: IPO[name] = v; changed = True
    return rounds
# liveness of the locals (RC_NOLIVE=1 disables): a local the callee may have written (OUT) is reloaded after the call only if the
# function may read it before writing it again (backward may-analysis over the lines; R[] / F[] keep the exact value meanwhile, and the
# dirty analysis treats the register as clean after the call, so a later store or return takes it from R[] / F[]).  Within a line, the
# reads after the call count and the writes are ignored (conservative); a line holding an inlined body (its own labels) kills nothing.
def liveness(conv, pfx, info, uncond, tail, extra):
    """live_out[i] (set of register numbers) and reads_at(i) -> [(pos, n)] for locals named <pfx><n>.  extra[i]: [(pos, n)] the stores
    the output adds to the line read (the dirty registers stored before a call or at a return: dirtiness is a may-property, so on a path
    where the local was not reloaded the store would copy a stale value unless the reload is kept)"""
    import os
    N = len(conv); RX = re.compile(r'\b%s(\d+)\b' % pfx)
    reads = []; kills = []
    for i, s in enumerate(conv):
        rd = list(extra[i]); wr = set()
        cr = cond_regions(s) if SOUND and ('if (' in s or 'switch (' in s) else []   # RC_SOUND: a write there kills nothing
        for m in RX.finditer(s):
            n = int(m.group(1))
            if pfx == 'r' and n == 0: continue
            if s.startswith(' = ', m.end()) or s[max(0, m.start() - 5):m.start()] in ('RCLD(', 'RCMV('):
                if not any(a <= m.start() < b for a, b in cr): wr.add(n)
            else: rd.append((m.start(), n))
        cond = ('_i' in s and re.search(r'_i\d+:;', s)) or re.search(r'if \([^;]*\) \{ ', s)   # inlined body / branch-likely: a write on one path only
        reads.append(rd); kills.append(set() if cond else wr - {n for _, n in rd})
    live_in = [set() for _ in range(N)]; live_out = [set() for _ in range(N)]
    if pfx in os.environ.get('RC_NOLIVE', ''): return [set(range(64)) for _ in range(N)], reads   # RC_NOLIVE=rf: off for the GPRs / FPRs
    if os.environ.get('RC_LIVE_RANGE'):
        lo, hi = (int(x) for x in os.environ['RC_LIVE_RANGE'].split(','))
        if not (lo <= NARROW_IDX[0] < hi): return [set(range(64)) for _ in range(N)], reads
    pred = [[] for _ in range(N)]
    for i in range(N):
        for t in info[i][2] + ([i + 1] if i + 1 < N and not uncond(i) else []): pred[t].append(i)
    work = list(range(N))
    if N: live_out[N - 1] = set(tail)
    while work:
        i = work.pop(); li = {n for _, n in reads[i]} | (live_out[i] - kills[i])
        if li != live_in[i]:
            live_in[i] = li
            for p in pred[i]:
                if not li <= live_out[p]: live_out[p] |= li; work.append(p)
    return live_out, reads
def ipo_sets(name):
    """the callee's (RIN, ROUT, FIN, FOUT) at a call site, or None (everything)"""
    import os
    if name == 'IRQ_POLL': return POLL_SETS
    if os.environ.get('RC_NOIPO') or not IPO: return None
    sp = special(name, IPO.get)
    if sp is not None: return sp
    if XUNIT: return IPO[name] if name in IPO else XIPO.get(name)
    return IPO.get(name) if name.startswith(('P', 'O')) else None
def process(body):
    if 'R[' not in body: return body
    import os
    wide = is_wide(body)
    WR = (wide_regs(body) if os.environ.get('RC_PERREG') else set(range(64))) if wide else set()   # the registers with int64_t locals
    fa = re.match(r'FN_ENTER\((0x[0-9a-f]+)\)', body)
    if os.environ.get('RC_NCHK') and fa: ldn = lambda n: 'r%d = R[%d]' % (n, n) if n in WR else 'r%d = NCHK(%d, %s)' % (n, n, fa.group(1))
    else: ldn = lambda n: 'r%d = R[%d]' % (n, n) if n in WR else 'r%d = (int32_t)R[%d]' % (n, n)
    regs = sorted(({int(x) for x in re.findall(r'\bR\[(\d+)\]', body)} | {x for m in MMI_RE.finditer(body) for x in [mmi_args(m)[0]] + mmi_args(m)[1]} | {int(x) for x in re.findall(r'\bVQM[FT]C2\((\d+),', body)}) - {0})
    in_set = [n for n in regs if n not in NOREL]
    lines = body.split('\n')
    conv = []
    for ln in lines:
        if ln.startswith('FN_ENTER('): conv.append(ln); continue
        s = re.sub(r'\bR\[(\d+)\]', r'r\1', ln)
        s = re.sub(r'\bVQMFC2\((\d+), (\d+)\);', lambda m: 'VQMFC2(%s, %s); r%s = R[%s];' % (m.group(1), m.group(2), m.group(1), m.group(1)) if m.group(1) != '0' else m.group(0), s)
        s = re.sub(r'\bVQMTC2\((\d+), (\d+)\);', lambda m: 'R[%s] = r%s; VQMTC2(%s, %s);' % (m.group(1), m.group(1), m.group(1), m.group(2)) if m.group(1) != '0' else m.group(0), s)
        s = MMI_RE.sub(lambda m: ''.join('R[%d] = r%d; ' % (x, x) for x in mmi_args(m)[1]) + m.group(0) + (' r%d = R[%d];' % ((mmi_args(m)[0],) * 2) if mmi_args(m)[0] else ''), s)
        conv.append(s)
    # may-dirty dataflow over the lines (a register written since the last store to R[]): fallthrough + goto edges
    N = len(conv); lab = {}
    for i, s in enumerate(conv):
        m = re.match(r'^L([0-9a-f]+):;', s)
        if m: lab[m.group(1)] = i
    def mkinfo(conv):
        info = []
        for s in conv:
            cs = _calls(s); w = [(m.start(), int(m.group(1))) for m in WRITE.finditer(s) if int(m.group(1))]
            info.append((cs, w, [lab[g] for g in gotos(s) if g in lab], 'return;' in s))
        return info
    def flow(info, xfer):
        din = [set() for _ in range(N)]; dout = [None] * N; work = list(range(N))
        while work:
            i = work.pop()
            o = xfer(i, din[i])
            if o != dout[i]:
                dout[i] = o
                for t in info[i][2] + ([i + 1] if i + 1 < N else []):
                    if not o <= din[t]: din[t] |= o; work.append(t)
        return din, dout
    if len(WR) < 64:
        # 64-bit pass-through of the registers the function has not written (their R[] still holds the caller's full value: a 64-bit
        # struct argument, a callee-saved register of the caller): sd / sq store R[n]; ld / lq load R[n] and the local (RCLD); a move
        # of such a register copies R[] too (RCMV).  Neither RCLD nor RCMV counts as a write (the local is the truncation; R[] is exact).
        info = mkinfo(conv)
        win, _ = flow(info, lambda i, d: set(d) | {n for _, n in info[i][1]})
        for i, s in enumerate(conv):
            unw = lambda n: n not in win[i] and n not in {x for _, x in info[i][1]} and n not in WR
            s = re.sub(r'\bWR64\(([^;]*?), r(\d+)\);', lambda m: 'WR64(%s, R[%s]);' % (m.group(1), m.group(2)) if m.group(2) != '0' and unw(int(m.group(2))) else m.group(0), s)
            s = re.sub(r'\br(\d+) = \(int64_t\)RD64\((.*?)\);', lambda m: 'RCLD(r%s, %s, (int64_t)RD64(%s));' % (m.group(1), m.group(1), m.group(2)) if int(m.group(1)) not in WR else m.group(0), s)
            s = re.sub(r'\br(\d+) = r(\d+) [+|] r0;', lambda m: 'RCMV(r%s, %s, %s);' % (m.group(1), m.group(1), m.group(2)) if m.group(2) != '0' and unw(int(m.group(2))) and int(m.group(1)) not in WR else m.group(0), s)
            s = re.sub(r'\br(\d+) = r0 [+|] r(\d+);', lambda m: 'RCMV(r%s, %s, %s);' % (m.group(1), m.group(1), m.group(2)) if m.group(2) != '0' and unw(int(m.group(2))) and int(m.group(1)) not in WR else m.group(0), s)
            # the store of a qmtc2 / MMI source register (conversion above): R[] already holds the exact value of an unwritten one
            s = re.sub(r'\bR\[(\d+)\] = r\1; ', lambda m: '' if unw(int(m.group(1))) else m.group(0), s)
            conv[i] = s
    info = mkinfo(conv)
    CLEAN = re.compile(r'\bRC(?:LD|MV)\(r(\d+), ')
    def xfer(i, d):
        cs, w, _, _ = info[i]; s = conv[i]; d = set(d)
        cl = [(m.start(), int(m.group(1))) for m in CLEAN.finditer(s)]
        ev = sorted([(p, n, 1) for p, n in w] + [(p, n, 0) for p, n in cl] + [(b, None, 2) for a, b, nm in cs])
        # the calls of a line are alternatives (if / else of the auto-dual sites): after the line, a register is clean only if every
        # alternative stored or reloaded it
        touched = None
        for a, b, nm in cs:
            t = ipo_sets(nm); t = ALLR if t is None else (t[0] | t[1]); touched = t if touched is None else touched & t
            if 'if (' in s[:a]: touched = set()   # MGS2: a call inside a branch (if (c_) { ...; IRQ_POLL(); goto }, BREAK of a division by zero): the fallthrough path stays dirty
        for p, n, k in ev:
            if k == 2: d = d - touched
            elif k == 1: d = d | {n}
            else: d = d - {n}
        return d
    din, dout = flow(info, xfer)
    def xfer_to(i, pos):
        """the dirty set at position pos of line i"""
        cs, w, _, _ = info[i]; s = conv[i]; d = din[i]
        cl = [(m.start(), int(m.group(1))) for m in CLEAN.finditer(s)]
        for p, n, k in sorted([(p, n, 1) for p, n in w] + [(p, n, 0) for p, n in cl] + [(b, None, 2) for a, b, nm in cs]):
            if p >= pos: break
            if k == 2: continue          # an earlier call on the same line is an alternative (if / else of the auto-dual sites): conservative
            elif k == 1: d = d | {n}
            else: d = d - {n}
        return d
    store = lambda ns: ''.join('R[%d] = r%d, ' % (n, n) for n in sorted(ns))
    fid = fa.group(1) if fa else '0'
    retstore = lambda ns: store(ns - SCRATCH if RETSCR else ns) + (''.join('R[%d] = RETPOISON(%d, %s), ' % (n, n, fid) for n in sorted(ns & SCRATCH)) if RETSCR == 'poison' else '')
    load = ''.join(', ' + ldn(n) for n in in_set)
    import os
    DSR = bool(os.environ.get('RC_DSR'))
    def need(name, d):
        """the registers to store before the call: the dirty ones the callee may read"""
        t = ipo_sets(name)
        if DSR: req = DSR_IN.get(name, DSR_ANY) if name.startswith(('P', 'O')) else DSR_ANY; d = (d - CALLEE) | (req & set(regs))
        if t is None: return d
        return d & (t[0] | (t[1] - (t[4] if len(t) > 4 else set()))) if SOUND else d & t[0]
    tail = dout[N - 1] if N else set()
    uncond = lambda i: uncond_line(conv[i], '(r0 == r0)')
    extra = []
    for i, s in enumerate(conv):
        cs, w, _, ret = info[i]; x = [(a, n) for (a, b, name) in cs for n in need(name, xfer_to(i, a))]
        if ret: q = s.index('return;'); x += [(q, n) for n in xfer_to(i, q) if not (RETSCR and n in SCRATCH)]
        extra.append(x)
    live_out, reads = liveness(conv, 'r', info, uncond, tail - SCRATCH if RETSCR else tail, extra)
    def reload(name, i, b):
        t = ipo_sets(name); lv = live_out[i] | {n for p, n in reads[i] if p >= b}
        return ''.join(', ' + ldn(n) for n in in_set if (t is None or n in t[1]) and n in lv)
    res = []
    for i, s in enumerate(conv):
        cs, w, _, ret = info[i]
        if cs:
            o = []; p = 0
            for (a, b, name) in cs:
                d = need(name, xfer_to(i, a))
                inner = s[a + len(name) + 1:b - 1]
                rl = reload(name, i, b)
                if rl and re.search(r'(=|\((u?int(8|16|32|64)_t|float)\))\s*$', s[:a]):
                    raise SystemExit('regcache: the value of %s is used but registers are reloaded after it: %s' % (name, s.strip()))
                o.append(s[p:a]); o.append('(%s%s(%s)%s)' % (store(d), name, inner, rl)); p = b
            o.append(s[p:]); s2 = ''.join(o)
        else: s2 = s
        if ret:
            # a return: the registers dirty at that point (dirty on entry to the line, or written in it after its last call)
            d = xfer_to(i, s.index('return;'))
            if DSR: d = d - CALLEE
            st = retstore(d)
            if st: s2 = s2.replace('return;', '{ %s; return; }' % st.rstrip(', '))
        res.append(s2)
    if all(n in WR for n in regs): decl = '  int64_t ' + ', '.join(['r0 = 0'] + [ldn(n) for n in regs]) + ';'
    else: decl = '  ' + ''.join('int64_t %s; ' % ldn(n) for n in regs if n in WR) + 'int32_t ' + ', '.join(['r0 = 0'] + [ldn(n) for n in regs if n not in WR]) + ';'
    if DSR: tail = tail - CALLEE
    k = 1 if res and res[0].startswith('FN_ENTER(') else 0
    res = res[:k] + [decl] + res[k:] + (['  ' + retstore(tail).rstrip(', ') + ';'] if retstore(tail) else [])
    return '\n'.join(res)
FWRITE = re.compile(r'\bf(\d+) = ')
FNOREL = set(range(20, 32))          # callee-saved FPU registers (EE ABI: f20-f31): unchanged by a call
def process_f(body):
    """the FPU registers F[n] of the function in float locals f<n>, same scheme as the GPRs (ACC and CC stay global: set right before use)"""
    if 'F[' not in body: return body
    regs = sorted({int(x) for x in re.findall(r'\bF\[(\d+)\]', body)})
    in_set = [n for n in regs if n not in FNOREL]
    conv = [ln if ln.startswith('FN_ENTER(') else re.sub(r'\bF\[(\d+)\]', r'f\1', ln) for ln in body.split('\n')]
    N = len(conv); lab = {}
    for i, s_ in enumerate(conv):
        m = re.match(r'^L([0-9a-f]+):;', s_)
        if m: lab[m.group(1)] = i
    info = []
    for s_ in conv:
        cs = _calls(s_); w = [(m.start(), int(m.group(1))) for m in FWRITE.finditer(s_)]
        info.append((cs, w, [lab[g] for g in gotos(s_) if g in lab], 'return;' in s_))
    def xfer(i, d):
        cs, w, _, _ = info[i]; d = set(d)
        touched = None
        for a, b, nm in cs:
            t = ipo_sets(nm); t = ALLF if t is None else (t[2] | t[3]); touched = t if touched is None else touched & t
            if 'if (' in conv[i][:a]: touched = set()   # a call inside a branch: the fallthrough path stays dirty (as for the GPRs)
        for p, n, k in sorted([(p, n, 1) for p, n in w] + [(b, -1, 2) for a, b, nm in cs]):
            d = (d - touched) if k == 2 else d | {n}
        return d
    din = [set() for _ in range(N)]; dout = [None] * N; work = list(range(N))
    while work:
        i = work.pop(); o = xfer(i, din[i])
        if o != dout[i]:
            dout[i] = o
            for t in info[i][2] + ([i + 1] if i + 1 < N else []):
                if not o <= din[t]: din[t] |= o; work.append(t)
    def at(i, pos):
        cs, w, _, _ = info[i]; return din[i] | {n for p, n in w if p < pos}
    store = lambda ns: ''.join('F[%d] = f%d, ' % (n, n) for n in sorted(ns))
    fa = re.match(r'FN_ENTER\((0x[0-9a-f]+)\)', body); fid = fa.group(1) if fa else '0'
    fretstore = lambda ns: store(ns - FSCRATCH if RETSCR else ns) + (''.join('F[%d] = RETPOISONF(%d, %s), ' % (n, n, fid) for n in sorted(ns & FSCRATCH)) if RETSCR == 'poison' else '')
    fin = lambda t: (t[2] | (t[3] - (t[5] if len(t) > 5 else set()))) if SOUND else t[2]   # RC_SOUND: FIN | (FOUT - FMOUT)
    tail = dout[N - 1] if N else set()
    uncond = lambda i: uncond_line(conv[i], '(r0 == r0)')
    extra = []
    for i, s_ in enumerate(conv):
        cs, w, _, ret = info[i]; x = []
        for (a, b, name) in cs: t = ipo_sets(name); x += [(a, n) for n in (at(i, a) if t is None else at(i, a) & fin(t))]
        if ret: q = s_.index('return;'); x += [(q, n) for n in at(i, q) if not (RETSCR and n in FSCRATCH)]
        extra.append(x)
    live_out, reads = liveness(conv, 'f', info, uncond, tail - FSCRATCH if RETSCR else tail, extra)
    res = []
    for i, s_ in enumerate(conv):
        cs, w, _, ret = info[i]
        if cs:
            o = []; p = 0
            for (a, b, name) in cs:
                t = ipo_sets(name); d = at(i, a) if t is None else at(i, a) & fin(t)
                lv = live_out[i] | {n for p, n in reads[i] if p >= b}
                ld = ''.join(', f%d = F[%d]' % (n, n) for n in in_set if (t is None or n in t[3]) and n in lv)
                o.append(s_[p:a]); o.append('(%s%s%s)' % (store(d), s_[a:b], ld)); p = b
            o.append(s_[p:]); s2 = ''.join(o)
        else: s2 = s_
        if ret:
            d = at(i, s_.index('return;'))
            st = fretstore(d)
            if st: s2 = s2.replace('return;', '{ %s; return; }' % st.rstrip(', '))
        res.append(s2)
    decl = '  float ' + ', '.join('f%d = F[%d]' % (n, n) for n in regs) + ';'
    k = 1 if res and res[0].startswith('FN_ENTER(') else 0
    return '\n'.join(res[:k] + [decl] + res[k:] + (['  ' + fretstore(tail).rstrip(', ') + ';'] if fretstore(tail) else []))
CALLSITE = re.compile(r'\bP([0-9a-f]+)\(\);')
def inline_pass(src):
    """small straight-line leaf functions (no call, label, branch or stack frame; at most INLINE_MAX guest instructions) are expanded at
    their direct call sites: the register store / reload around the call and the callee's entry / exit go away.  The function itself stays
    (indirect calls).  The expanded copy keeps its /*addr*/ comments; FN_ENTER of the callee is dropped (the profile charges the caller)."""
    import os
    mx = int(os.environ.get('INLINE_MAX', '24'))
    if os.environ.get('NOINLINE'): return src
    bodies = {m.group(1): m.group(2) for m in FN.finditer(src)}
    cand = {}
    for name, body in bodies.items():
        if not name.startswith('P'): continue
        lines = [l for l in body.split('\n') if not l.startswith('FN_ENTER(') and l.strip() and not re.match(r'^/\*[0-9a-f]+\*/\s*$', l)]
        code = [l for l in lines if re.match(r'^(/\*[0-9a-f]+\*/ \S|L[0-9a-f]+:;)', l)]
        if len([l for l in code if not l.startswith('L')]) > mx or len(code) != len(lines): continue
        text = '\n'.join(code)
        if re.search(r'\b(P[0-9a-f]+|X_[0-9a-f]+|O[0-9a-f]+|JALR|VUM0?|VCALLMS|YO_AUTO)\(|R\[31\]|\(uint64_t\)R|switch', text, re.M): continue
        if is_wide(body): continue
        cand[name] = code
    n = [0]
    def rep(m):
        name = 'P' + m.group(1)
        if name not in cand: return m.group(0)
        n[0] += 1; k = n[0]
        code = []
        for l in cand[name]:
            l = re.sub(r'\bL([0-9a-f]+)(:;|;)', lambda mm: 'L%s_i%d%s' % (mm.group(1), k, mm.group(2)), l)
            l = l.replace('return;', 'goto Lend_i%d;' % k)
            code.append(l)
        return '{ /* inlined %s */ ' % name + ' '.join(code) + ' Lend_i%d:; }' % k
    out = []
    for m in FN.finditer(src):
        pass
    src = CALLSITE.sub(rep, src)
    print('inline: %d candidates, %d call sites expanded' % (len(cand), n[0]))
    return src
def run(path):
    src = open(path).read()
    src = inline_pass(src)
    import os as _os
    if _os.environ.get('RC_DUMP'): open(_os.environ['RC_DUMP'], 'w').write(src)
    # the scratchpad base (lui 0x7000 in ComputeSkeletalHierarchy 0x19e21c, the only one in the set): guest address 0, where the masked
    # accessor mapped 0x7000xxxx (guest.h), so the unmasked accessor (GUEST_NOMASK) sees in-range addresses
    src = src.replace('/*19e21c*/ R[2] = 1879048192;', '/*19e21c*/ R[2] = 0;   /* lui 0x7000: the scratchpad, at guest 0 (regcache.py) */')
    import os
    if not os.environ.get('RC_NONARROW'):   # 64 -> 32-bit narrowing of the doubleword-shift idioms (narrow.py), before the width decision
        import narrow
        bodies = {m.group(1): m.group(2) for m in FN.finditer(src)}
        ipo = narrow.ipo_abi(bodies, (lambda b, look: ipo_function_sound(b, look)[:4]) if SOUND else ipo_function); narrow.MUST.clear(); narrow.MUST.update(must_abi_sound(bodies) if SOUND else narrow.must_abi(bodies))
        tot = [0, 0]
        def nf(m):
            b, k = narrow.narrow(m.group(2), ipo); tot[0] += k; tot[1] += k > 0
            return 'static void %s(void)\n{\n%s\n}' % (m.group(1), b)
        w0 = sum(1 for m in FN.finditer(src) if is_wide(m.group(2)))
        src = FN.sub(nf, src)
        print('narrow: %d dsll32 rewritten in %d functions; 64-bit functions %d -> %d' % (tot[0], tot[1], w0, sum(1 for m in FN.finditer(src) if is_wide(m.group(2)))))
    # the 16-byte alignment mask of sq / lq on the stack: every stack-pointer update of the recompiled code is a multiple of 16 (checked over
    # the disassembly: 5951 addiu $sp, all 16-aligned; no other writes of $sp in the set) and the host starts it at 0x3f000, so
    # ((sp + 16k) & ~15) is sp + 16k: dropping the mask lets the compiler fold the offsets into the stores (exact under that invariant)
    if not os.environ.get('RC_KEEPMASK'):
        k = [0]
        def unmask(m):
            if int(m.group(1)) % 16: return m.group(0)
            k[0] += 1; return '((uint32_t)(R[29] + %s))' % m.group(1)
        src = re.sub(r'\(\(uint32_t\)\(R\[29\] \+ (-?\d+)\)\) & ~15u', unmask, src)
        print('stack sq / lq masks dropped: %d' % k[0])
    n = [0]; wn = [0]
    lo, hi = (int(x) for x in os.environ.get('RC_RANGE', '0,999999').split(','))
    if os.environ.get('RC_DSR'):
        dn = [0]; cnt = [0]; dlo, dhi = (int(x) for x in os.environ.get('RC_DSR_RANGE', '0,999999').split(','))
        bodies = {}
        def g(m):
            cnt[0] += 1
            if not (dlo <= cnt[0] - 1 < dhi): b, k = m.group(2), 0
            else: b, k = dsr(m.group(2)); dn[0] += k
            bodies[m.group(1)] = b
            return 'static void %s(void)\n{\n%s\n}' % (m.group(1), b)
        src = FN.sub(g, src)
        # the interprocedural fixpoint: a call reads what its callee reads (an indirect call or a host function: everything)
        DSR_ANY.update(CALLEE)
        look = lambda name: DSR_IN.get(name, CALLEE) if name.startswith(('P', 'O')) else CALLEE
        for name, b in bodies.items(): DSR_IN[name] = set()
        changed = True; rounds = 0
        while changed and rounds < 30:
            changed = False; rounds += 1
            for name, b in bodies.items():
                v = dsr_in(b, look)
                if v != DSR_IN[name]: DSR_IN[name] = v; changed = True
        print('dead save removal: %d callee-saved saves dropped; interprocedural input sets after %d rounds: %d functions read a callee-saved register' % (dn[0], rounds, sum(1 for v in DSR_IN.values() if v)))
    if not os.environ.get('RC_NOIPO'):
        import json
        for fn in filter(None, os.environ.get('RC_IPO_LOAD', '').split(',')):
            for k, v in json.load(open(fn)).items():
                if SOUND and XUNIT and len(v) < 6: sys.exit('regcache: %s was saved without RC_SOUND (no must-write sets)' % fn)
                XIPO[k] = tuple(set(x) for x in (v if SOUND else v[:4]))
        bodies = {m.group(1): m.group(2) for m in FN.finditer(src)}
        r = ipo_run(bodies)
        print('interprocedural sets: %d functions, %d rounds; average |IN| %.1f |OUT| %.1f%s%s' % (len(IPO), r, sum(len(v[0]) for v in IPO.values()) / max(1, len(IPO)), sum(len(v[1]) for v in IPO.values()) / max(1, len(IPO)),
              '; cross-unit sets read: %d' % len(XIPO) if XIPO else '', '; poll reads %s / %s' % (sorted(POLL_SETS[0]), sorted(POLL_SETS[2])) if POLL_SETS[0] != ALLR or POLL_SETS[2] != ALLF else ''))
        if os.environ.get('RC_IPO_SAVE'):
            json.dump({k: [sorted(x) for x in v] for k, v in IPO.items()}, open(os.environ['RC_IPO_SAVE'], 'w'))
        if os.environ.get('RC_IPO_ONLY'): return
    def f(m):
        n[0] += 1
        if not (lo <= n[0] - 1 < hi): return m.group(0)
        NARROW_IDX[0] = n[0] - 1; wn[0] += is_wide(m.group(2))
        b = process(m.group(2))
        if not os.environ.get('RC_NOF'): b = process_f(b)
        return 'static void %s(void)\n{\n%s\n}' % (m.group(1), b)
    src = FN.sub(f, src)
    open(path, 'w').write(src)
    print('regcache: %d functions, %d with 64-bit locals' % (n[0], wn[0]))
if __name__ == '__main__': run(sys.argv[1])
