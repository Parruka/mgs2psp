"""vu_local.py <in header> <out header> [names]: the VU0 registers of each generated function in C locals instead of the global VU state.
The macro-mode code (recomp/vuemit.py) reads and writes VF[r].u[c], its clamped shadow VN[r].u[c] and VACC[c] in memory at every op
(about 19 PSP instructions per guest VU0 op, speed_notes.md 20).  Here a function that uses them gets locals vf<r>_<c>, vn<r>_<c>,
vacc<c>, loaded at entry and stored back (VLOC_OUT) / reloaded (VLOC_IN) wherever other code can see or change the VU state:
calls (P, S_<stage>_P, SYSCALL, JALR, BREAK, COP0W, rt_missing), the loop polls (IRQ_POLLF -> IRQ_POLLV), the generic VU ops
(VUM / VUM0 / VCFC2 / VCTC2), every return and the end, and the slow path of guest stores (gen_pre.h: GUEST_STORE / GUEST_STORE16
call VLOC_OUT / VLOC_IN around rt_store / rt_st*: an I/O store can run VU0 code, which boot.c's g_vu_ext also assumes; an I/O load
cannot, so loads keep MM).  VLQC2 / VSQC2 move between guest memory and the locals; VQMTC2 / VQMFC2 go through the global register.
Exact by construction: the same C operations on the same values, only where the values live changes (x86 and PSP hashes check it).
Each store-back / reload is code (2 x locals instructions per site), so a function is rewritten only when
(boundary sites + store sites) x locals <= K x its VU0 ops (K = 12; MGS2 v1 wrote all 258 and the w00a overlay outgrew its arena).
names: only these functions (comma list); K=<n> sets K."""
import re, sys
src = open(sys.argv[1]).read().split('\n'); only = None; KF = 12
for a in sys.argv[3:]:
    if a.startswith('K='): KF = float(a[2:])
    else: only = set(a.split(','))
ST = re.compile(r'\bWR(?:8|16|32|64|F)\(|\bVSQC2\(')
VUOP = re.compile(r'VU_MCL[1-4]\(|vu_max|vu_min|VLQC2\(|VSQC2\(')
def pays(body, regs, acc):
    t = '\n'.join(body); L = 4 * len(regs) + (4 if acc else 0)
    sites = len(CALL.findall(t)) + len(re.findall(r'IRQ_POLL[FV]\(', t)) + t.count('return;') + 1 + len(ST.findall(t))
    return sites * L <= KF * max(len(VUOP.findall(t)), 1)
DEF = re.compile(r'^static void ([A-Za-z0-9_]+)\(void\)$')
VFR = re.compile(r'\bVF\[(\d+)\]\.u\[(\d)\]'); VNR = re.compile(r'\bVN\[(\d+)\]\.u\[(\d)\]'); VAC = re.compile(r'\bVACC\[(\d)\]')
CALL = re.compile(r'\b(?:S_\w+?_)?P[0-9a-f]{5,6}\(\)|\b(?:SYSCALL|JALR|BREAK|COP0W|rt_missing|VUM0|VUM|VCTC2|VCFC2)\(')
def close(s, i):   # s[i] == '(' -> index after its matching ')'
    d = 0
    for j in range(i, len(s)):
        if s[j] == '(': d += 1
        elif s[j] == ')':
            d -= 1
            if not d: return j + 1
    raise ValueError('unbalanced: ' + s[:120])
def wrap_calls(s):
    out, i = [], 0
    while True:
        m = CALL.search(s, i)
        if not m: out.append(s[i:]); return ''.join(out)
        e = m.end() if m.group(0).endswith('()') else close(s, m.end() - 1); call = s[m.start():e]
        if call.startswith('VCFC2('): w = '({ VLOC_OUT; uint32_t c2_ = %s; VLOC_IN; c2_; })' % call
        else: w = '(VLOC_OUT, %s, VLOC_IN)' % call
        out.append(s[i:m.start()] + w); i = e
def body_regs(body):
    regs, vn, acc = set(), set(), False
    for l in body:
        for r, c in VFR.findall(l): regs.add(int(r))
        for r, c in VNR.findall(l): vn.add((int(r), int(c)))
        if VAC.search(l): acc = True
        for r in re.findall(r'\bVLQC2\((\d+),', l) + re.findall(r'\bVSQC2\((\d+),', l): regs.add(int(r))
        for r in re.findall(r'\bVQMTC2\(\d+, (\d+)\)', l) + re.findall(r'\bVQMFC2\(\d+, (\d+)\)', l): regs.add(int(r))
    regs.discard(0)
    return sorted(regs), sorted(vn), acc
def sub_arg(s, name, fn):   # name(arg1, rest) -> fn(arg1, rest) with balanced parens
    out, i = [], 0
    while True:
        j = s.find(name + '(', i)
        if j < 0 or (j > 0 and (s[j - 1].isalnum() or s[j - 1] == '_')):
            if j < 0: out.append(s[i:]); return ''.join(out)
            out.append(s[i:j + 1]); i = j + 1; continue
        e = close(s, j + len(name)); inner = s[j + len(name) + 1:e - 1]; k = inner.index(',')
        out.append(s[i:j] + fn(inner[:k].strip(), inner[k + 1:].strip())); i = e
