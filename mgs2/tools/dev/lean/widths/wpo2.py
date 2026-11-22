#!/usr/bin/env python3
"""wpo.py (dev, recomp/lean_plan.md step 2): per-register widths for every translated function, proved on regcache.py's own output.

A function's locals are int64_t (wide) or int32_t (narrow); a narrow local holds the low word of the guest register, sign-extended.
Whether that is the register's exact value depends on the guest value: S (the sign extension of its low word) or W (any other).
Phase 1 computes, for every point of every function, which registers may hold a W value (a forward may-analysis of the guest
program over regcache's output, with the interprocedural sets E[f]: the registers that may be W at f's entry; X[f]: may be W in R[]
after f returns; E_IND / X_IND for the calls through a register).  The guest values do not depend on the widths, so phase 1 runs on
the all-narrow output.  Phase 2, per function: regcache's output with the current wide set is checked; a narrow local that may hold
a W value where its full value is used (a compare or 64-bit shift right: F; sd / sq of the local: Q; a store R[n] = rn: S; a source
of a 64-bit move / logical / add / shift left into a wide local: D) joins the wide set; until nothing is found.  Each found site is
recorded (why).  The output then holds no use of a truncated W value: it computes what the all-64-bit translation computes.

Assumptions (documented in lean_plan.md when this lands): the runtime's kernel calls return S values in v0 (SYSCALL); a callee does
not write into its caller's stack frame except through an address the caller has taken; the stack pointer is a guest RAM address;
with A1 (option, default on: WPO_A1=0 makes them W) a daddiu / dnegu of an S value does not leave the sign-extended range.
usage: wpo.py <out.json> [unit ...]"""
import os, sys, re, json, time, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rcdrv as D
rc = D.rc
A1 = os.environ.get('WPO_A1', '1') == '1'
NOS = int(os.environ.get('WPO_NOS', '0'))     # dev: 1 = ignore stores R[n] = rn (an optimistic bound, not sound)
NOIND = int(os.environ.get('WPO_NOIND', '0'))  # dev: 1 = no W at the entry of functions a register call reaches (a bound, not sound)
B = lambda n: 1 << n
ALLM = ((1 << 32) - 1) & ~1
def mask(s):
    m = 0
    for n in s: m |= 1 << n
    return m & ALLM
def regs(m): return [n for n in range(1, 32) if m >> n & 1]
NOREL_M = mask(rc.NOREL)
CMT = re.compile(r'/\*.*?\*/')
LOC = re.compile(r'\br(\d+)\b')
ASG = re.compile(r'\br(\d+) = ')
GST = re.compile(r'\bR\[(\d+)\] = ')
RCLD = re.compile(r'\bRCLD\(r(\d+), (\d+), ')
RCMV = re.compile(r'\bRCMV\(r(\d+), (\d+), (\d+)\)')
GOTO = re.compile(r'\bgoto (L[0-9a-f]+(?:_i\d+)?|Lend_i\d+);')
ILAB = re.compile(r'\b(L[0-9a-f]+_i\d+|Lend_i\d+):;')
FLAB = re.compile(r'^(L[0-9a-f]+):;')
RET = re.compile(r'\breturn;')
S32 = '(int64_t)(int32_t)'
INL = re.compile(r'\{ /\* inlined (\w+) \*/')
HLW_RE = re.compile(r'\b(HI|LO|HI1|LO1) = ')
HLBIT = {'HI': 1, 'LO': 2, 'HI1': 4, 'LO1': 8}
UNKNOWN = collections.Counter()
SIF = rc.SIF_TARGETS

def match_close(t, i):
    """index after the bracket closing the one at t[i]"""
    o = t[i]; c = {'(': ')', '{': '}', '[': ']'}[o]; d = 0
    while True:
        ch = t[i]
        if ch == o: d += 1
        elif ch == c:
            d -= 1
            if d == 0: return i + 1
        i += 1

def rhs_end(t, i):
    """end of the expression starting at t[i]: a ';' or ',' at depth 0, or the ')' closing an enclosing bracket"""
    d = 0
    while i < len(t):
        ch = t[i]
        if ch in '([{': d += 1
        elif ch in ')]}':
            if d == 0: return i
            d -= 1
        elif ch in ';,' and d == 0: return i
        i += 1
    return i

def args_of(t, i):
    """the top-level arguments (start, end) of the call whose '(' is at t[i]"""
    e = match_close(t, i); out = []; d = 0; s = i + 1
    for j in range(i + 1, e - 1):
        ch = t[j]
        if ch in '([{': d += 1
        elif ch in ')]}': d -= 1
        elif ch == ',' and d == 0: out.append((s, j)); s = j + 2 if t[j + 1:j + 2] == ' ' else j + 1
    out.append((s, e - 1))
    return out, e

def cond_regions(t):
    """the spans of (comment-blanked) line t that run on some paths only: an if's body (unless its condition is always true: a nonzero
    constant, r0 == r0, or c_ set from one of those in the same block) and its else part, a switch block and each of its cases"""
    out = []
    for m in re.finditer(r'\b(if|switch) \(', t):
        j = match_close(t, m.end() - 1); k = j
        while k < len(t) and t[k] == ' ': k += 1
        if k >= len(t): continue
        e = match_close(t, k) if t[k] == '{' else t.index(';', k) + 1
        cond = t[m.end():j - 1].strip()
        always = False
        if m.group(1) == 'if':
            if re.fullmatch(r'\d+', cond) and int(cond): always = True
            elif cond == 'r0 == r0': always = True
            elif cond == 'c_':
                q = t.rfind('int c_ = (', 0, m.start())
                if q >= 0:
                    ce = match_close(t, q + len('int c_ = '))
                    cv = t[q + len('int c_ = ') + 1:ce - 1].strip()
                    if cv == 'r0 == r0' or (re.fullmatch(r'\d+', cv) and int(cv)): always = True
        if not always: out.append((k, e))
        if m.group(1) == 'switch' and t[k] == '{':
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
            out.append((q, match_close(t, q) if t[q] == '{' else t.index(';', q) + 1))
    return out

