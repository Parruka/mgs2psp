#!/usr/bin/env python3
"""wpd.py (dev, recomp/lean_plan.md step 2): per-register widths for every translated function, proved on regcache.py's own output,
by demand: a narrow (int32_t) local must become wide (int64_t) where it may hold a value other than a sign-extended 32-bit one (W, from
wpo2.py's phase 1) and some full use needs that value.

Demand runs backwards over regcache's output (wpo2.py's parse): a full use (a compare, a 64-bit shift right, mthi / mtlo, sdl / sdr: F;
a 64-bit store to memory: Q, the stack's too (exact memory, WPD_EXM=1, the default: MGS2's code reads stale stack words,
speed_notes §15 / §40, so what a store leaves there is state like any other; WPD_EXM=0, dev only: a store to the function's own
tracked stack slots needs what later reads of the slot need); a runtime helper that reads a register in full: qmtc2,
the MMI helpers, the quadword-copy idiom's entry check) demands the full value of its local or R[] register.  Demand on a wide local
passes to what made its value: the sources of a 64-bit move / logical / add / shift left (D), R[n] for a reload, a stack slot for an ld
of the frame (and the value stored there); demand on a narrow local stops: it holds the exact value where that value is sign-extended
(S), and where it may be W the local is a cut point: it becomes wide.  32-bit operations, loads from memory and HI / LO give exact values.
Calls: per function a summary of its entry demand, unconditional (U: what its own full uses need of R[] at its entry) and per register
at its returns (T[r]: the entry registers whose full values reach R[r] at a return); a demand after a call passes through T into the
caller.  A demand on R[r] after a call is the callee's return demand (DRET, the union over its call sites); a cut point reached only
from a return register r is widened when some caller demands r.  Calls through a register use the union over the address-taken
functions (taken.py) of the summaries; every such function gets the union of the demands after those calls.
Assumptions: wpo2.py's (A1 option, A2 save slots, SYSCALL results S, frames written only through taken addresses) and: the runtime
reads the argument registers of SYSCALL and the kernel calls as 32-bit values (boot.c SYSCALL, kernel.h).  A stack slot read before
the function writes it is W (whatever an earlier call left there; with exact memory it is the same bytes as v27's).
usage: wpd.py <out.json> [unit ...]"""
import os, sys, json, time, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wpo2 as W
D = W.D
UB = 1 << 32                 # an unconditional demand; bits 0-31: demanded when a caller demands that register at the return
LAB = [None]                 # dev (wpd_why.py): {key: (bit, text)}, each unconditional demand's source its own bit above 32
def ub(key, text):
    if LAB[0] is None: return UB
    x = LAB[0].get(key)
    if x is None: x = LAB[0][key] = (33 + len(LAB[0]), text)
    return 1 << x[0]
IDT = tuple([0] + [1 << r for r in range(1, 32)])
Z32 = (0,) * 32
EXM = int(os.environ.get('WPD_EXM', '1'))   # exact memory: every 64-bit store, the stack's too, needs the full value (0: dev, stack slots by demand)
NOIND = int(os.environ.get('WPO_NOIND', '0'))   # dev bound (not sound): calls through a register neither need nor pass on full values
# WPD_V32=<widths json> (the hybrid's 32-bit part): in the functions that json gives no wide register (v27's 32-bit functions), what the
# function receives is S (v27's NARROW assumption, which GEN_NCHK checks on the routes): no W at its entry and none made by a callee; W
# only from its own loads (an ld / lq of a stack slot not known S).  The widths found there keep the full values those loads make (a
# quadword copy through a register: P125fd8, recomp/widths_step2.md); the other functions' widths are taken from the full proof.
V64 = None
if os.environ.get('WPD_V32'): V64 = {k for k, v in json.load(open(os.environ['WPD_V32']))['wr'].items() if v}
def narrow_in(name): return V64 is not None and name not in V64
def capply_nin(v, S):
    """W.capply for a narrow-in function: only the caller's own W values that the callee passes through (rows D), none it makes (G)"""
    nv = v & S[2]
    for r, g, d, src in S[4]:
        if d & v: nv |= 1 << r
    return nv