def rewrite(name, body, regs, vn, acc):
    vf = ['vf%d_%d' % (r, c) for r in regs for c in range(4)]
    outs = ['VF[%d].u[%d] = vf%d_%d' % (r, c, r, c) for r in regs for c in range(4)] + (['VACC[%d] = vacc%d' % (c, c) for c in range(4)] if acc else [])
    ins = ['vf%d_%d = VF[%d].u[%d]' % (r, c, r, c) for r in regs for c in range(4)] + (['vacc%d = VACC[%d]' % (c, c) for c in range(4)] if acc else [])
    pre = ['#pragma push_macro("VLOC_OUT")', '#pragma push_macro("VLOC_IN")', '#undef VLOC_OUT', '#undef VLOC_IN',
           '#define VLOC_OUT (%s, (void)0)' % ', '.join(outs) if outs else '#define VLOC_OUT ((void)0)',
           '#define VLOC_IN (%s, (void)0)' % ', '.join(ins) if ins else '#define VLOC_IN ((void)0)']
    decl = []
    if regs: decl.append('  uint32_t %s;' % ', '.join('vf%d_%d = VF[%d].u[%d]' % (r, c, r, c) for r in regs for c in range(4)))
    if acc: decl.append('  uint32_t %s;' % ', '.join('vacc%d = VACC[%d]' % (c, c) for c in range(4)))
    if vn: decl.append('  uint32_t %s;' % ', '.join('vn%d_%d = 0' % rc for rc in vn))
    nb = []; at = None
    for k, l in enumerate(body):
        if at is None and re.match(r'^  int32_t r0 = 0', l): at = k + 1
        if l.startswith('FN_ENTER(') and at is None: at = k + 1
        l = wrap_calls(l)
        l = l.replace('IRQ_POLLF(', 'IRQ_POLLV(')   # IRQ_POLLV(flush, after): add the VU ones
        out = []
        i = 0
        while True:
            j = l.find('IRQ_POLLV(', i)
            if j < 0: out.append(l[i:]); break
            e = close(l, j + 9); inner = l[j + 10:e - 1]; d = 0; k = -1
            for q, ch in enumerate(inner):   # a top-level comma: vu_hoist's IRQ_POLLV(flush, after) already
                if ch in '([{': d += 1
                elif ch in ')]}': d -= 1
                elif ch == ',' and d == 0: k = q; break
            if k < 0: out.append(l[i:j] + 'IRQ_POLLV(((void)(%s), VLOC_OUT), VLOC_IN)' % inner)
            else: out.append(l[i:j] + 'IRQ_POLLV(((void)(%s), VLOC_OUT), (VLOC_IN, (void)(%s)))' % (inner[:k], inner[k + 1:]))
            i = e
        l = ''.join(out)
        # the locals first: the moves below write VF[...] on purpose (the global register around VQMTC2 / VQMFC2)
        l = VFR.sub(lambda m: m.group(0) if m.group(1) == '0' else 'vf%s_%s' % m.groups(), l); l = VNR.sub(lambda m: 'vn%s_%s' % m.groups(), l); l = VAC.sub(lambda m: 'vacc%s' % m.group(1), l)
        l = sub_arg(l, 'VLQC2', lambda r, a: '(void)0' if r == '0' else 'VLQ_L(%s, vf%s_0, vf%s_1, vf%s_2, vf%s_3)' % (a, r, r, r, r))
        l = sub_arg(l, 'VSQC2', lambda r, a: 'VSQ_L(%s, %s)' % (a, ', '.join(('vf%s_%d' % (r, c)) if r != '0' else ('VF[0].u[%d]' % c) for c in range(4))))
        l = sub_arg(l, 'VQMTC2', lambda t, d: '(VQMTC2(%s, %s)%s)' % (t, d, ''.join(', vf%s_%d = VF[%s].u[%d]' % (d, c, d, c) for c in range(4)) if d != '0' else ''))
        l = sub_arg(l, 'VQMFC2', lambda t, d: '(%sVQMFC2(%s, %s))' % (''.join('VF[%s].u[%d] = vf%s_%d, ' % (d, c, d, c) for c in range(4)) if d != '0' else '', t, d))
        l = re.sub(r'\breturn;', '{ VLOC_OUT; return; }', l)
        nb.append(l)
    if at is None: at = 1
    nb[at:at] = decl
    assert nb[-1] == '}', name
    nb.insert(len(nb) - 1, '  VLOC_OUT;')
    post = ['#pragma pop_macro("VLOC_OUT")', '#pragma pop_macro("VLOC_IN")']
    return pre, nb, post
out = []; i = 0; n = 0; nf = 0
while i < len(src):
    m = DEF.match(src[i])
    if not m or i + 1 >= len(src) or src[i + 1] != '{': out.append(src[i]); i += 1; continue
    j = i + 1
    while src[j] != '}': j += 1
    body = src[i + 1:j + 1]; nf += 1
    regs, vn, acc = body_regs(body)
    if (regs or vn or acc) and (only is None or m.group(1) in only) and (only is not None or pays(body, regs, acc)):
        pre, nb, post = rewrite(m.group(1), body, regs, vn, acc); out += pre + [src[i]] + nb + post; n += 1
    else: out += src[i:j + 1]
    i = j + 1
open(sys.argv[2], 'w').write('\n'.join(out))
sys.stderr.write('%s: %d of %d functions with VU0 locals\n' % (sys.argv[2], n, nf))