JMP_END = re.compile(r'(?:\bgoto (?:L[0-9a-f]+(?:_i\d+)?|Lend_i\d+)|\breturn);$')
def jump_regions(t, regsr):
    """the if bodies of (comment-blanked) line t that end with a jump of their own (goto, return), from cond_regions' spans regsr: such a
    body runs exactly on the path to its jump (a branch-likely delay slot: { if (c) { r30 = ...; goto L; } }), so its events form a
    block of their own, entered when the condition holds, and what follows the body runs only when it does not (outermost ones)"""
    out = []; rs = set(regsr)
    for m in re.finditer(r'\bif \(', t):
        j = match_close(t, m.end() - 1); k = j
        while k < len(t) and t[k] == ' ': k += 1
        if k >= len(t) or t[k] != '{': continue
        e = match_close(t, k)
        if (k, e) not in rs: continue
        body = t[k + 1:e - 1].rstrip(); mm = JMP_END.search(body)
        if not mm: continue
        q = k + 1 + mm.start()
        if any(a2 <= q < b2 for a2, b2 in regsr if k < a2 and b2 <= e): continue      # the jump itself conditional within the body
        if ILAB.search(t, k, e): continue
        out.append((k, e))
    return [r for r in out if not any(o != r and o[0] <= r[0] and r[1] <= o[1] for o in out)]

# ---- the value class of an assignment's right side, and the use kind of each local in it -------------------------------------
# classes: ('S',) ('W',) ('LD',) ('LDS', k) ('MV', m) (move: the guest value of m) ('P', mask) (S if every source is S) ('HL', bit)
#          ('REL',) (a reload after a call: the call set the value) ('SP', c) (sp += c)
# use kinds: 'L' low word only, 'F' full value, 'D' full value when the destination is wide
NUM = r'-?\d+'
def _int_ok(v): return -(1 << 31) <= v < (1 << 31)
SIMPLE = [
    (re.compile(r'r(\d+)'), 'mv'),
    (re.compile(r'r(\d+) [+|] r0|r0 [+|] r(\d+)|r(\d+) - r0'), 'mv'),
    (re.compile(r'r0 - r(\d+)'), 'neg'),
    (re.compile(r'r(\d+) & (\d+)'), 'andi'),
    (re.compile(r'r(\d+) [|^] (\d+)'), 'ori'),
    (re.compile(r'r(\d+) \+ (-?\d+)'), 'daddiu'),
    (re.compile(r'r(\d+) ([&|^]) r(\d+)'), 'log'),
    (re.compile(r'~\(r(\d+) \| r(\d+)\)'), 'nor'),
    (re.compile(r'r(\d+) >> (\d+)'), 'dsra'),
    (re.compile(r'r(\d+) >> \(r(\d+) & 63\)'), 'dsrav'),
    (re.compile(r'\(int64_t\)\(\(uint64_t\)r(\d+) << (\d+)\)'), 'dsll'),
    (re.compile(r'\(int64_t\)\(\(uint64_t\)r(\d+) >> (\d+)\)'), 'dsrl'),
    (re.compile(r'\(int64_t\)\(\(uint64_t\)r(\d+) << \(r(\d+) & 63\)\)'), 'dsllv'),
    (re.compile(r'\(int64_t\)\(\(uint64_t\)r(\d+) >> \(r(\d+) & 63\)\)'), 'dsrlv'),
    (re.compile(r'r(\d+) ([+-]) r(\d+)'), 'daddu'),
    (re.compile(r'\(r(\d+) < (?:r(\d+)|-?\d+)\)'), 'slt'),
    (re.compile(r'\(\(uint64_t\)r(\d+) < \(uint64_t\)(?:r(\d+)|\(int64_t\)-?\d+)\)'), 'slt'),
    (re.compile(r'HI|LO|HI1|LO1'), 'hilo'),
    (re.compile(r'\(int64_t\)SAR'), 's0'),
    (re.compile(r'-?\d+|0x[0-9a-f]+'), 'const'),
    (re.compile(r'R\[(\d+)\]|\(int32_t\)R\[(\d+)\]'), 'rel'),
    (re.compile(r'\(int32_t\)\(\(uint32_t\)r(\d+) << \d+\)|\(int32_t\)r(\d+) >> \d+|\(int32_t\)\(\(uint32_t\)r(\d+) >> \d+\)'), 'n32'),
    (re.compile(r'lo_'), 'lo_'),
    (re.compile(r'RH\[\d+\]'), 'w0'),
]
ADDR = re.compile(r'^\(uint32_t\)\(r(\d+) \+ (-?\d+)\)$')
ADDRQ = re.compile(r'^\(\(uint32_t\)\(r(\d+) \+ (-?\d+)\)\) & ~15u$')
S32_OK = re.compile(r'^\((?:r\d+ [+-] (?:r\d+|-?\d+)|\(uint32_t\)r\d+ (?:<<|>>) (?:\d+|\(r\d+ & 31\))|\(int32_t\)r\d+ >> (?:\d+|\(r\d+ & 31\))|F2I\(F\[\d+\]\))\)$|^(?:COP0R|VCFC2)\([^r]*\)$')

SPRD = re.compile(r'\b(RD8|RD16|RD32|RD64|RDF)\(\(uint32_t\)\(r29 \+ (-?\d+)\)\)')
SPSZ = {'RD8': 1, 'RD16': 2, 'RD32': 4, 'RD64': 8, 'RDF': 4}
SPVL = re.compile(r'\bVLQC2\(\d+, \(\(uint32_t\)\(r29 \+ (-?\d+)\)\) & ~15u\)')
SPGL = re.compile(r'\b(GLDL|GLDR|GLWL|GLWR)\(r\d+, \(uint32_t\)\(r29 \+ (-?\d+)\)\)')
SPQA = re.compile(r'\bRD64\(qa_( \+ 8)?\)')

def is_addr(t, p):
    """the local at t[p] is the base of a guest address (uint32_t)(rN + k)"""
    return t[max(0, p - 11):p] == '(uint32_t)(' and re.match(r'r\d+ \+ -?\d+\)', t[p:]) is not None

def gr(m):
    return [int(x) for x in m.groups() if x is not None and re.fullmatch(r'\d+', x)]

