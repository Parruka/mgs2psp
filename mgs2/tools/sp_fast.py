#!/usr/bin/env python3
"""sp_fast.py <gen header> [...]: rewrites the guest stack accesses of generated code (in place) to the gen_pre.h stack forms
RD<n>S / WR<n>S / RDFS / WRFS (sp, offset): an address $sp + k (recomp.py `(uint32_t)(r29 + k)` / `(uint32_t)(R[29] + k)`, the plain
loads and stores; the 16-byte-masked lq / sq forms stay) never lies in the register space and a frame never crosses the ends of the
32 MB of RAM, so the access needs neither the I/O test nor the mask of each address: the host base of $sp is computed once while $sp
is unchanged and k becomes the offset of the host load / store.  Exact under that invariant (x86 GEN_SP_CHECK reports any access that
breaks it).  Idempotent; prints the number of sites rewritten."""
import sys, re
LD = re.compile(r'\b(RD8|RD16|RD32|RD64|RDF)\(\(uint32_t\)\((r29|R\[29\]) \+ (-?\d+)\)\)')
ST = re.compile(r'\b(WR8|WR16|WR32|WR64|WRF)\(\(uint32_t\)\((r29|R\[29\]) \+ (-?\d+)\), ')
for fn in sys.argv[1:]:
    s = open(fn).read()
    t, a = LD.subn(lambda m: '%sS(%s, %s)' % m.groups(), s)
    t, b = ST.subn(lambda m: '%sS(%s, %s, ' % m.groups(), t)
    if t != s: open(fn, 'w').write(t)
    print('%s: %d stack loads, %d stack stores rewritten' % (fn, a, b))
