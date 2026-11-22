#!/usr/bin/env python3
"""ctxcount.py <gi counts> <steps> <header>...: the generated code's register-file (context) traffic per step by where it happens, from an
x86 run's guest instruction counts (tools/dev/gi_count.py + boot.c GEN_GI_COUNT: "pc count dg_count", dg = inside the draw pipeline).
A line's runs = its count / k (k as gi_count.py: 2 on a line with a control transfer, 1 otherwise, plus one per inlined marker).
Kinds: call sites by callee (direct in the same unit, direct into another unit, through a register (JALR / JSIF), host: SYSCALL, BREAK,
COP0OP), returns, function entries (the declaration loads, weighted by the runs of the function's first line), write-through moves in
bodies (RCLD / RCMV), other R[] reads in bodies, polls (IRQ_POLLF stores: they run only when the poll is due, so not weighted).
PSP instructions (callcost.py's model): a store of an int32_t local to R[] 3 (sra + 2 sw), of an int64_t local 2; a reload 1 / 2; F[] 1;
RCLD / RCMV 2 (a 64-bit store; GCC forwards the reload).  Conditional host calls (BREAK under its test) and conditional returns are
weighted as if taken every time the line runs: upper bounds."""
import re, sys
from collections import defaultdict
MARK = re.compile(r'^/\*([0-9a-f]{6})\*/', re.M)
INNER = re.compile(r'/\*[0-9a-f]{6}\*/')
XFER = re.compile(r'\bgoto\b|\breturn\b|\bJALR\(|\bJR\(|\b(?:S_\w+_)?P[0-9a-f]{5,6}\(\)')
FN = re.compile(r'^static void ((?:S_\w+_)?P[0-9a-f]{5,6})\(void\)\n\{\n(.*?)\n\}$', re.S | re.M)
CALLEE = re.compile(r'\b((?:S_\w+_)?P[0-9a-f]{5,6})\(\)|\b(JALR|JSIF|SYSCALL|BREAK|COP0OP)\(')
ST = re.compile(r'\bR\[(\d+)\] = r(\d+)\b')
LD = re.compile(r'\br(\d+) = (?:\(int32_t\))?R\[(\d+)\]')
FST = re.compile(r'\bF\[(\d+)\] = f(\d+)\b')
FLD = re.compile(r'\bf(\d+) = F\[(\d+)\]')
RET = re.compile(r'\{ ([^{}]*?); (?:\{ ([^{}]*?); )?return; \}(?: \})?')
RCX = re.compile(r'\bRC(LD|MV)\(')
RREAD = re.compile(r'\bR\[(\d+)\](?! = )')
DECL_R = re.compile(r'\br(\d+) = (?:\(int32_t\))?R\[(\d+)\]')
DECL_F = re.compile(r'\bf(\d+) = F\[(\d+)\]')

def group(ln, pos):
    """the innermost parenthesised group around pos: (start, end) of its text"""
    d = 0; i = pos - 1
    while i >= 0:
        c = ln[i]
        if c == ')': d += 1
        elif c == '(':
            if d == 0: break
            d -= 1
        i -= 1
    if i < 0: return None
    d = 0; j = i
    while j < len(ln):
        c = ln[j]; d += (c == '(') - (c == ')')
        if d == 0: return i, j + 1
        j += 1
    return None

def spans(s, name):
    out = []; i = 0
    while True:
        j = s.find(name + '(', i)
        if j < 0: return out
        d = 0; k = j + len(name)
        while k < len(s):
            c = s[k]; d += (c == '(') - (c == ')'); k += 1
            if d == 0: break
        out.append((j, k)); i = k