WIDE = {}                    # name -> frozenset of the registers with int64_t locals
FW = {}                      # name -> wfwd() result
UNKNOWN_CALLS = collections.Counter()
def bits(x):
    while x:
        lo = x & -x; yield lo.bit_length() - 1; x ^= lo

# ---- forward: the W bit of each definition, the stack pointer at each definition / stack store ---------------------------------
def wfwd(F):
    name = F.name; N = F.nb
    nin = narrow_in(name)
    e0 = 0 if nin else W.E.get(name, 0) | (W.G_['E_IND'] if F.ind else 0)
    wat = {}; spat = {}; unk = False
    if not N: return e0, wat, spat, unk
    din = [None] * N; din[0] = (e0, 0, frozenset()); work = [0]; inq = {0}
    while work:
        i = work.pop(); inq.discard(i)
        v, sp, sl = din[i]; ev, falls, _ = F.blocks[i]
        for j, (kind, d, c) in enumerate(ev):
            if kind == 'd':
                n, cls, deps = d[:3]; t = cls[0]
                if t == 'REL': wat[(i, j)] = v >> n & 1; spat[(i, j)] = sp; continue
                if t == 'S': w = 0
                elif t == 'MV': w = v >> cls[1] & 1
                elif t == 'P': w = 1 if v & cls[1] else 0
                elif t == 'HL': w = v >> W.HLR[cls[1]] & 1
                elif t == 'LDS':
                    if sp is None: w = 1; unk = True
                    else:
                        key = sp + cls[1]
                        w = 1 if (F.taken and key not in F.savek) else (0 if key in sl else 1)
                elif t == 'SP': w = 0
                else: w = 1
                wat[(i, j)] = w; spat[(i, j)] = sp
                if t == 'SP' and n == 29:
                    if c: sp = None; sl = frozenset()
                    elif sp is not None: sp += cls[1]
                if n == 29 and t != 'SP': sp = None; sl = frozenset()
                v = (v | (w << n)) if c else ((v & ~(1 << n)) | (w << n))
            elif kind == 'hw':
                bits_, val = d
                w = (v >> val[1] & 1) if val[0] == 'R' else (1 if val[0] == 'W' else 0)
                for b_, r in W.HLR.items():
                    if bits_ & b_: v = (v | (w << r)) if c else ((v & ~(1 << r)) | (w << r))
            elif kind == 'sr': spat[(i, j)] = sp
            elif kind == 'st':
                k, val = d; spat[(i, j)] = sp
                if sp is None: continue
                key = sp + k
                w = (v >> val[1] & 1) if val[0] in ('R', 'L') else 1
                if w: sl = sl - {key}
                elif not c: sl = sl | {key}
            elif kind == 'c':
                nm, rin, rout, mout = d; spat[(i, j)] = sp
                if nm in ('BREAK', 'COP0OP', 'COP0W'): continue
                nv = (capply_nin if nin else W.capply)(v, W.callsum(nm, rout, mout))
                v = (v | nv) if c else nv
            elif kind == 'g':
                if d >= 0: W.merge(din, d, (v, sp, sl), work, inq)
        if falls and i + 1 < N: W.merge(din, i + 1, (v, sp, sl), work, inq)
    return e0, wat, spat, unk

