#!/usr/bin/env python3
"""gl45 batch 3: execution counters in the hot functions of a generated header (x86 twin), for the sizing's weights.
usage: g45_count.py <in.h> <out.h> <hot list (callgrind self listing)> <n hot> <first counter index> <keys.json (appended to)>
Every guest instruction comment /*addr*/ of a hot function gets G45_BC(k) (times reached), every goto G45_BC(k) (times taken).
keys.json: {function: [first index, [[kind, addr, target], ...]]}."""
import re, sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glib
src, dst, hotf, nhot, k0, keysf = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
L, fns = glib.load(src); hot = [h for h, _ in glib.hot(hotf, nhot) if h in fns]
keys = json.load(open(keysf)) if os.path.exists(keysf) else {}
k = k0
for name in hot:
    a, b = fns[name]; ks = []; first = k
    def r(m):
        global k
        k += 1
        if m.group(1): ks.append(["at", m.group(1), ""]); return "/*%s*/ G45_BC(%d);" % (m.group(1), k - 1)
        ks.append(["goto", "", m.group(2)]); return "{ G45_BC(%d); goto %s; }" % (k - 1, m.group(2))
    for j in range(a + 2, b): L[j] = re.sub(r"/\*([0-9a-f]{6})\*/|goto (L[0-9a-zA-Z_]+);", r, L[j])
    keys[name] = [first, ks]
L.insert(0, "#define G45_BC(k) ((void)g45_bc[k]++)   /* tools/g45_count.py */"); L.insert(0, "extern long g45_bc[];")
open(dst, "w").write("\n".join(L)); json.dump(keys, open(keysf, "w"))
print("g45_count: %s: %d functions, counters %d..%d" % (dst, len(hot), k0, k - 1))
