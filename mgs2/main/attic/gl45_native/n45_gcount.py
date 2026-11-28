#!/usr/bin/env python3
"""gl45 batch 2: guest instructions a call of the counted function over a window of the twin's run (n45bc lines of tools/n45_bc.py's build).
usage: n45_gcount.py <counted gen_elf.h> <function> <out.log> <vb from> <vb to> [<loop from hex> <loop to hex>]"""
import re, sys
hdr, fn, log, lo, hi = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
la, lb = (int(sys.argv[6], 16), int(sys.argv[7], 16)) if len(sys.argv) > 7 else (0, 0)
rows = {}
for l in open(log, errors="replace"):
    if l.startswith("n45bc "):
        p = l.split(); rows[int(p[1])] = [int(x) for x in p[2:]]
d = [b - a for a, b in zip(rows[lo], rows[hi])]
src = open(hdr).read().split("\n")
i0 = next(i for i, l in enumerate(src) if re.fullmatch(r"static void %s(_tr)?\(void\)" % fn, l) and src[i + 1].startswith("{"))
e = next(j for j in range(i0 + 1, len(src)) if src[j] == "}")
body = "\n".join(src[i0:e])
tot = 0.0; loop = 0.0
for m in re.finditer(r"/\*([0-9a-f]{6})\*/ N45_BC\((\d+)\);(.*?)(?=/\*[0-9a-f]{6}\*/ N45_BC|$)", body, re.S):
    a = m.group(1); k = int(m.group(2)); t = re.sub(r"^L[0-9a-f_i]+:;\s*", "", m.group(3).strip()); n = d[k]
    if t.startswith("{ int c_") or t.startswith("{ r31 = 0x") or "return;" in t: g = 2 * n            # a branch / call / return and its delay slot
    elif t.startswith("{ if ("):                                                                        # a likely branch: the slot runs when taken
        gk = re.search(r"N45_BC\((\d+)\); goto", t); g = n + (d[int(gk.group(1))] if gk else 0)
    else: g = n
    tot += g
    if la <= int(a, 16) <= lb: loop += g
print("%s, vb %d-%d: %d calls (%.2f a step), %.1f guest instructions a call" % (fn, lo, hi, d[0], d[0] / (hi - lo), tot / d[0]))
if lb: print("  of them at %x-%x: %.1f a call (%.1f%%)" % (la, lb, loop / d[0], 100 * loop / tot))