# ---- call summaries: (T, U); T[r] the entry registers (mask) whose full values reach R[r] at a return, U the unconditional ones -----
BOT = (Z32, 0)
INF = 1 << 30
SARG = {}; SARG_IND = [None]
def sarg_fn(F):
    """the bytes of its caller's frame (entry-relative slots 0 and up) F reads or writes by its own sp; INF if its frame is taken or
    its sp is unknown at a stack access"""
    if F.taken: return INF
    e0, wat, spat, unk = wfwd(F)
    if unk: return INF
    ext = 0
    for i, (ev, falls, _) in enumerate(F.blocks):
        for j, (kind, d, c) in enumerate(ev):
            if kind == 'd' and d[1][0] == 'LDS': k, nb = d[1][1], 8
            elif kind == 'sr': k, nb = d
            elif kind == 'st': k, nb = d[0], 8
            else: continue
            sp = spat.get((i, j))
            if sp is None: return INF
            if sp + k >= 0: ext = max(ext, sp + k + nb)
    return ext
def sarg(nm):
    if nm == 'JALR' or nm == 'JSIF':
        if SARG_IND[0] is None:
            ts = [t for t in W.FUNCS if W.FUNCS[t].ind] if nm == 'JALR' else [t for t in W.SIF if t in W.FUNCS]
            SARG_IND[0] = max([sarg(t) for t in ts] + [0]) if all(t in W.FUNCS for t in ts) else INF
        return SARG_IND[0] if nm == 'JALR' else max([sarg(t) for t in W.SIF if t in W.FUNCS] + [0])
    if nm not in W.FUNCS: return INF
    if nm not in SARG: SARG[nm] = sarg_fn(W.FUNCS[nm])
    return SARG[nm]
SALL, SCALL, SRET = 'all', 'call', 'ret'      # DS keys: every slot (a read with sp unknown); every slot but the save slots (a call); 0 and up
UPST = collections.Counter()                 # dev: stores to entry-relative slots 0 and up (the caller's frame) per function
SUM = {}; SUMIND = [BOT]; _SIF = [None]; _HOST = {}
def host(rout, mout):
    k = (rout, mout); s = _HOST.get(k)
    if s is None: s = _HOST[k] = (tuple(0 if mout >> r & 1 else IDT[r] for r in range(32)), 0)
    return s
def sjoin(a, b):
    if a is b: return a
    return (tuple(x | y for x, y in zip(a[0], b[0])), a[1] | b[1])
def csum(nm, rout, mout):
    if nm in W.FUNCS: return SUM.get(nm, BOT)
    if nm == 'JALR': return (IDT, 0) if NOIND else SUMIND[0]
    if nm == 'JSIF':
        if _SIF[0] is None:
            s = BOT
            for t in W.SIF:
                if t in W.FUNCS: s = sjoin(s, SUM.get(t, BOT))
            _SIF[0] = s
        return _SIF[0]
    if nm.startswith(('P', 'S_', 'O')):
        UNKNOWN_CALLS[nm] += 1
        return host(rout, rout)     # a unit not read in this run: its writes taken as exact (dev runs only; the full run reads all)
    return host(rout, mout)

class Res:
    __slots__ = ('T', 'U', 'sites', 'cuts', 'D0')

