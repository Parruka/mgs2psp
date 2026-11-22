#!/usr/bin/env python3
"""ldu_fuse.py <in dir> <out dir>: header post-pass on gen_*.h.  An LDL a+7 / LDR a pair on one register (in either order, nothing between
them, the base another register) leaves that register holding the 8 bytes at a whatever it held before, and an SDL a+7 / SDR a pair writes
the 8 bytes at a.  Each such pair becomes one call: GLDU / GSDU (runtime/mmi.h, defined in boot/gen_dispatch.c) load or store the 8
bytes with one unaligned host access when both ends are plain RAM, else run the pair as written, in its order.  The second line keeps
its address comment as an empty statement, so no line moves.  A call in place of two calls adds no block, so the PGO profile still
matches (the helpers are called out of line in the profiled builds).  Prints the pairs fused per header."""
import re, sys, os, glob
LD = re.compile(r'^(/\*[0-9a-f]+\*/) (r\d+) = (GLDL|GLDR)\(\2, \(uint32_t\)\((r\d+) \+ (-?\d+)\)\);$')
ST = re.compile(r'^(/\*[0-9a-f]+\*/) (GSDL|GSDR)\((r\d+), \(uint32_t\)\((r\d+) \+ (-?\d+)\)\);$')

def pair(a, b, opl):
    """(low offset, LDR first?) when lines a, b are the two halves of one 8-byte access, else None"""
    if a.group(3 if opl else 2) == b.group(3 if opl else 2): return None
    o1, o2 = int(a.group(5)), int(b.group(5))
    first_l = a.group(3 if opl else 2).endswith('L')
    if first_l and o1 == o2 + 7: return o2, 0
    if not first_l and o2 == o1 + 7: return o1, 1
    return None

def fuse(text):
    L = text.split('\n'); nl = ns = 0; i = 0
    while i < len(L) - 1:
        a, b = LD.match(L[i]), LD.match(L[i + 1])
        if a and b and a.group(2) == b.group(2) and a.group(4) == b.group(4) and a.group(2) != a.group(4):
            p = pair(a, b, True)
            if p:
                L[i] = '%s %s = GLDU(%s, (uint32_t)(%s + %d), %d);' % (a.group(1), a.group(2), a.group(2), a.group(4), p[0], p[1])
                L[i + 1] = b.group(1) + ' ;'; nl += 1; i += 2; continue
        a, b = ST.match(L[i]), ST.match(L[i + 1])
        if a and b and a.group(3) == b.group(3) and a.group(4) == b.group(4):
            p = pair(a, b, False)
            if p:
                L[i] = '%s GSDU(%s, (uint32_t)(%s + %d), %d);' % (a.group(1), a.group(3), a.group(4), p[0], p[1])
                L[i + 1] = b.group(1) + ' ;'; ns += 1; i += 2; continue
        i += 1
    return '\n'.join(L), nl, ns

def main():
    src, dst = sys.argv[1:3]
    os.makedirs(dst, exist_ok=True)
    for f in sorted(glob.glob(os.path.join(src, 'gen_*.h'))):
        out, nl, ns = fuse(open(f).read())
        open(os.path.join(dst, os.path.basename(f)), 'w').write(out)
        print('%s: %d load pairs, %d store pairs' % (os.path.basename(f), nl, ns))

if __name__ == '__main__': main()
