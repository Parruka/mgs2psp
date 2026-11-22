#!/usr/bin/env python3
"""vu_hoist.py <gen header> [...]: (in place) takes the VU0 macro-op input clamps that repeat in every iteration of a simple loop out of it.

A macro op clamps each VF input it reads (`VN[x].u[k] = vu_u(vu_in(vu_f(VF[x].u[k])));`) and its ACC input (`vu_in(vu_f(VACC[k]))`).  In a
simple loop (one label, one back-edge at its end, entered only by falling into the label, no other label inside, no call, no micro
program) whose body never writes VF[x], VN[x] = clamp(VF[x]) is the same in every iteration: it is computed once before the label
and the clamps in the body go.  ACC is written only by macro-op results, which are VU-form (x86: VU_MCL clamps them; PSP: round toward
zero + flush to zero, vu0.h VU_MCL_RZ), so once ACC is VU-form at the loop entry its clamps in the body are the identity: the preheader
clamps ACC in place (invisible: every read of ACC goes through such a clamp) and the body reads it as it is.  Only the runtime's slow
paths can change VF / ACC behind the code's back: loads never run a device (boot.c rt_mm), a store outside RAM (boot.c rt_store) can
start VIF0 / a micro program, and the poll can run interrupt handlers (which may use VN, our scratch).  Both slow paths count in
g_vu_ext: a loop that stores keeps the count at entry, and checks it at the top of the body and after each store, redoing the
preheader clamps when it moved (VH_CHK, gen_pre.h); a loop without stores redoes them after the poll's slow path (IRQ_POLLV).  Used
only where the clamps taken out outweigh the checks.  Exact on both hosts (check: x86 hashes).  Idempotent; prints the loops changed
and the clamps taken out."""
import sys, re
FN = re.compile(r'(static void \w+\(void\)\n\{\nFN_ENTER\(0x[0-9a-f]+\);\n)(.*?)(\n\}\n)', re.S)
CLAMP = re.compile(r'VN\[(\d+)\]\.u\[(\d)\] = vu_u\(vu_in\(vu_f\(VF\[\1\]\.u\[\2\]\)\)\); ')
ACC_IN = re.compile(r'vu_in\(vu_f\(VACC\[(\d)\]\)\)')
LABEL = re.compile(r'^(L[0-9a-f]+):;$')
LABEL_ANY = re.compile(r'^L[0-9a-f]+:;')   # also a label this pass already gave a VH_CHK
BAD = re.compile(r'\b(?:S_\w+_)?P[0-9a-f]{6}\(\)|\bgen_call|\bguest_call|\bVUM0?\(|\bBREAK\(|\bSYSCALL|\brt_\w+\(|\bGUEST_STORE|'
                 r'\bSQ\w*\(|\bIRQ_POLL\(|\bgoto \*|\bswitch\b|\breturn\b|\bVCALLMS|\bVCTC2\(|\bCOP0W\(|\bJALR\(')
STORE = re.compile(r'\b(?:WR(?:8|16|32|64|F|128)|VSQC2|GS[DW][LR])\(')   # WR*S (stack) never reach the register space (sp_fast.py)
VF_W = re.compile(r'\bVF\[(\d+)\]\.u\[\d\] = |\bVLQC2\((\d+),|\bVQMTC2\(\d+, (\d+)\)')
ACC_W = re.compile(r'\bVACC\[(\d)\] = ([^;]*);')
UNCOND = re.compile(r'int c_ = \(r0 == r0\)|\breturn\b')