def dback(F, fw):
    """F's demand: Res with T, U (its summary), sites {(block, event): (callee, {r: mask after the call})}, cuts {key: (n, mask, line)}"""
    e0, wat, spat, unk = fw
    name = F.name; wide = WIDE[name]; N = F.nb
    R_ = Res(); R_.sites = {}; R_.cuts = {}
    if not N:
        R_.T = IDT; R_.U = 0; R_.D0 = None; return R_
    succ = [[] for _ in range(N)]
    for i, (ev, falls, _) in enumerate(F.blocks):
        for kind, d, c in ev:
            if kind == 'g' and d >= 0: succ[i].append(d)
        if falls and i + 1 < N: succ[i].append(i + 1)
    reach = [False] * N; reach[0] = True; st = [0]      # blocks no path from the entry reaches (code after a tail jump) take no part
    while st:
        i = st.pop()
        for t in succ[i]:
            if not reach[t]: reach[t] = True; st.append(t)
    pred = [[] for _ in range(N)]
    for i in range(N):
        if reach[i]:
            for t in succ[i]: pred[t].append(i)
    RX = (Z32, IDT, {})
    din = [None] * N; work = [i for i in range(N) if reach[i]]; inq = set(work)
    while work:
        i = work.pop(); inq.discard(i); ev, falls, li = F.blocks[i]
        # the demand at the block's end
        if falls:
            o = din[i + 1] if i + 1 < N else RX
            if o is None: o = (Z32, Z32, {})
        else: o = (Z32, Z32, {})
        DL = list(o[0]); DR = list(o[1]); DS = dict(o[2])
        for j in range(len(ev) - 1, -1, -1):
            kind, d, c = ev[j]
            if kind == 'x':     # the caller's frame (entry-relative slots 0 and up) stays readable after the return: SRET
                if c:
                    for r in range(1, 32): DR[r] |= IDT[r]
                    DS[SRET] = DS.get(SRET, 0) | ub((i, j, 'x'), 'return (line %d): the caller\'s frame' % li)
                else: DL = [0] * 32; DR = list(IDT); DS = {SRET: ub((i, j, 'x'), 'return (line %d): the caller\'s frame' % li)}
            elif kind == 'g':
                if d < 0: continue
                t = din[d]
                if t is None:
                    if not c: DL = [0] * 32; DR = [0] * 32; DS = {}
                    continue
                if c:
                    DL = [x | y for x, y in zip(DL, t[0])]; DR = [x | y for x, y in zip(DR, t[1])]
                    for k_, m in t[2].items(): DS[k_] = DS.get(k_, 0) | m
                else: DL = list(t[0]); DR = list(t[1]); DS = dict(t[2])
            elif kind == 'u':
                n, k = d
                if k == 'F' or k == 'Q' or (EXM and k == 'QS'): DL[n] |= ub((i, j), '%s use of r%d (line %d)' % (k, n, li))
            elif kind == 'ru':
                n, k = d
                if k == 'F' or k == 'Q': DR[n] |= ub((i, j), '%s use of R[%d] (line %d)' % (k, n, li))
            elif kind == 'rset':
                if not c: DR[d] = 0
            elif kind == 's':
                if DR[d]: DL[d] |= DR[d]
                if not c: DR[d] = 0
            elif kind == 'd':
                n, cls, deps, mode = d; t = cls[0]
                m = DL[n]
                if not c: DL[n] = 0
                mr = 0
                if mode == 'rcld' or mode == 'rcmv':
                    mr = DR[n]
                    if not c: DR[n] = 0
                if m:
                    if n not in wide:
                        if wat.get((i, j), 1):      # a block the forward pass never reached (no path from the entry): taken as W
                            o_ = R_.cuts.get((i, j))
                            R_.cuts[(i, j)] = (n, m | (o_[1] if o_ else 0), li)
                    elif t == 'REL' or mode == 'glue': DR[n] |= m
                    elif t == 'LDS':
                        sp = spat.get((i, j))
                        if sp is not None:
                            key = sp + cls[1]
                            if not (F.taken and key not in F.savek): DS[key] = DS.get(key, 0) | m
                    elif deps:
                        for s_ in bits(deps): DL[s_] |= m
                if mr:
                    if mode == 'rcmv': DR[cls[1]] |= mr
                    elif t == 'LDS':
                        sp = spat.get((i, j))
                        if sp is not None:
                            key = sp + cls[1]
                            if not (F.taken and key not in F.savek): DS[key] = DS.get(key, 0) | mr
            elif kind == 'sr':      # a read of the stack other than a 64-bit load into a local: the slots whose upper word it reads
                k0, nb = d; sp = spat.get((i, j))
                if sp is None or unk: DS[SALL] = DS.get(SALL, 0) | ub((i, j), 'read of the stack, sp unknown (line %d)' % li); continue
                for s8 in range(k0 & ~7, k0 + nb, 8):
                    if k0 < s8 + 8 and k0 + nb > s8 + 4:
                        key = sp + s8; DS[key] = DS.get(key, 0) | ub((i, j, s8), 'read of the stack at sp%+d (line %d)' % (k0, li))
            elif kind == 'st':
                k, val = d
                if val[0] != 'L' and val[0] != 'R': continue      # a part of the slot, or not a register: the bytes written are exact
                if EXM and val[0] == 'R': DR[val[1]] |= ub((i, j), 'store of R[%d] to the stack (line %d)' % (val[1], li))
                sp = spat.get((i, j))
                if sp is None or unk or (F.taken and (sp + k) not in F.savek): m = ub((i, j), 'store of %s%d to the stack, sp unknown or frame taken (line %d)' % (val[0], val[1], li))
                else:
                    key = sp + k; m = DS.get(key, 0) | DS.get(SALL, 0)
                    if key not in F.savek: m |= DS.get(SCALL, 0)
                    if key >= 0: m |= DS.get(SRET, 0); UPST[name] += 1
                    if not c: DS.pop(key, None)
                if m:
                    if val[0] == 'L': DL[val[1]] |= m
                    else: DR[val[1]] |= m
            elif kind == 'c':
                nm, rin, rout, mout = d
                if nm in ('BREAK', 'COP0OP', 'COP0W'): continue
                # a callee may read its caller's frame by its own sp (arguments on the stack): the slots [sp, sp + sarg(callee)); with
                # the extent unknown (a taken frame, a host call), every slot but the register save slots (A2)
                a_ = sarg(nm); spc = spat.get((i, j))
                if a_ >= INF or spc is None or unk: DS[SCALL] = DS.get(SCALL, 0) | ub((i, j, 'c'), 'call of %s (line %d) may read the frame' % (nm, li))
                else:
                    for s8 in range(spc & ~7, spc + a_, 8): DS[s8] = DS.get(s8, 0) | ub((i, j, 'c', s8), 'call of %s (line %d) reads sp%+d' % (nm, li, s8 - spc))
                aft = {r: DR[r] for r in bits(rout & ~1) if DR[r]}
                if aft:
                    o_ = R_.sites.get((i, j))
                    if o_ is None: R_.sites[(i, j)] = (nm, aft)
                    else:
                        for r, a in aft.items(): o_[1][r] = o_[1].get(r, 0) | a
                T, U = csum(nm, rout, mout)
                nDR = list(DR) if c else [0] * 32
                for r in range(1, 32):
                    a = DR[r]
                    if a:
                        tr = T[r]
                        if tr == IDT[r]: nDR[r] |= a
                        else:
                            for s_ in bits(tr): nDR[s_] |= a
                for s_ in bits(U): nDR[s_] |= ub((i, j, s_), '%s needs R[%d] (line %d)' % (nm, s_, li))
                DR = nDR
        new = (tuple(DL), tuple(DR), DS)
        old = din[i]
        if old is not None:
            new = (tuple(x | y for x, y in zip(old[0], new[0])), tuple(x | y for x, y in zip(old[1], new[1])), dict(old[2]))
            for k_, m in DS.items(): new[2][k_] = new[2].get(k_, 0) | m
            if new[0] == old[0] and new[1] == old[1] and new[2] == old[2]: continue
        din[i] = new
        for p in pred[i]:
            if p not in inq: work.append(p); inq.add(p)
    DL0, DR0, _ = din[0] if din[0] is not None else (Z32, Z32, {})
    DR0 = list(DR0)
    for n in range(1, 32):
        m = DL0[n]
        if not m: continue
        if n not in wide:
            if e0 >> n & 1:
                o_ = R_.cuts.get(('entry', n))
                R_.cuts[('entry', n)] = (n, m | (o_[1] if o_ else 0), 'entry')
        else: DR0[n] |= m
    if F.idiom:     # rt_idiom_qcopy at the entry: reads R[4..6] in full; when it copies, returns with R[2..7] written (exact)
        for n in (4, 5, 6): DR0[n] |= ub(('idiom', n), 'idiom check of R[%d]' % n)
        for r in range(1, 32):
            if not 2 <= r <= 7: DR0[r] |= IDT[r]
    T = [0] * 32; U = 0
    for s_ in range(1, 32):
        m = DR0[s_]
        if not m: continue
        if m >> 32: U |= 1 << s_
        for r in bits(m & (UB - 1)): T[r] |= 1 << s_
    R_.T = tuple(T); R_.U = U; R_.D0 = tuple(DR0) if LAB[0] is not None else None   # dev tools: the entry demands with their labels
    return R_