def classify(e, off, qa):
    """(class, [(pos, n, kind)]) of the right side e (comment-blanked) starting at off; qa: the line's qa_ base (n, k) before it"""
    e0 = e; e = e.rstrip(); uses = []
    locs = [(off + m.start(), int(m.group(1))) for m in LOC.finditer(e)]
    def allk(k): return [(p, n, k) for p, n in locs if n]
    for rx, kind in SIMPLE:
        m = rx.fullmatch(e)
        if not m: continue
        g = gr(m)
        if kind == 'mv':
            s = g[0]; return ('MV', s), allk('D')
        if kind == 'neg': return (('P', mask([g[0]])) if A1 else ('W',)), allk('D')
        if kind == 'andi': return ('S',), allk('L')
        if kind == 'ori': return ('P', mask([g[0]])), allk('D')
        if kind == 'daddiu':
            if g[0] == 29: return ('S',), allk('D')
            return (('P', mask([g[0]])) if A1 else ('W',)), allk('D')
        if kind == 'log':
            s, op, t = int(m.group(1)), m.group(2), int(m.group(3))
            if op == '&' and 0 in (s, t): return ('S',), allk('D')
            return ('P', mask([s, t])), allk('D')
        if kind == 'nor': return ('P', mask(g)), allk('D')
        if kind == 'dsra':
            k = g[1]
            if k == 0: return ('MV', g[0]), allk('D')
            return (('S',) if k >= 32 else ('P', mask([g[0]]))), allk('F')
        if kind == 'dsrav':
            return ('P', mask([g[0]])), [(p, n, 'F' if i == 0 else 'L') for i, (p, n) in enumerate(locs) if n]
        if kind == 'dsll':
            k = g[1]
            if k == 0: return ('MV', g[0]), allk('D')
            return ('W',), allk('L' if k >= 32 else 'D')
        if kind == 'dsrl':
            k = g[1]
            if k == 0: return ('MV', g[0]), allk('D')
            return ('W',), allk('F')
        if kind == 'dsllv': return ('W',), [(p, n, 'D' if i == 0 else 'L') for i, (p, n) in enumerate(locs) if n]
        if kind == 'dsrlv': return ('W',), [(p, n, 'F' if i == 0 else 'L') for i, (p, n) in enumerate(locs) if n]
        if kind == 'daddu':
            s, t = int(m.group(1)), int(m.group(3))
            if s == 0 or t == 0: break     # r0 forms: matched above (mv / neg)
            return ('W',), allk('D')
        if kind == 'slt': return ('S',), allk('F')
        if kind == 'hilo': return ('HL', HLBIT[e]), []
        if kind == 's0': return ('S',), []
        if kind == 'const':
            v = int(e, 0); return (('S',) if _int_ok(v) else ('W',)), []
        if kind == 'rel': return ('REL',), []
        if kind == 'n32': return ('S',), allk('L')
        if kind == 'lo_': return ('LO_',), []
        if kind == 'w0': return ('W',), []
    if e.startswith(S32):
        inner = e[len(S32):]
        if S32_OK.match(inner):
            if e.startswith(S32 + '(r29 + ') and re.fullmatch(r'\(r29 \+ (-?\d+)\)', inner): return ('SP?', int(inner[7:-1])), allk('L')
            return ('S',), allk('L')
    m = re.fullmatch(r'\(int64_t\)RD64\((.*)\)', e)
    if m:
        a = m.group(1); ad = ADDR.match(a)
        if ad: return (('LDS', int(ad.group(2))) if ad.group(1) == '29' else ('LD',)), allk('L')
        if a == 'qa_' and qa: return (('LDS', qa[1]) if qa[0] == 29 else ('LD',)), []
        if a == 'qa_': return ('LD',), []
    m = re.fullmatch(r'(\(int8_t\)RD8|\(int16_t\)RD16|\(int32_t\)RD32|RD8|RD16|RD32)\((.*)\)', e)
    if m and ADDR.match(m.group(2)):
        return (('W',) if m.group(1) == 'RD32' else ('S',)), allk('L')
    m = re.fullmatch(r'(\(int32_t\))?HWR32\(0x[0-9a-f]+\)', e)
    if m: return (('S',) if m.group(1) else ('W',)), []
    m = re.fullmatch(r'(GLWL|GLWR|GLDL|GLDR)\(r(\d+), (.*)\)', e)
    if m and ADDR.match(m.group(3)):
        t = int(m.group(2)); op = m.group(1)
        k0 = 'L' if op == 'GLWL' else 'D'
        us = [(p, n, k0 if i == 0 else 'L') for i, (p, n) in enumerate(locs) if n]
        if op == 'GLWL': return ('S',), us
        if op == 'GLWR': return ('P', mask([t])), us
        return ('W',), us
    UNKNOWN['rhs ' + re.sub(r'\d+', 'N', e)[:80]] += 1
    return ('W',), allk('F')

GLD_ST = re.compile(r'\s*r(\d+) = (GLDL|GLDR)\(r(\d+), \(uint32_t\)\(r(\d+) \+ (-?\d+)\)\);')
def gld_pair(n, op, base, off, rest):
    """rest: the (comment-blanked) text after an unaligned 64-bit load rn = op(rn, base + off); true when it starts with the other half of
    the pair (same destination and base, ldl at k + 7 and ldr at k, either order): the two load all eight bytes whatever the alignment,
    so the old value the first one merges into is dead"""
    m = GLD_ST.match(rest)
    if not m or int(m.group(1)) != n or int(m.group(3)) != n or int(m.group(4)) != base or base == n or m.group(2) == op: return False
    o2 = int(m.group(5))
    return off == o2 + 7 if op == 'GLDL' else o2 == off + 7

class Fn:
    __slots__ = ('name', 'unit', 'open', 'ind', 'blocks', 'nb', 'lab', 'taken', 'calls', 'idiom', 'hl_read', 'jalr', 'wide', 'n_line', 'rin', 'savek')

