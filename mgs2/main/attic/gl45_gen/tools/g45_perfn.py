#!/usr/bin/env python3
"""gl45 batch 3: g45_perfn.py <out dir> <out.tsv>: one row a hot function from tools/g45_size.py's results: calls a step, PSP
slots a step in today's code, with the macro layer only, with each lever alone, with all three, in mix 1 (each lever where it pays
alone) and in the chosen mix; the bytes of
its section today and of its untouched copy under lever (a); the levers the chosen mix (sel_final.json) keeps in it."""
import json, sys
d, outf = sys.argv[1], sys.argv[2]
def load(v):
    r = {}
    for u in ("elf", "w00a"): r.update(json.load(open("%s/size_%s_%s.json" % (d, v, u))))
    return r
vs = ["base", "baseL", "c", "b1", "b2s", "a", "abc", "sel", "mix"]; V = {v: load(v) for v in vs}; S = json.load(open(d + "/sel_final.json"))
rows = ["function\tunit\tcalls\ttoday\tmacro_layer\tc\tb1\tb2_chosen\ta\tabc\tmix1\tmix\tbytes_today\tbytes_copy_a\tmix_levers"]
for fn, x in sorted(V["base"].items(), key=lambda kv: -kv[1]["slots"]):
    s = S.get(fn, {}); lv = "".join(k for k in ("c", "b1", "a") if s.get(k)) + (("+b2:all" if s["b2"] == "all" else "+b2:%d" % len(s["b2"])) if s.get("b2") else "")
    rows.append("%s\t%s\t%.2f\t" % (fn, "deck" if fn.startswith("S_") else "ELF", x["calls"]) + "\t".join("%.0f" % V[v][fn]["slots"] for v in vs)
                + "\t%d\t%d\t%s" % (x["bytes"], V["a"][fn]["bytes_s"], lv or "-"))
open(outf, "w").write("\n".join(rows) + "\n"); print("g45_perfn: %d rows" % (len(rows) - 1))