# ---- the whole program -------------------------------------------------------------------------------------------------------------
RES = {}
def summaries(order, t0):
    """the summaries' fixpoint from the current ones (they only grow), starting with the functions in order"""
    callers = W.CG['callers']; jalrs = W.CG['jalrs']
    work = collections.deque(order); inq = set(order); ev = 0; done = set()
    while work:
        name = work.popleft(); inq.discard(name); F = W.FUNCS[name]; ev += 1; done.add(name)
        fw = FW.get(name)
        if fw is None: fw = FW[name] = wfwd(F)
        r = dback(F, fw); RES[name] = r
        o = SUM.get(name); s = (r.T, r.U)
        if o is not None: s = sjoin(o, s)
        if o is not None and s == o: continue
        SUM[name] = s; push = list(callers[name])
        if name in W.SIF: _SIF[0] = None
        if F.ind:
            si = SUMIND[0]; ni = sjoin(si, s)
            if ni != si: SUMIND[0] = ni; push += jalrs
        for p in push:
            if p not in inq: work.append(p); inq.add(p)
        if ev % 20000 == 0: print('  summaries: %d evaluations, %d queued (%.0f s)' % (ev, len(work), time.time() - t0), flush=True)
    return ev, done

DRET = collections.defaultdict(int); DIND = [0]; DSIF = [0]
def dret(name):
    F = W.FUNCS[name]
    return DRET[name] | (DIND[0] if F.ind else 0) | (DSIF[0] if name in W.SIF else 0)
