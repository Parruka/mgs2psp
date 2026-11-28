#!/usr/bin/env python3
"""gl45 study, batch 3: three generator levers as a post-pass over a generated header (boot/gen_lev.h holds the access forms).
usage: gen_levers.py <in.h> <out.h> <levers: any of a b c, e.g. abc> <selection.json | all> <report.json>
selection.json: {function: {"c": 0/1, "b1": 0/1, "b2": ["r16", ...], "a": 0/1}}; "all": every function, every base register.
The selection is a speed choice only.  Exactness is the pass's own: a function (or a register) whose text holds anything the pass
does not recognise is left as it is and named in the report.
 (c) clock in a local   GEN_KC_ON after the locals; kl_ written back before each return and around every call out (GEN_KCALL).
 (b) host pointers      b1: hs_ for $sp, set again after every assignment to r29.  b2: h<N>_ for a base register, set again after
                        every assignment to it; its accesses become the _H forms (the I/O test stays).
 (a) guarded bases      a base register that holds an entry value (never assigned, or one copy of an entry register made in the
                        first straight-line run) and is otherwise only restored on the way out: the function becomes a wrapper
                        that tests GEN_FAST of the entry value(s) and runs either the copy with _N / _HN accesses or the
                        untouched translation."""
import re, sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glib
FN = r"(?:S_\w+?_)?P[0-9a-f]{6}"
PURE = set("""vu_f vu_u vu_rnmul vu_cl vu_rnadd vu_rnsub vu_fin vu_in vout vin vu_min vu_max vu_ftoi vu_itof vu_setflags vu_vgrp
VU_FCL1 VU_FCL2 VU_FCL3 VU_FCL4 VU_MCL1 VU_MCL2 VU_MCL3 VU_MCL4 VQMTC2 VQMFC2 VUM0 VUM memcpy I2F F2I F2INT FADD FSUB FDIV FMUL
fabsf sqrtf fmaxf fminf RCLD RCMV if switch sizeof FN_ENTER IRQ_POLLF""".split())
MEM1 = "RD8B RD16B RD32B RD64B RDFB WR8B WR16B WR32B WR64B WRFB".split()
MEM2 = "GLDUB GSDUB VLQC2B VSQC2B".split()
STK = "RD8S RD16S RD32S RD64S RDFS WR8S WR16S WR32S WR64S WRFS".split()
BRK = "JALR JSIF SYSCALL BREAK COP0R COP0W COP0OP HWR32 HWW32".split()          # calls out the pass can bracket for (c)
NOC = "GLDL GLDR GLWL GLWR GSDL GSDR GSWL GSWR GLDU GSDU RD8 RD16 RD32 RD64 RDF WR8 WR16 WR32 WR64 WRF VLQC2 VSQC2".split()   # slow path inside an inline function: no (c)
def nocomment(s): return re.sub(r"/\*(?![0-9a-f]{6}\*/).*?\*/", " ", s, flags=re.S)
def stmt_end(s, p):
    """index just after the ';' that ends the statement holding position p"""
    d = 0; i = p
    while i < len(s):
        c = s[i]
        if c == '(': d += 1
        elif c == ')': d -= 1
        elif c == ';' and d <= 0: return i + 1
        i += 1
    return -1
def balanced(s, p):
    """s[p] == '(': index just after its closing ')'"""
    d = 0; i = p
    while i < len(s):
        if s[i] == '(': d += 1
        elif s[i] == ')':
            d -= 1
            if d == 0: return i + 1
        i += 1
    return -1
def assigns(lines, reg):
    """[(line, pos, kind, text)] of every assignment to the local `reg`; None when a form the pass cannot follow is present"""
    out = []
    for j, l in enumerate(lines):
        if re.search(r"(?<![\w\]\.])%s\s*(?:\+\+|--|(?:\+|-|\*|/|%%|&|\||\^|<<|>>)=)|(?:\+\+|--)\s*%s\b|&\s*%s\b(?!\s*[\w(])" % (reg, reg, reg), l): return None
        for m in re.finditer(r"(?<![\w\]\.])%s = (?!=)" % reg, l): out.append((j, m.start(), "plain", l[m.start():stmt_end(l, m.start())]))
        for m in re.finditer(r"\b(RCMV|RCLD)\(%s," % reg, l): out.append((j, m.start(), m.group(1).lower(), l[m.start():stmt_end(l, m.start())]))
    return out
def base_use(l, reg):
    return re.search(r"\b(?:%s)\(%s," % ("|".join(MEM1), reg), l) or re.search(r"\b(?:%s)\([^,()]+, %s," % ("|".join(MEM2), reg), l)
def bases(lines):
    s = set()
    for l in lines:
        s.update(m.group(1) for m in re.finditer(r"\b(?:%s)\((r\d+)," % "|".join(MEM1), l))
        s.update(m.group(1) for m in re.finditer(r"\b(?:%s)\([^,()]+, (r\d+)," % "|".join(MEM2), l))
    return s
