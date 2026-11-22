#!/usr/bin/env python3
"""nchk_strip.py <header in> <header out> (dev): a hybrid header made with RC_NCHK=1, the NARROW check (guest.h GEN_NCHK) kept only in the
functions v27 makes 32-bit (/home/claude/lean/w3/wpw_v27x.json: no wide register): in v27's 64-bit functions the proved widths let a
32-bit local take a register whose upper word is never needed, so NCHK / RCLD / RCMV there become the unchecked forms."""
import sys, re, json
V = json.load(open('/home/claude/lean/w3/wpw_v27x.json'))['wr']
src = open(sys.argv[1]).read().split('\n'); out = ['#define RCLD_(l, n, e) ((void)(R[n] = (e)), (void)((l) = (int32_t)R[n]))',
      '#define RCMV_(l, n, src) ((void)(R[n] = R[src]), (void)((l) = (int32_t)R[n]))']
cur = None; k = 0; nf = 0
FN = re.compile(r'^static void (\w+)\(void\)$')
for l in src:
    m = FN.match(l)
    if m: cur = m.group(1) if V.get(m.group(1)) else None; nf += cur is not None
    elif l == '}': cur = None
    elif cur:
        l2 = re.sub(r'\bNCHK\((\d+), 0x[0-9a-f]+\)', r'(int32_t)R[\1]', l)
        l2 = re.sub(r'\bRCLD\(', 'RCLD_(', l2); l2 = re.sub(r'\bRCMV\(', 'RCMV_(', l2)
        if l2 != l: k += 1
        l = l2
    out.append(l)
open(sys.argv[2], 'w').write('\n'.join(out))
print('%s: %d functions unchecked, %d lines changed' % (sys.argv[1], nf, k))