def parse(name, U, out, wide):
    """the blocks of function name's output out: [(events, falls)], events [(kind, data, cond)] in order"""
    F = Fn(); F.name = name; F.unit = U.name; F.open = name in U.open; F.ind = name in INDN; F.wide = wide
    ts = rc.ipo_sets(name); F.rin = ALLM if ts is None else mask(ts[0])
    lines = out.split('\n'); blocks = []; lab = {}; xb = {}; taken = False; calls = set(); F.idiom = False; F.hl_read = False; F.jalr = False
    for li, s in enumerate(lines):
        if li == 1 and (s.startswith('  int64_t ') or s.startswith('  int32_t ')): continue      # the declarations: entry loads
        t = CMT.sub(lambda m: ' ' * len(m.group(0)), s)
        fm = FLAB.match(t)
        ev = []
        regsr = cond_regions(t) if ('if (' in t or 'switch (' in t) else []
        cond = lambda p: any(a <= p < b for a, b in regsr)
        defs_at = set()
        # inline-internal labels split the line into blocks
        splits = [(m.start(), m.group(1)) for m in ILAB.finditer(t)]
        qa_list = [(m.start(), (int(m.group(1)), int(m.group(2)) & ~15)) for m in re.finditer(r'\bqa_ = \(\(uint32_t\)\(r(\d+) \+ (-?\d+)\)\) & ~15u;', t)]
        def qa_at(p):
            q = None
            for a, v in qa_list:
                if a < p: q = v
            return q
        # VQMFC2 / MMI glue loads (the helper wrote R[n]): W
        glue = set()
        for m in re.finditer(r'\bVQMFC2\((\d+), \d+\); r(\d+) = R\[(\d+)\];', t):
            glue.add(m.start(2) - 1); ev.append((m.start(), 1, 'rset', int(m.group(1)), cond(m.start())))
        for m in re.finditer(r'\bVQMTC2\((\d+), ', t):
            if int(m.group(1)): ev.append((m.start(), 1, 'ru', (int(m.group(1)), 'F'), cond(m.start())))
        for m in rc.MMI_RE.finditer(t):
            d, srcs = rc.mmi_args(m)[0], rc.mmi_args(m)[1]
            for x in srcs:
                if x: ev.append((m.start(), 1, 'ru', (x, 'F'), cond(m.start())))
            if d:
                ev.append((m.end(), 0, 'rset', d, cond(m.start())))
                g = re.match(r' r(\d+) = R\[(\d+)\];', t[m.end():])
                if g: glue.add(m.end() + 1)
            if m.group(1) in ('PMULTH', 'PDIVBW'): ev.append((m.end(), 1, 'hw', (15, ('W',)), cond(m.start())))
        # assignments to locals
        skip_use = set(); lds = []
        for m in ASG.finditer(t):
            n = int(m.group(1)); p0 = m.start()
            if t[p0 - 1:p0] == '(' and t[max(0, p0 - 5):p0] in ('RCLD(', 'RCMV('): continue
            skip_use.add(p0)
            e0 = m.end(); e1 = rhs_end(t, e0); e = t[e0:e1]
            if p0 in glue: cls, us = ('W',), []
            else: cls, us = classify(e, e0, qa_at(p0))
            if cls[0] == 'LO_':
                blk = t.rfind('{ int64_t lo_ = ', 0, p0)
                lm = re.match(r'\{ int64_t lo_ = ([^,;]*), hi_ = ([^;]*);', t[blk:])
                c2, u2 = classify(lm.group(1), blk + len('{ int64_t lo_ = '), None)
                cls = c2 if c2[0] in ('S', 'W', 'P', 'MV') else ('W',)
                us = u2
            if cls[0] == 'SP?': cls = ('SP', cls[1]) if n == 29 else ('S',)
            if n == 0: continue
            gm = re.fullmatch(r'(GLDL|GLDR)\(r(\d+), \(uint32_t\)\(r(\d+) \+ (-?\d+)\)\)', e.strip())
            if gm and int(gm.group(2)) == n:
                rest = t[e1 + 1:]
                if not rest.strip() and li + 1 < len(lines): rest = CMT.sub(lambda q: ' ' * len(q.group(0)), lines[li + 1])
                if gld_pair(n, gm.group(1), int(gm.group(3)), int(gm.group(4)), rest):      # the merge input: overwritten by the pair
                    for x in us:
                        if x[1] == n and x[2] == 'D': skip_use.add(x[0])
                    us = [x for x in us if not (x[1] == n and x[2] == 'D')]
            for p, k, kind in us:
                skip_use.add(p)
                if kind == 'L' or kind == 'F': ev.append((p, 0, 'u', (k, kind), cond(p)))
            deps = mask([k for p, k, kind in us if kind == 'D'])
            for p, k, kind in us:
                if k == 29 and not (cls[0] == 'SP' and n == 29) and not is_addr(t, p): taken = True
            ev.append((e1, 1, 'd', (n, cls, deps, 'glue' if p0 in glue else None), cond(p0)))
            if cls[0] == 'HL': F.hl_read = True
            if cls[0] == 'LDS': lds.append((e0, e1))
        for m in RCLD.finditer(t):
            n = int(m.group(1)); skip_use.add(m.start(1) - 1)
            a, e = args_of(t, m.end() - len(m.group(0)) + 4)
            ex = t[a[2][0]:a[2][1]]
            cls, us = classify(ex, a[2][0], qa_at(m.start()))
            if cls[0] not in ('LD', 'LDS'): UNKNOWN['rcld ' + ex[:60]] += 1; cls = ('W',)
            for p, k, kind in us: skip_use.add(p); ev.append((p, 0, 'u', (k, kind), cond(p)))
            ev.append((e, 1, 'd', (n, cls, 0, 'rcld'), cond(m.start())))
            if cls[0] == 'LDS': lds.append((a[2][0], a[2][1]))
        for m in RCMV.finditer(t):
            n, src = int(m.group(1)), int(m.group(3)); skip_use.add(m.start(1) - 1)
            ev.append((m.end(), 1, 'd', (n, ('MV', src), 0, 'rcmv'), cond(m.start())))
        # global stores (before a call, at a return, the tail, before a qmtc2 / MMI helper)
        for m in GST.finditer(t):
            n = int(m.group(1)); v = re.match(r'r(\d+)\b', t[m.end():])
            if not v or int(v.group(1)) != n: UNKNOWN['gstore ' + t[m.start():m.start() + 40]] += 1; continue
            skip_use.add(m.end())
            ev.append((m.start(), 0, 's', n, cond(m.start())))
        # HI / LO writes
        for m in HLW_RE.finditer(t):
            if t[m.start() - 1:m.start()] in ('.', '>'): continue
            e0 = m.end(); e1 = rhs_end(t, e0); e = t[e0:e1].strip()
            v = re.fullmatch(r'r(\d+)', e)
            if v:
                n = int(v.group(1)); skip_use.add(e0)
                if n: ev.append((e0, 0, 'u', (n, 'F'), cond(e0)))
                ev.append((e1, 1, 'hw', (HLBIT[m.group(1)], ('R', n)), cond(m.start())))
            else: ev.append((e1, 1, 'hw', (HLBIT[m.group(1)], ('S',)), cond(m.start())))
        # stores to memory: WR64 values (Q), stack slots
        for m in re.finditer(r'\b(WR64|WR32|WR16|WR8|WRF|GSWL|GSWR|GSDL|GSDR|VSQC2)\(', t):
            op = m.group(1); a, e = args_of(t, m.end() - 1)
            if op in ('GSWL', 'GSWR', 'GSDL', 'GSDR'): ap, vp = a[1], a[0]
            elif op == 'VSQC2': ap, vp = a[1], None
            else: ap, vp = a[0], a[1]
            at = t[ap[0]:ap[1]]; vt = t[vp[0]:vp[1]] if vp else ''
            sk = None
            mm = ADDR.match(at)
            if mm and mm.group(1) == '29': sk = int(mm.group(2))
            mm = ADDRQ.match(at)
            if mm and mm.group(1) == '29': sk = int(mm.group(2)) & ~15
            mm = re.match(r'^\(\(\(uint32_t\)\(r(\d+) \+ (-?\d+)\)\) & ~15u\) \+ 8$', at)
            if mm and mm.group(1) == '29': sk = (int(mm.group(2)) & ~15) + 8
            for x in LOC.finditer(at):
                skip_use.add(ap[0] + x.start())
                n = int(x.group(1))
                if n: ev.append((ap[0] + x.start(), 0, 'u', (n, 'L'), cond(m.start())))
            if vp:
                lv = re.fullmatch(r'r(\d+)', vt); gv = re.fullmatch(r'R\[(\d+)\]', vt)
                if lv:
                    n = int(lv.group(1)); skip_use.add(vp[0])
                    k = ('QS' if sk is not None else 'Q') if op == 'WR64' else ('F' if op in ('GSDL', 'GSDR') else 'L')
                    if n: ev.append((vp[0], 0, 'u', (n, k), cond(m.start())))
                    if n == 29: taken = True
                elif gv and int(gv.group(1)) and (sk is None or op != 'WR64'):     # R[n] itself: a register no path of the function writes
                    k = 'Q' if op == 'WR64' else ('F' if op in ('GSDL', 'GSDR') else 'L')
                    ev.append((vp[0], 0, 'ru', (int(gv.group(1)), k), cond(m.start())))
            if sk is not None:
                if op == 'WR64':
                    lv = re.fullmatch(r'r(\d+)|R\[(\d+)\]', vt)
                    val = (('L', int(lv.group(1))) if lv.group(1) else ('R', int(lv.group(2)))) if lv else ('W',)
                    ev.append((e, 1, 'st', (sk, val), cond(m.start())))
                else:
                    base = sk & ~7
                    ev.append((e, 1, 'st', (base, ('W',)), cond(m.start())))
                    if op in ('GSDL', 'GSDR', 'VSQC2'): ev.append((e, 1, 'st', (base + 8, ('W',)), cond(m.start())))
        # reads of the stack other than a 64-bit load into a local (float, vector, 8-32-bit, ldl / ldr, lwl / lwr, the upper qword of lq)
        for m in SPRD.finditer(t):
            if any(a_ <= m.start() < b_ for a_, b_ in lds): continue
            ev.append((m.start(), 0, 'sr', (int(m.group(2)), SPSZ[m.group(1)]), cond(m.start())))
        for m in SPVL.finditer(t): ev.append((m.start(), 0, 'sr', (int(m.group(1)) & ~15, 16), cond(m.start())))
        for m in SPGL.finditer(t):
            w = 8 if m.group(1) in ('GLDL', 'GLDR') else 4
            ev.append((m.start(), 0, 'sr', (int(m.group(2)) & ~(w - 1), w), cond(m.start())))
        for m in SPQA.finditer(t):
            q = qa_at(m.start())
            if q is None or q[0] != 29 or (not m.group(1) and any(a_ <= m.start() < b_ for a_, b_ in lds)): continue
            ev.append((m.start(), 0, 'sr', (q[1] + (8 if m.group(1) else 0), 8), cond(m.start())))
        # calls
        for a, b, nm in rc._calls(t):
            calls.add(nm)
            if nm == 'JALR': F.jalr = True
            if nm == 'GEN_IDIOM_QCOPY': F.idiom = True; continue
            if nm == 'BREAK':        # fatal: the stores before it do not matter
                d = 0; q = a - 1
                while q >= 0:
                    if t[q] == ')': d += 1
                    elif t[q] == '(':
                        if d == 0: break
                        d -= 1
                    q -= 1
                ev = [x for x in ev if not (x[2] == 's' and q <= x[0] < a)]
                continue
            t_ = rc.ipo_sets(nm)
            rin, rout, mout = (ALLM, ALLM & ~NOREL_M, 0) if t_ is None else (mask(t_[0]), mask(t_[1]), mask(t_[4]) if len(t_) > 4 else 0)
            ev.append((b, 2, 'c', (nm, rin, rout, mout), cond(a)))
        for m in GOTO.finditer(t): ev.append((m.start(), 3, 'g', m.group(1), cond(m.start())))
        for m in RET.finditer(t): ev.append((m.start(), 3, 'x', None, cond(m.start())))
        # every other local read: its context
        for m in LOC.finditer(t):
            p = m.start(); n = int(m.group(1))
            if p in skip_use or n == 0 or (li == 0 and 'GEN_IDIOM' in t): continue
            if t.startswith(' = ', m.end()) and not t.startswith(' = =', m.end()): continue
            pre = t[max(0, p - 10):p]
            if pre.endswith('(uint32_t)') or pre.endswith('(int32_t)'): k = 'L'
            elif re.match(r' & (?:31|63)\)', t[m.end():]): k = 'L'
            elif is_addr(t, p): k = 'L'
            elif t[max(0, p - 15):p].endswith('int64_t t_ = ') and re.match(r';', t[m.end():]): k = 'L'
            else:
                k = 'F'
                ctx = re.sub(r'r\d+', 'r', t[max(0, p - 18):m.end() + 12])
                if not re.search(r'(?:==|!=|<=|>=|<|>) |\((?:r|0) (?:==|!=|<=|>=|<|>)|hi_ = r', ctx): UNKNOWN['use ' + ctx] += 1
            if n == 29 and not is_addr(t, p): taken = True
            ev.append((p, 0, 'u', (n, k), cond(p)))
        ev.sort(key=lambda x: (x[0], x[1]))
        # blocks of this line
        bounds = [0] + [p for p, _ in splits] + [len(t) + 1]
        names = [fm.group(1) if fm else None] + [nm for _, nm in splits]
        jr = jump_regions(t, regsr) if regsr else []
        for j in range(len(bounds) - 1):
            a, b = bounds[j], bounds[j + 1]
            rin = [r for r in jr if a <= r[0] and r[1] <= b]
            bev = []
            for (p, o, k, d, c) in ev:
                if not (a <= p < b): continue
                r = None
                for r_ in rin:
                    if r_[0] <= p < r_[1]: r = r_; break
                if r is None: bev.append((k, d, c)); continue
                key = ('jr', li, r[0])
                if key not in xb: xb[key] = []; bev.append(('g', key, True))     # the body: a block of its own (jump_regions)
                xb[key].append((k, d, any(a2 <= p < b2 for a2, b2 in regsr if r[0] < a2 and b2 <= r[1])))
            if names[j]: lab[names[j]] = len(blocks)
            exit_ = any(k in ('g', 'x') and not c for k, d, c in bev)
            blocks.append([bev, not exit_, li])
    if xb:
        blocks.append([[('x', None, False)], False, len(lines) - 1])     # falling off the last line: a return (the bodies follow it)
        for key, bev in xb.items():
            if not any(k in ('g', 'x') and not c for k, d, c in bev): raise RuntimeError('%s line %d: jump region without its jump' % (name, key[1]))
            lab[key] = len(blocks); blocks.append([bev, False, key[1]])
    F.blocks = blocks; F.lab = lab; F.taken = taken; F.calls = calls; F.nb = len(blocks)
    for bl in blocks:
        bl[0] = [(k, (lab.get(d, -1) if k == 'g' else d), c) for k, d, c in bl[0]]
        if any(k == 'g' and d == -1 for k, d, c in bl[0]): UNKNOWN['goto to unknown label in ' + name] += 1
    F.savek = save_keys(F)
    return F