def dret_fix(names):
    """the return demands' fixpoint (they only grow), starting with the functions in names"""
    opens = W.CG['opens']; sif = [t for t in W.SIF if t in W.FUNCS]
    work = collections.deque(names); inq = set(names)
    while work:
        f = work.popleft(); inq.discard(f); act = dret(f)
        for nm, aft in RES[f].sites.values():
            m = 0
            for r, a in aft.items():
                if a >> 32 or a & act: m |= 1 << r
            if not m: continue
            if nm == 'JALR':
                if NOIND: continue
                if m & ~DIND[0]:
                    DIND[0] |= m
                    for g in opens:
                        if g not in inq: work.append(g); inq.add(g)
            elif nm == 'JSIF':
                if m & ~DSIF[0]:
                    DSIF[0] |= m
                    for g in sif:
                        if g not in inq: work.append(g); inq.add(g)
            elif nm in W.FUNCS:
                if m & ~DRET[nm]:
                    DRET[nm] |= m
                    if nm not in inq: work.append(nm); inq.add(nm)

def run(units, t0):
    """the widths' fixpoint over units (WIDE); returns why {function: {register: text}}"""
    keep = set(os.environ.get('WPO_KEEP', 'P12dbc0,S_w00a_P333380').split(','))
    W.load_all(units, t0); W.phase1(t0); US = W.CG['US']
    UNIT = {name: F.unit for name, F in W.FUNCS.items()}
    for name in W.FUNCS:
        if name in keep:
            U = US[UNIT[name]]
            WIDE[name] = frozenset(range(64)) if D.IS_WIDE(U.bodies[name]) else frozenset()
            if WIDE[name]:
                D.ctx(U); W.FUNCS[name] = W.parse(name, U, D.emit(U, name, set(WIDE[name])), set(WIDE[name]))
        else: WIDE[name] = frozenset()
    order = W.postorder(list(W.FUNCS), W.CG['callees'])
    why = collections.defaultdict(dict); it = 0; work = order; dwork = list(W.FUNCS)
    while True:
        it += 1
        ev, done = summaries(work, t0)
        dret_fix(list(done) if it > 1 else dwork)
        widen = collections.defaultdict(set); stuck = collections.Counter(); ncut = 0
        for name, r in RES.items():
            act = dret(name)
            for key, (n, m, li) in r.cuts.items():
                if not (m >> 32 or m & act) or n in WIDE[name]: continue
                ncut += 1
                if name in keep: stuck[name] += 1; continue
                widen[name].add(n)
                if n not in why[name]: why[name][n] = '%s line %s (%s)' % ('cut' if key[0] != 'entry' else 'entry', li, 'U' if m >> 32 else 'R' + ','.join(str(b) for b in bits(m & act & (UB - 1))))
        nw = sum(len(x) for x in widen.values())
        print('round %d: %d evaluations, %d demanded cut points, %d locals widened in %d functions, stuck %s; DIND %s (%.0f s)' %
              (it, ev, ncut, nw, len(widen), dict(stuck), W.rregs(DIND[0]), time.time() - t0), flush=True)
        if not widen: break
        for f, ns in widen.items():
            WIDE[f] = WIDE[f] | frozenset(ns)
            U = US[UNIT[f]]; D.ctx(U)
            W.FUNCS[f] = W.parse(f, U, D.emit(U, f, set(WIDE[f])), set(WIDE[f])); FW.pop(f, None)
        work = [f for f in order if f in widen]
        if it > 200: print('no fixpoint'); break
    if UNKNOWN_CALLS: print('calls into units not read (taken as exact): %d names' % len(UNKNOWN_CALLS))
    return why

