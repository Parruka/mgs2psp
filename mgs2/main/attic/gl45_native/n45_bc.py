#!/usr/bin/env python3
"""gl45 study, batch 2: execution counters in one translated function, for a weighted instruction count.
usage: python3 -I n45_bc.py <hooked gen_elf.h> <out gen_elf.h> <function, e.g. P14b008> <map.json>
Every guest instruction comment /*addr*/ in the function gets N45_BC(k) (times reached), every goto gets N45_BC(k) (times taken)."""
import re, sys, json
src, dst, fn, mp = sys.argv[1:5]
L = open(src).read().split("\n")
at = [i for i, l in enumerate(L) if re.fullmatch(r"static void %s(_tr)?\(void\)" % fn, l) and L[i + 1].startswith("{")]
assert len(at) == 1, at
i0 = at[0]; e = next(j for j in range(i0 + 1, len(L)) if L[j] == "}")
keys = []
def sub(line, ln):
    def r(m):
        k = len(keys)
        if m.group(1): keys.append(("at", m.group(1), ln, "")); return "/*%s*/ N45_BC(%d);" % (m.group(1), k)
        keys.append(("goto", "", ln, m.group(2))); return "{ N45_BC(%d); goto %s; }" % (k, m.group(2))
    return re.sub(r"/\*([0-9a-f]{6})\*/|goto (L[0-9a-f_i]+);", r, line)
for j in range(i0 + 2, e):
    L[j] = sub(L[j], j - i0 + 1)
# a goto key belongs to the last 'at' key before it on the same line
cur = ""
K = []
for kind, a, ln, t in keys:
    if kind == "at": cur = a
    K.append({"kind": kind, "addr": cur, "line": ln, "target": t})
open(dst, "w").write("\n".join(L)); json.dump(K, open(mp, "w"))
print("n45_bc: %s: %d counters in %s (lines %d..%d)" % (dst, len(K), fn, i0 + 1, e + 1))