SAVED = set(range(16, 24)) | {30, 31}
def save_keys(F):
    """the stack slots (entry-relative) the function writes only with callee-saved registers (s0-s7, fp) or ra: its register save
    slots (a function with two frames may save different registers in the same slot).  Assumption A2: no store through a pointer
    writes a function's register save slots, so they keep what the function saved even when it has passed a stack address on (taken)"""
    N = F.nb
    if not N: return frozenset()
    spin = [None] * N; spin[0] = 0; seen = [False] * N; seen[0] = True; work = [0]; st = {}; ld = {}
    UNK = object()
    def mg(t, sp):
        if not seen[t]: seen[t] = True; spin[t] = sp; work.append(t)
        elif spin[t] is not None and spin[t] != sp: spin[t] = None; work.append(t)
    while work:
        i = work.pop(); sp = spin[i]; ev, falls, _ = F.blocks[i]
        for kind, d, c in ev:
            if kind == 'd':
                n, cls = d[0], d[1]
                if cls[0] == 'LDS' and sp is not None: ld.setdefault(sp + cls[1], set()).add(n)
                if n == 29:
                    if cls[0] == 'SP' and not c and sp is not None: sp += cls[1]
                    else: sp = None
            elif kind == 'st' and sp is not None:
                k, val = d
                st.setdefault(sp + k, set()).add(val[1] if val[0] in ('R', 'L') else UNK)
            elif kind == 'g' and d >= 0: mg(d, sp)
        if falls and i + 1 < N: mg(i + 1, sp)
    return frozenset(k for k, vs in st.items() if all(s is not UNK and s in SAVED for s in vs))