def hoist(body):
    lines = body.split('\n'); nloops = nclamps = 0
    labels = {m.group(1): i for i, l in enumerate(lines) for m in [LABEL.match(l)] if m}
    for lab, i in sorted(labels.items(), key=lambda t: -t[1]):
        refs = [j for j, l in enumerate(lines) if ('goto %s;' % lab) in l]
        if len(refs) != 1 or refs[0] <= i or i == 0 or UNCOND.search(lines[i - 1]) or 'vu_hoist' in lines[i - 1]: continue
        j = refs[0]; tail = lines[j]
        m = re.search(r'if \(c_\) (?:\{ IRQ_POLLF\(\((.*)\)\); )?goto %s;(?: \})+\s*$' % lab, tail)
        if not m: continue
        body_l = lines[i + 1:j + 1]
        if any(LABEL_ANY.match(l) for l in body_l[:-1]): continue
        text = '\n'.join(body_l)
        if BAD.search(text.replace(m.group(0), '')): continue
        written = {int(g) for w in VF_W.finditer(text) for g in w.groups() if g is not None}
        cl = sorted({(int(a), int(b)) for a, b in CLAMP.findall(text) if int(a) not in written})
        acc_ok = all('VU_MCL' in l for l in body_l if ACC_W.search(l)) and \
                 all(re.fullmatch(r't\d_', v.strip()) for k, v in ACC_W.findall(text))
        accs = sorted({int(k) for k in ACC_IN.findall(text)}) if acc_ok else []
        if not cl and not accs: continue
        sts = [t for t in range(i + 1, j) if STORE.search(lines[t])]
        stl = bool(sts) or bool(STORE.search(tail))
        hx = {x for x, k in cl}   # a store needs its own check only if a hoisted value is read after it in the iteration
        sts = [t for t in sts if any(int(v) in hx for l in lines[t + 1:j + 1] for v in re.findall(r'VN\[(\d+)\]', l)) or
               (accs and any('VACC[' in l for l in lines[t + 1:j + 1]))]
        n0 = sum(len(CLAMP.findall(l)) + len(ACC_IN.findall(l)) for l in body_l)
        if stl and (len(cl) + len(accs)) < 2 * (len(sts) + 1): continue   # a check costs about half a clamp
        pre = ''.join('VN[%d].u[%d] = vu_u(vu_in(vu_f(VF[%d].u[%d]))); ' % (x, k, x, k) for x, k in cl) + \
              ''.join('VACC[%d] = vu_u(vu_in(vu_f(VACC[%d]))); ' % (k, k) for k in accs)
        for t in range(i + 1, j + 1):
            l = CLAMP.sub(lambda c: '' if (int(c.group(1)), int(c.group(2))) in cl else c.group(0), lines[t])
            if accs: l = ACC_IN.sub(lambda c: 'vu_f(VACC[%s])' % c.group(1) if int(c.group(1)) in accs else c.group(0), l)
            lines[t] = l
        n1 = sum(len(CLAMP.findall(l)) + len(ACC_IN.findall(l)) for l in lines[i + 1:j + 1])
        after = '(' + pre.rstrip('; ').replace('; ', ', ') + ')'
        if stl:
            e = 'vh_' + lab
            for t in sts: lines[t] += ' VH_CHK(%s, %s);' % (e, after)
            lines[i] += ' VH_CHK(%s, %s);' % (e, after)
            lines.insert(i, '/* vu_hoist */ uint32_t %s = g_vu_ext; { %s}' % (e, pre))
        else:
            if m.group(1) is not None:
                lines[j] = lines[j].replace('IRQ_POLLF((%s));' % m.group(1), 'IRQ_POLLV((%s), %s);' % (m.group(1), after))
            lines.insert(i, '/* vu_hoist */ { %s}' % pre)
        nloops += 1; nclamps += n0 - n1
    return '\n'.join(lines), nloops, nclamps

for fn in sys.argv[1:]:
    s = open(fn).read(); tot = [0, 0]
    def one(m):
        b, a, c = hoist(m.group(2)); tot[0] += a; tot[1] += c
        return m.group(1) + b + m.group(3)
    t = FN.sub(one, s)
    if t != s: open(fn, 'w').write(t)
    print('%s: %d loops, %d clamps per iteration taken out' % (fn, tot[0], tot[1]))