def refresh(lines, reg, hp, sites):
    """put `hp = gen_hb(reg);` after every statement that assigns reg; False when an access through reg follows inside such a statement"""
    for j, p, kind, text in sorted(sites, key=lambda x: (x[0], -x[1])):
        l = lines[j]; e = stmt_end(l, p)
        if e < 0 or base_use(l[p:e], reg) or re.search(r"\b(?:%s)\(%s," % ("|".join(STK), reg), l[p + len(reg):e]): return False
        lines[j] = l[:e] + " %s = gen_hb((uint32_t)%s);" % (hp, reg) + l[e:]
    return True
def convert(name, body, levers, sel, rep):
    """body: the function's lines from its '{' line to the line before '}'.  Returns (new body, guards or None)"""
    text = nocomment("\n".join(body))
    toks = set(re.findall(r"\b([A-Za-z_][A-Za-z_0-9]*)\s*\(", text))
    known = PURE | set(MEM1) | set(MEM2) | set(STK) | set(BRK) | set(NOC)
    unk = sorted(t for t in toks if t not in known and not re.fullmatch(FN, t) and not re.fullmatch(r"M_[A-Z0-9]+", t))
    if unk or re.search(r"\breturn\s+[^;]", text) or "IRQ_POLLV" in text or "IRQ_POLL(" in text or "#" in text:
        rep["refused"] = "unknown: " + " ".join(unk[:6]) if unk else "return value / other poll form / preprocessor line"; return body, None
    if "gen_g0_fn" in body[0]: rep["refused"] = "a G0 body (its stores call the runtime's guard: left as it is)"; return body, None
    L = list(body)
    first = next((j for j in range(1, len(L)) if re.match(r"/\*[0-9a-f]{6}\*/|L\w+:;", L[j])), None)
    if first is None or any(not re.match(r"\s*$|FN_ENTER\(|  (?:int64_t|int32_t|float|uint32_t|uint64_t|VQ)\b", L[j]) for j in range(1, first)):
        rep["refused"] = "the opening lines are not FN_ENTER + locals"; return body, None
    decl = []
    # (a) the guarded bases, found on the untouched text
    guards = {}
    if "a" in levers and sel.get("a"):
        bs = bases(L)
        stop = next((j for j in range(first, len(L)) if re.search(r"^L\w+:;|\bgoto\b|\breturn\b|\b(?:%s)\(\)|\b(?:%s)\(" % (FN, "|".join(BRK)), L[j])), len(L))
        for reg in sorted(bs):
            n = int(reg[1:]); si = assigns(L, reg)
            if si is None or not any(j < first for j, _, _, _ in si): continue
            src = n; ok = True; copy = None
            for j, p, kind, t in si:
                if j < first: continue
                if kind == "rcld" and re.fullmatch(r"RCLD\(%s, %d, \(int64_t\)RD64S\(r29, \d+\)\);" % (reg, n), t):
                    r = next((q for q in range(j, len(L)) if "return;" in L[q]), len(L) - 1)
                    tail = L[j][p + len(t):] + "\n" + "\n".join(L[j + 1:r + 1])
                    if "goto" in "\n".join(L[j:r + 1]) or base_use(tail, reg): ok = False
                    continue
                m = re.fullmatch(r"RCMV\(%s, %d, (\d+)\);" % (reg, n), t) or re.fullmatch(r"%s = r(\d+) \+ r0;" % reg, t)
                if m and copy is None and first <= j < stop:
                    s = int(m.group(1)); sr = "r%d" % s; ss = assigns(L, sr)
                    pre = "\n".join(L[first:j]) + "\n" + L[j][:p]
                    if ss is None or any(first <= q < j or (q == j and pp < p) for q, pp, _, _ in ss) or re.search(r"\bR\[%d\] =|\bRC(?:MV|LD)\(\w+, %d," % (s, s), pre) or base_use(pre, reg) or s == 0:
                        ok = False
                    copy = j; src = s; continue
                ok = False
            if ok: guards[reg] = src
        if not guards: rep["a"] = "no guarded base"
    # (b2) host pointers of base registers
    hreg = {}
    if "b" in levers:
        want = sel.get("b2", [])
        if want == "all": want = sorted(bases(L))
        for reg in want:
            si = assigns(L, reg)
            if si is None or not base_use("\n".join(L), reg): rep.setdefault("b2_refused", []).append(reg); continue
            T = list(L)
            if not refresh(T, reg, "h%s_" % reg[1:], si): rep.setdefault("b2_refused", []).append(reg); continue
            L = T; hreg[reg] = "h%s_" % reg[1:]; decl.append("uint8_t *h%s_;" % reg[1:])
    # the accesses' forms
    for reg in sorted(set(hreg) | set(guards)):
        suf = ("_H" if reg in hreg else "") + ("N" if reg in guards and reg in hreg else "_N" if reg in guards else "")
        hp = hreg.get(reg)
        for j in range(first, len(L)):
            if hp:
                L[j] = re.sub(r"\b(%s)\(%s," % ("|".join(MEM1), reg), lambda m: "%s%s(%s, %s," % (m.group(1), suf, hp, reg), L[j])
                L[j] = re.sub(r"\b(%s)\(([^,()]+), %s," % ("|".join(MEM2), reg), lambda m: "%s%s(%s, %s, %s," % (m.group(1), suf, hp, m.group(2), reg), L[j])
            else:
                L[j] = re.sub(r"\b(%s)\(%s," % ("|".join(MEM1), reg), lambda m: "%s%s(%s," % (m.group(1), suf, reg), L[j])
                L[j] = re.sub(r"\b(%s)\(([^,()]+), %s," % ("|".join(MEM2), reg), lambda m: "%s%s(%s, %s," % (m.group(1), suf, m.group(2), reg), L[j])
    rep["b2"] = sorted(hreg); rep["a_guards"] = {k: v for k, v in guards.items()}
    # (b1) the stack's host pointer
    if "b" in levers and sel.get("b1"):
        t = "\n".join(L)
        sargs = set(re.findall(r"\b(?:%s)\((\w+)," % "|".join(STK), t)); si = assigns(L, "r29")
        if not sargs: rep["b1"] = "no stack access"
        elif sargs != {"r29"} or si is None or "MMS(" in t: rep["b1"] = "refused"
        else:
            T = list(L)
            if refresh(T, "r29", "hs_", si): L = T; decl.append("GEN_HS_ON;"); rep["b1"] = 1
            else: rep["b1"] = "refused"
    # (c) the clock
    if "c" in levers and sel.get("c"):
        t = "\n".join(L)
        if "IRQ_POLLF" not in text: rep["c"] = "no poll"
        elif toks & set(NOC): rep["c"] = "refused: " + " ".join(sorted(toks & set(NOC))[:4])
        else:
            for j in range(first, len(L)):
                l = L[j]; o = []; p = 0
                for m in re.finditer(r"\b(%s)\(\)|\b(%s)\(" % (FN, "|".join(BRK)), l):
                    if m.start() < p: continue
                    e = m.end() if m.group(1) else balanced(l, m.end() - 1)
                    o.append(l[p:m.start()] + ("GEN_KCALLV(" if m.group(2) in ("COP0R", "HWR32") else "GEN_KCALL(") + l[m.start():e] + ")"); p = e
                l = "".join(o) + l[p:]
                L[j] = l.replace("return;", "{ GEN_KPUT; return; }")
            L.insert(first, "  GEN_KC_ON;"); L.append("  GEN_KPUT;"); rep["c"] = 1
    if decl: L.insert(1, "  " + " ".join(decl))
    return L, (guards if guards else None)