# ---- the guest values: registers 0-31, HI 32, LO 33, HI1 34, LO1 35 ----------------------------------------------------------------
# Phase 1a, per function: a summary of its effect, for each register at its returns: G (may be W whatever the entry values) and D (the
# registers whose W at entry may make it W).  A call applies the callee's summary to the caller's state, so a value passed through a
# function by one caller does not reach another caller.  Phase 1b: the entry sets E[f] (the W registers at f's calls; E_IND at the calls
# through a register, for every function a register call can reach).  Phase 2: the widths, per function, from E[f] and the summaries.
NR = 36; ROWM = (1 << NR) - 1
HLR = {1: 32, 2: 33, 4: 34, 8: 35}
ALLR = ROWM & ~1
HLM = 15 << 32
IDENT = 0
for _r in range(1, NR): IDENT |= 1 << (_r * NR + _r)
def row(Dm, r): return (Dm >> (r * NR)) & ROWM
def rregs(m): return [n for n in range(1, NR) if m >> n & 1]

def mksum(G, Dm):
    """a summary with its pass-through rows (unchanged registers) apart: (G, D, pt, pt rows, [(r, g, d, sources)])"""
    pt = 0; ptrows = 0; rows = []
    for r in range(1, NR):
        d = row(Dm, r); g = G >> r & 1
        if d == 1 << r and not g: pt |= 1 << r; ptrows |= ROWM << (r * NR)
        else: rows.append((r, g, d, rregs(d)))
    return (G, Dm, pt, ptrows, rows)
BOT = mksum(0, 0)      # no return seen yet
SUMS = {}; SUMIND = [BOT]; _UNK = {}; _HOST = {}; _JSIF = [None]
def _clear_rows(Dm, m):
    for r in rregs(m): Dm &= ~(ROWM << (r * NR))
    return Dm
def unknown_sum(rout):
    """a function of a unit not read in this run: W in every register it may write, HI / LO included"""
    s = _UNK.get(rout)
    if s is None: s = _UNK[rout] = mksum(rout | HLM, _clear_rows(IDENT, rout | HLM))
    return s
def host_sum(rout, mout):
    """a runtime call (SYSCALL, IRQ_POLL): S in the registers it always writes, the others unchanged or S"""
    s = _HOST.get((rout, mout))
    if s is None: s = _HOST[(rout, mout)] = mksum(0, _clear_rows(IDENT, rout & mout))
    return s
FUNCS = {}
def callsum(nm, rout, mout):
    if nm in FUNCS: return SUMS.get(nm, BOT)
    if nm == 'JALR': return SUMIND[0]
    if nm == 'JSIF':
        if _JSIF[0] is None:
            if all(t in FUNCS for t in SIF):
                g = 0; dm = 0
                for t in SIF: s = SUMS.get(t, BOT); g |= s[0]; dm |= s[1]
                _JSIF[0] = mksum(g, dm)
            else: _JSIF[0] = unknown_sum(ALLR)
        return _JSIF[0]
    if nm.startswith(('P', 'S_', 'O')): return unknown_sum(rout)
    return host_sum(rout, mout)

def rapply(G, Dm, S):
    """the state after a call: the callee's summary S over the caller's state (G, Dm)"""
    nG = G & S[2]; nD = Dm & S[3]
    for r, g, d, src in S[4]:
        dd = 0
        for s in src:
            g |= G >> s & 1; dd |= (Dm >> (s * NR)) & ROWM
        nG |= g << r; nD |= dd << (r * NR)
    return nG, nD

def capply(v, S):
    """the same for a concrete set v of W registers"""
    nv = v & S[2]
    for r, g, d, src in S[4]:
        if g or d & v: nv |= 1 << r
    return nv

IDIOM_X = (B(2) | B(3), _clear_rows(IDENT, B(2) | B(3) | B(4) | B(5) | B(6) | B(7)))   # rt_idiom_qcopy: v0 / v1 copied data, a0-a3 S

def rmerge(din, t, st, work, inq):
    if din[t] is None: din[t] = (st[0], st[1], st[2], dict(st[3]))
    else:
        G0, D0, sp0, sl0 = din[t]; G, Dm, sp, sl = st
        nsp = sp0 if sp0 == sp else None
        nsl = {} if nsp is None else {k: (a[0] | b[0], a[1] | b[1]) for k, a in sl0.items() for b in (sl.get(k),) if b is not None}
        new = (G0 | G, D0 | Dm, nsp, nsl)
        if new == din[t]: return
        din[t] = new
    if t not in inq: work.append(t); inq.add(t)

