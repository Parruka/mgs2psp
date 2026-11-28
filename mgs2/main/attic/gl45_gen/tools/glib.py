"""gl45 batch 3: shared helpers: the generated headers' function bodies, the hot list."""
import re
def load(path):
    L = open(path).read().split("\n")
    fns = {}   # name -> (first line index of 'static void NAME(void)', index of closing '}')
    i = 0; n = len(L)
    while i < n:
        m = re.fullmatch(r"static void ((?:S_\w+?_)?P[0-9a-f]{6})\(void\)", L[i])
        if m and i + 1 < n and L[i + 1].startswith("{"):
            e = i + 1
            while L[e] != "}": e += 1
            fns[m.group(1)] = (i, e); i = e
        i += 1
    return L, fns
def hot(path, n):
    """the n hottest translated functions of the rain wait by the twin's callgrind self cost: [(name, x86 instructions in the window)]"""
    out = []
    for l in open(path):
        m = re.match(r"\s*([\d,]+) \(\s*[\d.]+%\)\s+\S+?:((?:S_\w+?_)?P[0-9a-f]{6})(?:_tr)? ", l)
        if m: out.append((m.group(2), int(m.group(1).replace(",", ""))))
    return out[:n]
TOK = re.compile(r"/\*([0-9a-f]{6})\*/|goto (L[0-9a-zA-Z_]+);")
def load2(path):
    """as load, plus tools/gen_levers.py's copies NAME_s / NAME_f and wrappers: (lines, {name: (first, last)}, {wrapper name: line})"""
    L = open(path).read().split("\n"); fns = {}; wr = {}
    i = 0; n = len(L)
    while i < n:
        m = re.match(r"static (?:__attribute__\(\(.*?\)\) )?(?:inline )?void ((?:S_\w+?_)?P[0-9a-f]{6}(?:_s|_f)?)\(void\)(.*)$", L[i])
        if m:
            if m.group(2).lstrip().startswith("{ if ("): wr[m.group(1)] = i
            elif i + 1 < n and L[i + 1].startswith("{") and not m.group(2).strip().startswith(";"):
                e = i + 1
                while L[e] != "}": e += 1
                fns[m.group(1)] = (i, e); i = e
        i += 1
    return L, fns, wr
def pieces(l):
    """one body line cut into [(text, kind, token number in the line)]: a guest instruction comment starts a piece; a taken block
    `if (...) { ...; goto L; }` (or a bare goto) is a piece of its own, owned by its goto."""
    toks = list(TOK.finditer(l)); cuts = {0, len(l)}; own = []   # own: (start, end, kind, n)
    for n, m in enumerate(toks):
        if m.group(1): cuts.add(m.start()); own.append((m.start(), None, "at", n)); continue
        gs, ge = m.start(), m.end(); d = 0; bi = -1
        for p in range(gs - 1, -1, -1):
            if l[p] == '}': d += 1
            elif l[p] == '{':
                if d == 0: bi = p; break
                d -= 1
        ce = ge
        while ce < len(l) and l[ce] == ' ': ce += 1
        if bi >= 0 and l[:bi].rstrip().endswith(')') and ce < len(l) and l[ce] == '}':
            cuts.add(bi + 1); cuts.add(ce); own.append((bi + 1, ce, "goto", n))
        else:
            cuts.add(gs); cuts.add(ge); own.append((gs, ge, "goto", n))
    cs = sorted(cuts); out = []
    for a, b in zip(cs, cs[1:]):
        k = None
        for s, e, kind, n in own:
            if kind == "goto" and s <= a and b <= e: k = ("goto", n)
        if k is None:
            for s, e, kind, n in own:
                if kind == "at" and s == a: k = ("at", n)
        out.append((l[a:b], k))
    return out
