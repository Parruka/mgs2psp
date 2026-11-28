#!/usr/bin/env python3
"""gl45 batch 3: PSP instruction slots a step of the hot functions of one PSP debug object, weighted with the twin's counters.
usage: g45_size.py <object (-g, from a tools/g45_split.py header)> <map.json> <keys.json> <counts.json (a list: counter -> times
                   in the window)> <steps in the window> <out.json> [-v function]

Each PSP instruction carries the header line it was compiled from, and the map gives that line's counter on the twin (a guest
instruction: times reached; a taken block: times taken; a wrapper: calls).  A line's count is not its block's count: the compiler
copies and moves code (a loop's test copied in front of the loop still carries the test's line).  So the counts of the PSP code's
own basic blocks are rebuilt, a function at a time, as the flow through its branch graph that
  - enters with the function's calls a step and is conserved at every block (exact conditions);
  - puts nothing on a slow side: a block that calls rt_poll_slow or the untouched copy NAME_s, holds a break, or is a VU0 group's
    fallback (SLOWSYM); a block that calls a memory helper (SLOW) from a line with base-register forms only (their helper call
    is the I/O side by construction); for a plain form (a hardware register read on the main path is one) only when the block
    then joins what the other side of its branch runs into (exact conditions);
  - comes as close as it can to (a) every instruction's own line count, and (b) for each guest conditional branch, the twin's
    taken count summed over the PSP branches compiled from it (their taken sides told by what the targets hold: the taken
    block's or the target label's line against the next instruction's), weight GW each;
the sum of absolute differences is minimised (a linear program; least absolute values, so that a few moved instructions do not
drag a block).  A branch-likely's delay slot counts with its taken side.  A computed jump's targets may take flow from nowhere.
out.json: {function: {"slots": a step, "calls": a step, "bytes": the function's section, "bytes_s": the untouched copy's,
"n": instructions, "misfit": sum over instructions |block count - line count|, "lsum": sum of the line counts, "table": bool}}."""
import subprocess, sys, re, json, collections
import numpy as np
from scipy.optimize import linprog
from scipy.sparse import coo_matrix
obj, mapf, keysf, cntf, steps, outf = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5]), sys.argv[6]
verbose = sys.argv[8] if len(sys.argv) > 8 and sys.argv[7] == "-v" else None
M = json.load(open(mapf)); K = json.load(open(keysf)); D = json.load(open(cntf))
LN = {int(k): v for k, v in M["lines"].items()}; CL = {int(k): v for k, v in M["calls"].items()}
SLOW = {"rt_mm", "rt_store", "rt_st8", "rt_st16", "rt_st32", "rt_st64", "GSDU", "GLDU"}
SLOWSYM = {"&g_vgrp_fb"}      # a VU0 group's fallback (gen_pre.h vu_vgrp: taken on special values only) counts itself here
GW = 50.0
KI = {}; NEXT_AT = {}; GOTOS = {}; ATADDR = {}; CALLS = {}
for fn, (k0, ks) in K.items():
    last = None; aa = ATADDR.setdefault(fn, {})
    for i, (kind, addr, tgt) in enumerate(ks):
        idx = k0 + i; KI[idx] = (kind, addr, tgt)
        if kind == "at":
            if last is not None: NEXT_AT[last] = idx
            last = idx; GOTOS[idx] = []; aa.setdefault(addr, idx)
        elif last is not None: GOTOS[last].append(idx)
    first = ks[0][1] if ks and ks[0][0] == "at" else None
    back = sum(D[k0 + i] for i, (kind, addr, tgt) in enumerate(ks) if kind == "goto" and first and tgt == "L" + first)
    CALLS[fn] = max(D[k0] - back, 0) / steps if ks else 0.0
hot = sorted(set(re.sub(r"_(s|f)$", "", n) for n in M["fn"]))
secs = subprocess.run(["psp-objdump", "-h", obj], capture_output=True, text=True).stdout
size = {}
for m in re.finditer(r"^\s*\d+ (\.text\S*)\s+([0-9a-f]{8})", secs, re.M): size[m.group(1)] = int(m.group(2), 16)
BR = re.compile(r"^(b|bal|beq|bne|beqz|bnez|bgez|bgtz|blez|bltz|bgezal|bltzal|bc1t|bc1f|bvt|bvf)(l?)$")
def lweight(file, line, hdr):
    if file != hdr: return None
    if line in LN: return D[LN[line]] / steps
    if line in CL: return D[CL[line]] / steps
    return None