STATE_KEYS = ('SUMS', 'SUMIND', 'E', 'G_', 'INDN')
def save_state(path, units):
    """dev (WPD_STATE=<file>): the fixpoint's state for the investigation tools (tools/wpstate.py): phase 1's summaries and entry sets,
    the widths, the demand summaries and return demands, the call graph; the functions are parsed again on demand"""
    import pickle
    st = {'units': units, 'W': {k: getattr(W, k) for k in STATE_KEYS}, 'WIDE': WIDE, 'SUM': SUM, 'SUMIND': SUMIND[0], 'DRET': dict(DRET),
          'DIND': DIND[0], 'DSIF': DSIF[0], 'CG': {k: v for k, v in W.CG.items() if k != 'US'},
          'UNIT': {n: F.unit for n, F in W.FUNCS.items()}}
    with open(path, 'wb') as f: pickle.dump(st, f, protocol=pickle.HIGHEST_PROTOCOL)

def main():
    out = sys.argv[1]; units = sys.argv[2:] or D.UNITS; t0 = time.time()
    why = run(units, t0)
    xs = [v for v in SARG.values()]
    print('stack arguments: callees read or write their caller\'s frame: %d of %d with an extent (%d bytes at most), %d unknown (taken frame or '
          'sp); stores to the caller\'s frame by sp in %d functions %s' % (sum(1 for v in xs if 0 < v < INF), len(xs), max([v for v in xs if v < INF] + [0]),
          sum(1 for v in xs if v >= INF), len(UPST), sorted(UPST)[:8]))
    if os.environ.get('WPD_STATE'): save_state(os.environ['WPD_STATE'], units); print('state saved (%.0f s)' % (time.time() - t0))
    json.dump({'wr': {k: sorted(v) for k, v in WIDE.items()}, 'why': {k: {str(a): b for a, b in v.items()} for k, v in why.items()},
               'E': {k: W.rregs(v) for k, v in W.E.items() if v}, 'E_IND': W.rregs(W.G_['E_IND']), 'DIND': W.rregs(DIND[0]), 'A1': W.A1},
              open(out, 'w'))
    print('done (%.0f s)' % (time.time() - t0))

if __name__ == '__main__': main()