def rel(F):
    """phase 1a: F's summary (G, D at its returns)"""
    N = F.nb
    if not N: return 0, IDENT
    din = [None] * N; din[0] = (0, IDENT, 0, {}); work = [0]; inq = {0}
    XG, XD = IDIOM_X if F.idiom else (0, 0)
    while work:
        i = work.pop(); inq.discard(i)
        G, Dm, sp, sl = din[i]; sl = dict(sl); ev, falls, _ = F.blocks[i]
        for kind, d, c in ev:
            if kind == 'd':
                n, cls, deps = d[:3]; t = cls[0]
                if t == 'REL': continue
                if t == 'S': g = 0; dd = 0
                elif t == 'MV': s = cls[1]; g = G >> s & 1; dd = (Dm >> (s * NR)) & ROWM
                elif t == 'P':
                    m = cls[1]; g = 1 if G & m else 0; dd = 0
                    for s in rregs(m): dd |= (Dm >> (s * NR)) & ROWM
                elif t == 'HL': s = HLR[cls[1]]; g = G >> s & 1; dd = (Dm >> (s * NR)) & ROWM
                elif t == 'LDS':
                    key = None if sp is None else sp + cls[1]
                    x = None if (key is None or (F.taken and key not in F.savek)) else sl.get(key)
                    if x is None: g = 1; dd = 0
                    else: g, dd = x
                elif t == 'SP':
                    g = 0; dd = 0
                    if n == 29:
                        if c: sp = None; sl = {}
                        elif sp is not None: sp += cls[1]
                else: g = 1; dd = 0
                if n == 29 and t != 'SP': sp = None; sl = {}
                sh = n * NR
                if c: G |= g << n; Dm |= dd << sh
                else: G = (G & ~(1 << n)) | (g << n); Dm = (Dm & ~(ROWM << sh)) | (dd << sh)
            elif kind == 'hw':
                bits, val = d
                if val[0] == 'R': s = val[1]; g = G >> s & 1; dd = (Dm >> (s * NR)) & ROWM
                elif val[0] == 'W': g = 1; dd = 0
                else: g = 0; dd = 0
                for b_, r in HLR.items():
                    if bits & b_:
                        sh = r * NR
                        if c: G |= g << r; Dm |= dd << sh
                        else: G = (G & ~(1 << r)) | (g << r); Dm = (Dm & ~(ROWM << sh)) | (dd << sh)
            elif kind == 'st':
                k, val = d
                if sp is None: continue
                key = sp + k
                x = (G >> val[1] & 1, (Dm >> (val[1] * NR)) & ROWM) if val[0] in ('R', 'L') else None
                if c:
                    o = sl.get(key)
                    if o is not None:
                        if x is None: del sl[key]
                        else: sl[key] = (o[0] | x[0], o[1] | x[1])
                elif x is None: sl.pop(key, None)
                else: sl[key] = x
            elif kind == 'c':
                nm, rin, rout, mout = d
                if nm in ('BREAK', 'COP0OP', 'COP0W'): continue
                nG, nD = rapply(G, Dm, callsum(nm, rout, mout))
                if c: G |= nG; Dm |= nD
                else: G, Dm = nG, nD
            elif kind == 'g':
                if d >= 0: rmerge(din, d, (G, Dm, sp, sl), work, inq)
            elif kind == 'x':
                XG |= G; XD |= Dm
        if falls:
            if i + 1 < N: rmerge(din, i + 1, (G, Dm, sp, sl), work, inq)
            else: XG |= G; XD |= Dm
    return XG, XD

E = {}; G_ = {'E_IND': 0}

def analyse(F, check):
    """phases 1b / 2, with concrete W sets: (flags {n: why}, contributions {callee: entry W}, W at the calls through a register)"""
    e0 = E.get(F.name, 0) | (G_['E_IND'] if F.ind else 0)
    wide = mask(F.wide) if check else 0
    N = F.nb
    if not N: return {}, {}, 0
    din = [None] * N; din[0] = (e0, 0, frozenset()); work = [0]; inq = {0}
    flags = {}; contrib = {}; eind = 0
    def flag(n, why):
        if n not in flags: flags[n] = why
    while work:
        i = work.pop(); inq.discard(i)
        v, sp, sl = din[i]; ev, falls, _ = F.blocks[i]
        for kind, d, c in ev:
            if kind == 'u':
                n, k = d
                if k != 'L' and check and v >> n & 1 and not wide >> n & 1: flag(n, k)
            elif kind == 'd':
                n, cls, deps = d[:3]; t = cls[0]
                if check and wide >> n & 1 and deps & v & ~wide:
                    for s in regs(deps & v & ~wide): flag(s, 'D')
                if t == 'REL': continue
                if t == 'S': w = 0
                elif t == 'MV': w = v >> cls[1] & 1
                elif t == 'P': w = 1 if v & cls[1] else 0
                elif t == 'HL': w = v >> HLR[cls[1]] & 1
                elif t == 'LDS':
                    if sp is None: w = 1
                    else:
                        key = sp + cls[1]
                        w = 1 if (F.taken and key not in F.savek) else (0 if key in sl else 1)
                elif t == 'SP':
                    w = 0
                    if n == 29:
                        if c: sp = None; sl = frozenset()
                        elif sp is not None: sp += cls[1]
                else: w = 1
                if n == 29 and t != 'SP': sp = None; sl = frozenset()
                if c: v |= w << n
                else: v = (v & ~B(n)) | (w << n)
            elif kind == 's':
                if check and NOS == 0 and v >> d & 1 and not wide >> d & 1: flag(d, 'S')
            elif kind == 'hw':
                bits, val = d
                w = (v >> val[1] & 1) if val[0] == 'R' else (1 if val[0] == 'W' else 0)
                for b_, r in HLR.items():
                    if bits & b_: v = (v | (w << r)) if c else ((v & ~B(r)) | (w << r))
            elif kind == 'st':
                k, val = d
                if sp is None: continue
                key = sp + k
                w = (v >> val[1] & 1) if val[0] in ('R', 'L') else 1
                if w: sl = sl - {key}
                elif not c: sl = sl | {key}
            elif kind == 'c':
                nm, rin, rout, mout = d
                if nm in FUNCS: contrib[nm] = contrib.get(nm, 0) | v
                elif nm == 'JALR': eind |= v
                elif nm == 'JSIF':
                    for tg in SIF: contrib[tg] = contrib.get(tg, 0) | v
                if nm in ('BREAK', 'COP0OP', 'COP0W'): continue
                nv = capply(v, callsum(nm, rout, mout))
                v = (v | nv) if c else nv
            elif kind == 'g':
                if d >= 0: merge(din, d, (v, sp, sl), work, inq)
        if falls and i + 1 < N: merge(din, i + 1, (v, sp, sl), work, inq)
    return flags, contrib, eind

