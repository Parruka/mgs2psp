#!/usr/bin/env python3
"""xunit.py <full gen_*.h> <unit: elf | stage name> <cov list for E> <cov_u.txt> <out .h>: free play's extra functions as a separate unit
(v25f, audit H1).  E = the unit's functions in the cov list but not in cov_u (the reduced build's).  Each E body is copied from the full
translation with its name prefixed X (P... -> XP..., S_<st>_P... -> XS_<st>_P...); its calls to other E functions (this unit's, or the
ELF's from a stage) use the X names, every other call keeps the main name (the reduced unit's function: kept, or a stub that reaches
rt_missing).  No gen_init / gen_call / ftab tails.  A sorted table {address, X function} for the PSP's missing-function hook (psp/xhook.c
x_call): the ELF's is gen_xtab / gen_xtab_n; a stage's is in the section .ovlx_hdr (magic XTB1, count, table), which psp/ovl_x.ld puts
right after the overlay's own end (__ovl_end), so the reduced overlay keeps its bytes and addresses."""
import re, sys
def cov(path):
    S = {}
    for ln in open(path):
        p = ln.split()
        if len(p) != 2: continue
        u, a = p; m = re.search(r'gen_s_(\w+)\.h$', u)
        u = m.group(1) if m else ('elf' if u in ('elf', 'gen_elf.h') or u.endswith('/gen_elf.h') else u)
        S.setdefault(u, set()).add(int(a, 16))
    return S
src, unit, covl, covu, out = sys.argv[1:6]
C = {}
for f in covl.split(','):
    for u, s in cov(f).items(): C.setdefault(u, set()).update(s)
U = cov(covu)
E = {u: C[u] - U.get(u, set()) for u in C}
Eu = E.get(unit, set()); Ee = E.get('elf', set())
pre = '' if unit == 'elf' else 'S_%s_' % unit
L = open(src).read().split('\n')
DEF = re.compile(r'^static void (\w+)\(void\)$')
DECL = re.compile(r'^static void (\w+)\(void\);$')
decls, bodies, got = [], [], set()
rx_stage = re.compile(r'\b%sP([0-9a-f]+)\(\)' % pre) if pre else None
rx_elf = re.compile(r'\bP([0-9a-f]{6})\(\)')
def ren(l):
    if rx_stage: l = rx_stage.sub(lambda m: ('X%sP%s()' if int(m.group(1), 16) in Eu else '%sP%s()') % (pre, m.group(1)), l)
    return rx_elf.sub(lambda m: ('XP%s()' if int(m.group(1), 16) in Ee else 'P%s()') % m.group(1), l)
i = 0
while i < len(L):
    l = L[i]
    if DECL.match(l) or l == 'static void JALR(uint32_t);': decls.append(l); i += 1; continue
    m = DEF.match(l)
    if m and i + 2 < len(L) and L[i + 1] == '{' and L[i + 2].startswith('FN_ENTER(0x'):
        a = int(re.match(r'FN_ENTER\(0x([0-9a-f]+)\)', L[i + 2]).group(1), 16); j = i + 1
        while L[j] != '}': j += 1
        if a in Eu:
            assert m.group(1) == '%sP%06x' % (pre, a) or m.group(1) == '%sP%x' % (pre, a), m.group(1)
            bodies.append('static void X%s(void)' % m.group(1)); bodies += [ren(x) for x in L[i + 1:j + 1]]; got.add(a)
        i = j + 1; continue
    i += 1
miss = Eu - got
if miss: sys.exit('xunit: %d E functions of %s not in %s: %s' % (len(miss), unit, src, ' '.join('%x' % a for a in sorted(miss)[:10])))
name = lambda a: 'X%sP%06x' % (pre, a)
o = ['/* tools/xunit.py: %s, %d extra functions (E = %s - %s) */' % (unit, len(Eu), covl, covu)]
o += decls
o += ['static void %s(void);' % name(a) for a in sorted(Eu)]
if unit != 'elf': o += ['void XP%06x(void);' % a for a in sorted(Ee)]   # (the ELF's extra functions, in the main ELF's gen_x.o)
o += bodies
tab = ['  { 0x%xu, %s },' % (a, name(a)) for a in sorted(Eu)]
if unit == 'elf':
    o += ['const struct GenFn gen_xtab[] = {'] + tab + ['};', 'const int gen_xtab_n = %d;' % len(Eu)]
else:
    o += ['static const struct GenFn xtab_%s[] = {' % unit] + tab + ['};',
          'struct OvlX { uint32_t magic; int n; const struct GenFn *tab; };',
          'const struct OvlX ovlx_hdr_%s __attribute__((section(".ovlx_hdr"), used)) = { 0x31425458u, %d, xtab_%s };' % (unit, len(Eu), unit)]
open(out, 'w').write('\n'.join(o) + '\n')
print('%s: %d extra functions (%d body lines), %d ELF extras declared' % (unit, len(Eu), len(bodies), len(Ee) if unit != 'elf' else 0))
