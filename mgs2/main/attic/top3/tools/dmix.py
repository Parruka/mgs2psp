#!/usr/bin/env python3
"""dmix.py PROF DIS1 DIS2: generated code (P..., S_w00a_P...) executed instructions and misses by host mnemonic class; D-misses by
load / store and by base register (sp = GCC stack; others); per step (80)."""
import sys, re, collections
prof, *dis = sys.argv[1:]
M = {}; F = {}
cur = None
for d in dis:
    for l in open(d):
        m = re.match(r'^([0-9a-f]{8}) <([^>]+)>:', l)
        if m: cur = m.group(2); continue
        m = re.match(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)', l)
        if m: pc = int(m.group(1), 16); M[pc] = (m.group(2), m.group(3)); F[pc] = cur
gen = re.compile(r'^(S_\w+?_)?P[0-9a-f]{6}')
C = collections.defaultdict(lambda: [0, 0, 0]); B = collections.defaultdict(lambda: [0, 0, 0])
for l in open(prof):
    if l.startswith('#'): continue
    f = l.split()
    if len(f) < 4: continue
    pc = int(f[0], 16)
    if pc not in F or not F[pc] or not gen.match(F[pc]): continue
    mn, ops = M[pc]; c, im, dm = int(f[1]), int(f[2]), int(f[3])
    k = mn
    if mn in ('lw','lh','lhu','lb','lbu','lwc1','lv.q','lv.s','lwl','lwr'): k = 'LOAD ' + mn
    if mn in ('sw','sh','sb','swc1','sv.q','sv.s','swl','swr'): k = 'STORE ' + mn
    x = C[k]; x[0] += c; x[1] += im; x[2] += dm
    m = re.search(r'\((\w+)\)', ops)
    if k.startswith('LOAD') or k.startswith('STORE'):
        base = m.group(1) if m else '?'
        y = B[(k.split()[0], 'sp' if base == 'sp' else 'other')]; y[0] += c; y[2] += dm
ms = lambda v: (1.65 * v[0] + 68 * v[1] + 86 * v[2]) / 333000 / 80
T = [sum(v[i] for v in C.values()) for i in range(3)]
print('generated code: %.0f instr, %.0f I, %.0f D a step = %.2f ms' % (T[0]/80, T[1]/80, T[2]/80, ms(T)))
for k, v in sorted(C.items(), key=lambda kv: -kv[1][0])[:40]:
    print('  %-14s %9.0f instr %5.1f%%  D %7.0f  I %6.0f  %.2f ms' % (k, v[0]/80, 100.0*v[0]/T[0], v[2]/80, v[1]/80, ms(v)))
for k, v in sorted(B.items()): print('  %-6s base %-5s instr %9.0f  D-miss %7.0f a step' % (k[0], k[1], v[0]/80, v[2]/80))