def main():
    src, dst, levers, self_, repf = sys.argv[1:6]
    L, fns = glib.load(src)
    ALL = {"c": 1, "b1": 1, "b2": "all", "a": 1}
    if self_.startswith("all"):                      # all, or all-b1 / all-b2 / ...: everything but the named parts
        for k in self_.split("-")[1:]: ALL.pop(k)
        sel = None
    else: sel = json.load(open(self_))
    out = []; i = 0; report = {}; order = sorted(fns.items(), key=lambda x: x[1][0]); n = 0
    for name, (a, b) in order:
        out.extend(L[i:a]); i = b + 1
        s = ALL if sel is None else sel.get(name)
        if not s: out.extend(L[a:b + 1]); continue
        rep = {}; body, guards = convert(name, L[a + 1:b], levers, s, rep); report[name] = rep
        if "refused" in rep or body == L[a + 1:b]: out.extend(L[a:b + 1]); continue
        n += 1
        if guards:
            out.append("static __attribute__((noinline, cold)) void %s_s(void)   /* tools/gen_levers.py: the translation as it was */" % name); out.extend(L[a + 1:b + 1])
            out.append("static __attribute__((always_inline)) inline void %s_f(void)   /* tools/gen_levers.py: behind the entry guard */" % name); out.extend(body); out.append("}")
            cond = " && ".join("GEN_FAST((uint32_t)R[%d])" % s_ for s_ in sorted(set(guards.values())))
            out.append("static void %s(void) { if (GEN_GUARD(%s)) %s_f(); else %s_s(); }   /* tools/gen_levers.py */" % (name, cond, name, name))
        else:
            out.append(L[a]); out.extend(body); out.append("}")
    out.extend(L[i:])
    open(dst, "w").write("\n".join(out)); json.dump(report, open(repf, "w"), indent=0)
    ref = sum(1 for r in report.values() if "refused" in r)
    print("gen_levers %s: %s: %d functions looked at, %d changed, %d refused whole; c on %d, b1 on %d, b2 registers %d, guarded functions %d" % (
        levers, dst, len(report), n, ref, sum(1 for r in report.values() if r.get("c") == 1), sum(1 for r in report.values() if r.get("b1") == 1),
        sum(len(r.get("b2", [])) for r in report.values()), sum(1 for r in report.values() if r.get("a_guards"))))
main()
