#!/usr/bin/env python3
"""mm_base.py <gen header> [...]: rewrites the guest accesses of generated code through a register base + a constant offset (in place) to
the gen_pre.h base forms RD<n>B / WR<n>B / RDFB / WRFB (base, k) (recomp/lean_plan.md step 3).  The I/O test and the host address are then
taken from the base register, not from each address: the compiler shares both between the accesses through one register (one test per
base, often one for a whole run of accesses) and k becomes the host load / store offset.
  - plain loads and stores: recomp.py `(uint32_t)(rN + k)` / `(uint32_t)(R[N] + k)`; exact while base and base + k lie in the same 256 MB
    segment and the same 32 MB window, as every access of a valid pointer to a field does (x86 GEN_MB_CHECK reports any access that breaks
    it).  $zero (a constant address: the plain form folds) and $sp (tools/sp_fast.py's forms) are left alone.
  - the two halves of lq / sq: base = the 16-byte-aligned address, k = 0 / 8 (exact for every address).
  - tools/ldu_fuse.py's GLDU / GSDU (an 8-byte unaligned pair) through a base register: GLDUB / GSDUB, inline (the invariant for k and
    k + 7); the out-of-line helpers stay the slow path.
  - lqc2 / sqc2 (VLQC2 / VSQC2, `((uint32_t)(rN + k)) & ~15u`) with k a multiple of 16: VLQC2B / VSQC2B (ft, rN & ~15, k), $sp included
    (the invariant for k and k + 15).
Run after sp_fast.py (and the passes that read the plain forms).  Idempotent; prints the number of sites rewritten."""
import sys, re
REG = r'(r(?:[1-9]|1\d|2[0-8]|3[01])|R\[(?:[1-9]|1\d|2[0-8]|3[01])\])'
LD = re.compile(r'\b(RD8|RD16|RD32|RD64|RDF)\(\(uint32_t\)\(' + REG + r' \+ (-?\d+)\)\)')
ST = re.compile(r'\b(WR8|WR16|WR32|WR64|WRF)\(\(uint32_t\)\(' + REG + r' \+ (-?\d+)\), ')
LQ0 = re.compile(r'\bRD64\(qa_\)')
LQ8 = re.compile(r'\bRD64\(qa_ \+ 8\)')
SQ0 = re.compile(r'\bWR64\((\(\(uint32_t\)\((?:r\d+|R\[\d+\]) \+ -?\d+\)\) & ~15u), ')
SQ8 = re.compile(r'\bWR64\(\((\(\(uint32_t\)\((?:r\d+|R\[\d+\]) \+ -?\d+\)\) & ~15u)\) \+ 8, ')
LDU = re.compile(r'\bGLDU\((r\d+), \(uint32_t\)\(' + REG + r' \+ (-?\d+)\), ([01])\)')
SDU = re.compile(r'\bGSDU\((r\d+), \(uint32_t\)\(' + REG + r' \+ (-?\d+)\), ([01])\)')
VQ = re.compile(r'\b(VLQC2|VSQC2)\((\d+), \(\(uint32_t\)\((r(?:[1-9]|[12]\d|3[01])|R\[(?:[1-9]|[12]\d|3[01])\]) \+ (-?\d+)\)\) & ~15u\)')
for fn in sys.argv[1:]:
    s = open(fn).read()
    t, a = LD.subn(lambda m: '%sB(%s, %s)' % m.groups(), s)
    t, b = ST.subn(lambda m: '%sB(%s, %s, ' % m.groups(), t)
    t, c0 = LQ0.subn('RD64B(qa_, 0)', t)
    t, c8 = LQ8.subn('RD64B(qa_, 8)', t)
    t, d0 = SQ0.subn(lambda m: 'WR64B(%s, 0, ' % m.group(1), t)
    t, d8 = SQ8.subn(lambda m: 'WR64B(%s, 8, ' % m.group(1), t)
    t, e = LDU.subn(lambda m: 'GLDUB(%s, %s, %s, %s)' % m.groups(), t)
    t, f = SDU.subn(lambda m: 'GSDUB(%s, %s, %s, %s)' % m.groups(), t)
    t, g = VQ.subn(lambda m: '%sB(%s, (uint32_t)(%s) & ~15u, %s)' % m.groups() if int(m.group(4)) % 16 == 0 else m.group(0), t)
    if t != s: open(fn, 'w').write(t)
    print('%s: %d loads, %d stores, %d + %d lq halves, %d + %d sq halves, %d GLDU, %d GSDU, %d lqc2 / sqc2 rewritten'
          % (fn, a, b, c0, c8, d0, d8, e, f, len(re.findall(r'\bV[LS]QC2B\(', t)) - len(re.findall(r'\bV[LS]QC2B\(', s))))