PLAIN = re.compile(r"\b(RD8|RD16|RD32|RD64|RDF|WR8|WR16|WR32|WR64|WRF|GLDL|GLDR|GLWL|GLWR|GSDL|GSDR|GSWL|GSWR|GLDU|GSDU|VLQC2|VSQC2)\(")
TEXT = {}
def text(hdr, ln):
    if hdr not in TEXT: TEXT[hdr] = open(hdr).read().split("\n")
    return TEXT[hdr][ln - 1] if 0 < ln <= len(TEXT[hdr]) else ""
res = {}
def one(fn, strict=True):
    sec = ".text." + fn
    out = subprocess.run(["psp-objdump", "-dlr", "--inlines", "--no-show-raw-insn", "-j", sec, obj], capture_output=True, text=True).stdout
    ins = []; cur = (None, 0); hdr = None; chain = []
    for l in out.split("\n"):
        m = re.match(r"^(/\S+):(\d+)(?: \(discriminator \d+\))?$", l)
        if m:
            cur = (m.group(1), int(m.group(2)))
            if hdr is None and "/gen/g45/" in m.group(1): hdr = m.group(1)
            continue
        m = re.match(r"^inlined by (/\S+):(\d+)", l)
        if m:
            chain.append((m.group(1), int(m.group(2))))
            if hdr is None and "/gen/g45/" in m.group(1): hdr = m.group(1)
            continue
        m = re.match(r"^\s+([0-9a-f]+):\t(\S+)(?:\t(.*))?$", l)
        if m:     # the line an instruction counts under: its own when that is in the generated header, else the header line it was inlined into
            loc = cur
            if not (cur[0] or "").count("/gen/g45/"):
                loc = next((c for c in chain if "/gen/g45/" in c[0]), cur)
            ins.append([int(m.group(1), 16), m.group(2), m.group(3) or "", loc, None]); chain = []; continue
        m = re.match(r"^\t\t\t[0-9a-f]+: (R_MIPS_\w+)\t(\S+)", l)
        if m and ins: ins[-1][4] = m.group(2) if m.group(1) == "R_MIPS_26" else ("&" + m.group(2))
    if not ins: return None
    idx = {a: i for i, (a, *_) in enumerate(ins)}; n = len(ins)
    leaders = {0}; table = False; term = {}
    for i, (a, op, args, _, rel) in enumerate(ins):
        m = BR.match(op)
        if m:
            t = re.search(r"([0-9a-f]+) <", args); tgt = int(t.group(1), 16) if t else None
            if tgt is not None and tgt in idx: leaders.add(tgt)
            if a + 8 in idx: leaders.add(a + 8)
            uncond = op == "b" or (op in ("beq", "beqz", "bgez", "blez") and re.match(r"zero,(zero,)?[0-9a-f]+ <", args) is not None)
            term[i] = ("b", tgt, None) if uncond else ("c", tgt, a + 8, bool(m.group(2)))
        elif op == "jr":
            if a + 8 in idx: leaders.add(a + 8)
            if not args.strip().startswith("ra"): table = True
            term[i] = ("x",)
        elif op == "j":
            if a + 8 in idx: leaders.add(a + 8)
            term[i] = ("x",)
    L = sorted(leaders); blocks = []
    for k, a in enumerate(L):
        e = L[k + 1] if k + 1 < len(L) else ins[-1][0] + 4
        blocks.append((idx[a], idx[e] if e in idx else n))
    bof = {ins[s][0]: k for k, (s, e) in enumerate(blocks)}; nb = len(blocks)
    slow = [False] * nb; mem = [False] * nb; tm = [None] * nb; lcnt = [None] * nb; evs = [None] * nb
    for k, (s, e) in enumerate(blocks):
        cnt = collections.Counter(); lc = collections.Counter()
        for i in range(s, e):
            a, op, args, (f, ln), rel = ins[i]
            if (op == "jal" and rel and (rel == "rt_poll_slow" or rel.endswith("_s"))) or (op == "j" and rel and rel.endswith("_s")) or op == "break" or (rel in SLOWSYM and op == "sw"): slow[k] = True
            elif op == "jal" and rel in SLOW:
                if strict and f == hdr and not PLAIN.search(text(hdr, ln)): slow[k] = True      # a base-register form's I/O side
                else: mem[k] = True                                                   # a plain form: main path or I/O side (memslow)
            if i in term: tm[k] = (i,) + term[i]
        t = tm[k]; skip = t[0] + 1 if (t is not None and t[1] == "c" and t[4]) else -1      # a branch-likely's delay slot
        for i in range(s, e):
            f, ln = ins[i][3]
            if i == skip: continue
            w = lweight(f, ln, hdr)
            if w is not None: cnt[w] += 1
            if f == hdr and ln in LN: lc[LN[ln]] += 1
        lcnt[k] = lc; evs[k] = cnt
    def succ1(k):
        t = tm[k]
        if t is None: return k + 1 if k + 1 < nb else None
        if t[1] == "b": return bof.get(t[2])
        return None
    def memslow(x, y):
        if x is None or y is None or not mem[x]: return False
        j = succ1(x); q = y
        for _ in range(4):
            if j is None or q is None: return False
            if q == j: return True
            q = succ1(q)
        return False
    def sig(k, want, d=0):
        if k is None: return 0
        c = sum(v for i_, v in lcnt[k].items() if i_ in want)
        if c == 0 and not lcnt[k] and d < 2: return sig(succ1(k), want, d + 1)
        return c
    # edges
    E = []            # (from, to)
    zero = set()      # edges held at 0
    gt = collections.defaultdict(list); gn = collections.defaultdict(list); tk_edge = {}
    for k in range(nb):
        t = tm[k]
        if t is None:
            if k + 1 < nb: E.append((k, k + 1))
            continue
        if t[1] == "x": continue
        if t[1] == "b":
            if t[2] in bof: E.append((k, bof[t[2]]))
            continue
        i, _, tgt, fall, likely = t; T = bof.get(tgt); F = bof.get(fall)
        eT = eF = None
        if T is not None: eT = len(E); E.append((k, T))
        if F is not None: eF = len(E); E.append((k, F))
        tk_edge[k] = eT
        if T is None or F is None: continue
        if memslow(T, F): zero.add(eT); continue
        if memslow(F, T): zero.add(eF); continue
        f, ln = ins[i][3]; ci = LN.get(ln) if f == hdr else None
        if ci is not None and KI[ci][0] == "at" and len(GOTOS.get(ci, [])) == 1 and not slow[T] and not slow[F]:
            g = GOTOS[ci][0]; tk = {g}; la = ATADDR[fn].get(KI[g][2][1:])
            if la is not None: tk.add(la)
            nt = {NEXT_AT[ci]} if ci in NEXT_AT else set()
            a_ = sig(T, tk) + sig(F, nt); b_ = sig(F, tk) + sig(T, nt)
            if a_ > b_: gt[ci].append(eT); gn[ci].append(eF)
            elif b_ > a_: gt[ci].append(eF); gn[ci].append(eT)
    ne = len(E); outs = [[] for _ in range(nb)]; inn = [[] for _ in range(nb)]
    for j, (u, v) in enumerate(E): outs[u].append(j); inn[v].append(j)
    # variables: edge flows, then an exit flow per block without a way on, then a source per orphan block of a table function
    nv = ne; xv = {}; sv = {}
    for k in range(nb):
        if not outs[k]: xv[k] = nv; nv += 1
    if table:
        for k in range(1, nb):
            if not inn[k] and not slow[k]: sv[k] = nv; nv += 1
    def cexpr(k): return [(j, 1.0) for j in outs[k]] + ([(xv[k], 1.0)] if k in xv else [])
    cost = [0.0] * nv; ub = [None] * nv
    for j in zero: ub[j] = 0.0
    for k in range(nb):
        if slow[k]:
            for j in inn[k]: ub[j] = 0.0
    eqr = []; eqc = []; eqv = []; beq = []
    for k in range(nb):
        r = len(beq)
        for j in inn[k]: eqr.append(r); eqc.append(j); eqv.append(1.0)
        for j, c_ in cexpr(k): eqr.append(r); eqc.append(j); eqv.append(-c_)
        if k in sv: eqr.append(r); eqc.append(sv[k]); eqv.append(1.0)
        beq.append(-(CALLS.get(fn, 0.0)) if k == 0 else 0.0)
    ubr = []; ubc = []; ubv = []; bub = []
    def absterm(expr, const, w):         # w * |expr - const|
        nonlocal nv
        t = nv; nv += 1; cost.append(w); ub.append(None)
        r = len(bub)
        for j, c_ in expr: ubr.append(r); ubc.append(j); ubv.append(c_)
        ubr.append(r); ubc.append(t); ubv.append(-1.0); bub.append(const)
        r = len(bub)
        for j, c_ in expr: ubr.append(r); ubc.append(j); ubv.append(-c_)
        ubr.append(r); ubc.append(t); ubv.append(-1.0); bub.append(-const)
    for k, (s, e) in enumerate(blocks):
        for j, c_ in cexpr(k): cost[j] += 1e-5 * (e - s)
        if slow[k]: continue
        for v, m_ in evs[k].items(): absterm(cexpr(k), v, float(m_))
    for ci, es in gt.items():
        g = GOTOS[ci][0]; absterm([(j, 1.0) for j in es], D[g] / steps, GW)
        if len(es) == 1: absterm([(j, 1.0) for j in gn[ci]], max(D[ci] - D[g], 0) / steps, GW)
    A_eq = coo_matrix((eqv, (eqr, eqc)), shape=(len(beq), nv)); A_ub = coo_matrix((ubv, (ubr, ubc)), shape=(len(bub), nv)) if bub else None
    r = linprog(cost, A_ub=A_ub, b_ub=bub if bub else None, A_eq=A_eq, b_eq=beq, bounds=[(0.0, u) for u in ub], method="highs")
    if r.status != 0 and strict: return one(fn, False)     # e.g. a base-register form with a constant I/O base: its helper call is the main path
    if r.status != 0: return {"slots": 0.0, "calls": CALLS.get(fn, 0.0), "bytes": size[sec], "bytes_s": 0, "n": n, "table": table, "misfit": -1.0, "lsum": 0.0, "fail": r.message}
    x = r.x; c = [sum(x[j] * c_ for j, c_ in cexpr(k)) for k in range(nb)]
    slots = misfit = lsum = 0.0; dump = []
    for k, (s, e) in enumerate(blocks):
        w = c[k]; bsl = w * (e - s); t = tm[k]
        if t is not None and t[1] == "c" and t[4] and t[0] + 1 < e and tk_edge.get(k) is not None: bsl -= w - x[tk_edge[k]]
        slots += bsl
        for v, m_ in evs[k].items(): misfit += m_ * abs(w - v) if not slow[k] else 0.0; lsum += m_ * v if not slow[k] else 0.0
        if verbose == fn: dump.append("%5x %4d instr  count %10.3f  lines %s %s" % (ins[s][0], e - s, w, " ".join("%.3fx%d" % (v, m_) for v, m_ in sorted(evs[k].items(), key=lambda z: -z[1])[:4]), "slow" if slow[k] else ""))
    if verbose == fn: print("\n".join(dump))
    return {"slots": slots, "calls": CALLS.get(fn, 0.0), "bytes": size[sec], "bytes_s": size.get(".text.unlikely." + fn + "_s", size.get(".text." + fn + "_s", 0)), "n": n, "table": table, "misfit": misfit, "lsum": lsum}
for fn in hot:
    if ".text." + fn not in size or (verbose and verbose != fn): continue
    r = one(fn)
    if r: res[fn] = r
if outf != "-": json.dump(res, open(outf, "w"))
t = sum(r["slots"] for r in res.values())
noev = [f for f, r in res.items() if r["lsum"] == 0 and r["calls"] > 0]
if noev: print("g45_size: NO LINE EVIDENCE in %d functions (their figure is a shortest path, not a measurement): %s" % (len(noev), " ".join(noev[:6])))
print("g45_size %s: %d functions, %.0f PSP slots a step; misfit %.0f of %.0f line-count slots (%.1f%%); %d not solved; %d bytes hot sections, %d bytes untouched copies" % (
    obj.split("/")[-1], len(res), t, sum(r["misfit"] for r in res.values()), sum(r["lsum"] for r in res.values()),
    100 * sum(r["misfit"] for r in res.values()) / max(sum(r["lsum"] for r in res.values()), 1), sum(1 for r in res.values() if "fail" in r),
    sum(r["bytes"] for r in res.values()), sum(r["bytes_s"] for r in res.values())))