def merge(din, t, o, work, inq):
    v, sp, sl = o
    if din[t] is None: din[t] = o
    else:
        a, s, l = din[t]
        ns = s if s == sp else None; nl = (l & sl) if ns is not None else frozenset()
        if (a | v, ns, nl) == din[t]: return
        din[t] = (a | v, ns, nl)
    if t not in inq: work.append(t); inq.add(t)

def postorder(names, callees):
    """callees before callers (iterative DFS), for phase 1a's first sweep"""
    seen = set(); out = []
    for r in names:
        if r in seen: continue
        seen.add(r); st = [(r, iter(sorted(callees[r])))]
        while st:
            n, it = st[-1]
            for c in it:
                if c not in seen: seen.add(c); st.append((c, iter(sorted(callees[c])))); break
            else: st.pop(); out.append(n)
    return out

CG = {}
INDN = set()     # the functions a call through a register or the runtime may enter (taken.py: address taken)
DEVX = int(os.environ.get('WPO_DEVX', '0'))   # dev bound: an ELF function called by name from a stage not read is not taken as entered so
def load_all(units, t0):
    US = {}; callers = collections.defaultdict(set); callees = collections.defaultdict(set); opens = []; jalrs = []
    tk = json.load(open(os.environ.get('WPO_TAKEN', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'taken.json'))))
    INDN.clear()
    for u in units: INDN.update(tk['taken'][u])
    if not DEVX:
        for f, us in tk['xcalls'].items():
            if any(x not in units for x in us): INDN.add(f)   # its callers in a unit not read here: unknown, as a register call's
    for u in units:
        U = D.load(u); US[u] = U; D.ctx(U)
        for name in U.order:
            F = parse(name, U, D.emit(U, name, set()), set())
            FUNCS[name] = F
        print('%s: %d functions parsed (%.0f s)' % (u, len(U.order), time.time() - t0), flush=True)
    for name, F in FUNCS.items():
        for c in F.calls:
            ts = [c] if c in FUNCS else (list(SIF) if c == 'JSIF' else [])
            for tg in ts:
                if tg in FUNCS: callers[tg].add(name); callees[name].add(tg)
        if F.ind: opens.append(name)
        if F.jalr: jalrs.append(name)
    CG.update(callers=callers, callees=callees, opens=opens, jalrs=jalrs, US=US)
    print('unknown forms: %d kinds' % len(UNKNOWN), flush=True)
    for k, v in UNKNOWN.most_common(40): print('  %6d %s' % (v, k))

def phase1(t0):
    callers = CG['callers']; callees = CG['callees']; opens = CG['opens']; jalrs = CG['jalrs']
    # phase 1a: the summaries
    order = postorder(list(FUNCS), callees)
    work = collections.deque(order); inq = set(order); ev = 0
    while work:
        name = work.popleft(); inq.discard(name); F = FUNCS[name]; ev += 1
        XG, XD = rel(F)
        o = SUMS.get(name)
        if o is not None and (XG | o[0], XD | o[1]) == (o[0], o[1]): continue
        if o is not None: XG |= o[0]; XD |= o[1]
        SUMS[name] = mksum(XG, XD); push = list(callers[name])
        if name in SIF: _JSIF[0] = None
        if F.ind:
            si = SUMIND[0]
            if (si[0] | XG, si[1] | XD) != (si[0], si[1]): SUMIND[0] = mksum(si[0] | XG, si[1] | XD); push += jalrs
        for p in push:
            if p not in inq: work.append(p); inq.add(p)
        if ev % 5000 == 0: print('  phase 1a: %d evaluations, %d queued (%.0f s)' % (ev, len(work), time.time() - t0), flush=True)
    si = SUMIND[0]
    print('phase 1a: %d evaluations (%.0f s); calls through a register: W generated %s, pass-through rows %s' %
          (ev, time.time() - t0, rregs(si[0]), len(si[4])), flush=True)
    # phase 1b: the entry sets
    work = collections.deque(order[::-1]); inq = set(order); ev = 0
    while work:
        name = work.popleft(); inq.discard(name); F = FUNCS[name]; ev += 1
        _, contrib, eind = analyse(F, False)
        push = []
        for c, m in contrib.items():
            if m & ~E.get(c, 0): E[c] = E.get(c, 0) | m; push.append(c)
        if NOIND: eind = 0
        if eind & ~G_['E_IND']: G_['E_IND'] |= eind; push += opens
        for p in push:
            if p not in inq: work.append(p); inq.add(p)
    print('phase 1b: %d evaluations (%.0f s); E_IND %s' % (ev, time.time() - t0, rregs(G_['E_IND'])), flush=True)

def main():
    out = sys.argv[1]; units = sys.argv[2:] or D.UNITS; t0 = time.time()
    keep = set(os.environ.get('WPO_KEEP', 'P12dbc0,S_w00a_P333380').split(','))   # natively hooked: their hooks name the v27 locals
    load_all(units, t0); phase1(t0); US = CG['US']
    # phase 2: the widths, per function
    WR = {}; why = {}; rounds = collections.Counter()
    for u in units:
        U = US[u]; D.ctx(U)
        for name in U.order:
            F = FUNCS[name]
            if name in keep:
                WR[name] = set(range(64)) if D.IS_WIDE(U.bodies[name]) else set(); why[name] = {'keep': 1}; continue
            wr = set(); r = 0
            while True:
                fl, _, _ = analyse(F, True)
                r += 1
                if not fl: break
                for n, k in fl.items():
                    if n not in wr: why.setdefault(name, {})[n] = k
                wr |= set(fl)
                F = parse(name, U, D.emit(U, name, wr), wr)
                if r > 40: print('no fixpoint:', name); break
            WR[name] = wr; rounds[r] += 1
        print('%s: widths done (%.0f s)' % (u, time.time() - t0), flush=True)
    print('rounds', sorted(rounds.items()))
    print('unknown forms after phase 2: %d kinds' % len(UNKNOWN))
    for k, v in UNKNOWN.most_common(40): print('  %6d %s' % (v, k))
    sumj = {k: {'G': rregs(s[0]), 'D': {str(r): rregs(d) for r, g, d, src in s[4] if d}} for k, s in SUMS.items()}
    json.dump({'wr': {k: sorted(v) for k, v in WR.items()}, 'why': {k: {str(a): b for a, b in v.items()} for k, v in why.items()},
               'E': {k: rregs(v) for k, v in E.items() if v}, 'E_IND': rregs(G_['E_IND']), 'SUM': sumj,
               'SUMIND_G': rregs(SUMIND[0][0]), 'A1': A1}, open(out, 'w'))
    print('done (%.0f s)' % (time.time() - t0))

if __name__ == '__main__': main()