def scan(path, unit, defined):
    res = {}; entries = []
    src = open(path).read()
    for m in FN.finditer(src):
        name, body = m.group(1), m.group(2)
        w64 = set(); ent = defaultdict(int); first = None
        for ln in body.split('\n'):
            if ln.startswith('  ') and not ln.startswith('  /*') and first is None and ('int64_t' in ln or 'int32_t' in ln or 'float' in ln):
                for part in ln.split(';'):
                    part = part.strip()
                    if part.startswith('int64_t'): w64 |= {int(x) for x in re.findall(r'\br(\d+)\b', part)}
                for x in DECL_R.finditer(ln):
                    if x.group(1) == x.group(2): ent['entry ld'] += 1; ent['entry instr'] += 2 if int(x.group(1)) in w64 else 1
                for x in DECL_F.finditer(ln):
                    if x.group(1) == x.group(2): ent['entry F ld'] += 1; ent['entry instr'] += 1
                continue
            mm = MARK.match(ln)
            if not mm: continue
            pc = int(mm.group(1), 16)
            if first is None: first = pc
            k = (2 if XFER.search(ln) else 1) + len(INNER.findall(ln, mm.end()))
            c = defaultdict(float)
            cut = spans(ln, 'IRQ_POLLF')
            for a, b in cut:
                seg = ln[a:b]; c['poll st (static)'] += len(ST.findall(seg)) + len(FST.findall(seg))
            text = ''.join(ln[p:q] for p, q in zip([0] + [b for a, b in cut], [a for a, b in cut] + [len(ln)]))
            used = []
            for cm in CALLEE.finditer(text):
                if cm.group(1):
                    own = cm.group(1).startswith('S_') or unit == 'elf'
                    kind = 'call same unit' if own else 'call into ELF'
                else:
                    kind = {'JALR': 'call indirect', 'JSIF': 'call indirect', 'SYSCALL': 'host SYSCALL', 'BREAK': 'host BREAK (cond.)', 'COP0OP': 'host COP0OP'}[cm.group(2)]
                g = group(text, cm.start())
                if not g: continue
                while True:   # nested call groups: (R stores, (F stores, callee, F reloads), R reloads)
                    pg = group(text, g[0])
                    if not pg or (pg[0] > 0 and (text[pg[0] - 1].isalnum() or text[pg[0] - 1] == '_')) or not text.startswith(('(R[', '(F[', '(('), pg[0]): break
                    g = pg
                a, b = g; pre = text[a:cm.start()]; post = text[cm.end():b]
                c[kind + ' #'] += 1
                for x in ST.finditer(pre): c[kind + ' st'] += 1; c[kind + ' instr'] += 2 if int(x.group(2)) in w64 else 3
                for x in FST.finditer(pre): c[kind + ' F st'] += 1; c[kind + ' instr'] += 1
                for x in LD.finditer(post):
                    if x.group(1) == x.group(2): c[kind + ' ld'] += 1; c[kind + ' instr'] += 2 if int(x.group(1)) in w64 else 1
                for x in FLD.finditer(post):
                    if x.group(1) == x.group(2): c[kind + ' F ld'] += 1; c[kind + ' instr'] += 1
                used.append((a, b))
            rest = ''.join(text[p:q] for p, q in zip([0] + [b for a, b in used], [a for a, b in used] + [len(text)]))
            for r in RET.finditer(rest):
                seg = r.group(0); c['return #'] += 1
                for x in ST.finditer(seg): c['return st'] += 1; c['return instr'] += 2 if int(x.group(2)) in w64 else 3
                for x in FST.finditer(seg): c['return F st'] += 1; c['return instr'] += 1
            rest = RET.sub('', rest)
            for x in ST.finditer(rest): c['other st'] += 1; c['other instr'] += 2 if int(x.group(2)) in w64 else 3
            for x in FST.finditer(rest): c['other F st'] += 1; c['other instr'] += 1
            n = len(RCX.findall(rest)); c['write-through #'] += n; c['write-through instr'] += 2 * n
            n = len([x for x in RREAD.finditer(rest)]) - len(ST.findall(rest)); c['body R read'] += max(n, 0); c['body R read instr'] += max(n, 0)
            res[pc] = (k, c)
        if first is not None and ent: entries.append((first, ent))
    return res, entries

gi = {}
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 3: gi[int(p[0], 16)] = (int(p[1]), int(p[2]))
steps = float(sys.argv[2]); heads = sys.argv[3:]
defined = {}
for h in heads:
    u = 'elf' if h.endswith('gen_elf.h') else re.sub(r'^.*gen_s_(\w+)\.h$', r'\1', h)
    defined[u] = {int(x, 16) for x in re.findall(r'^static void P([0-9a-f]{5,6})\(void\)$', open(h).read(), re.M)}
tot = {u: [defaultdict(float), defaultdict(float)] for u in ['elf', 'stage']}
for h in heads:
    u = 'elf' if h.endswith('gen_elf.h') else re.sub(r'^.*gen_s_(\w+)\.h$', r'\1', h)
    res, entries = scan(h, u, defined)
    T = tot['elf' if u == 'elf' else 'stage']
    for pc, (k, c) in res.items():
        if pc not in gi: continue
        n, d = gi[pc]
        for kk, v in c.items():
            if kk.endswith('(static)'): T[0][kk] += v; continue
            T[0][kk] += v * n / k; T[1][kk] += v * d / k
    for first, ent in entries:
        if first not in gi: continue
        n, d = gi[first]
        k = res[first][0] if first in res else 1
        for kk, v in ent.items(): T[0][kk] += v * n / k; T[1][kk] += v * d / k
        T[0]['entry #'] += n / k; T[1]['entry #'] += d / k
kinds = ['call same unit', 'call into ELF', 'call other unit', 'call indirect', 'host SYSCALL', 'host BREAK (cond.)', 'host COP0OP', 'return', 'entry', 'write-through', 'body R read', 'other']
print('per step over %g steps (logic = outside the draw pipeline P11da90; draw = inside it)' % steps)
print('%-8s %-20s %10s %9s %9s %9s %9s %11s' % ('unit', 'kind', 'events', 'R st', 'F st', 'R ld', 'F ld', 'PSP instr'))
for u in ['elf', 'stage']:
    for part, name in ((0, 'logic'), (1, 'draw')):
        T = tot[u][part]; s = 0
        for kd in kinds:
            ev = T.get(kd + ' #', 0); ins = T.get(kd + ' instr', 0); s += ins
            if ev == 0 and ins == 0: continue
            print('%-8s %-20s %10.0f %9.0f %9.0f %9.0f %9.0f %11.0f' % (u + ' ' + name, kd, ev / steps, T.get(kd + ' st', 0) / steps, T.get(kd + ' F st', 0) / steps,
                  (T.get(kd + ' ld', 0) + (T.get('entry ld', 0) if kd == 'entry' else 0)) / steps, (T.get(kd + ' F ld', 0) + (T.get('entry F ld', 0) if kd == 'entry' else 0)) / steps, ins / steps))
        print('%-8s %-20s %10s %9s %9s %9s %9s %11.0f  (%.2f ms at 333 MHz, 1 instr / cycle)' % (u + ' ' + name, 'TOTAL', '', '', '', '', '', s / steps, s / steps / 333e3))
    if tot[u][0].get('poll st (static)'): print('%-8s poll stores (static count): %d' % (u, tot[u][0]['poll st (static)']))
