#!/usr/bin/env python3
"""gl45 batch 3: the sizing's tables from tools/g45_size.py's results out/size_<variant>_<unit>.json.
usage: g45_table.py <out dir> <variant> ... [-base <variant to compare with, default base>] [-sel <selection.json to write>] [-top n]
For each variant against base: PSP slots a step of the hot functions (both units), the functions it lowers / raises, and what
the variant would give if it were applied only where it lowers the count ("where it pays").  -sel writes tools/gen_levers.py's
selection from the single-lever variants c, b1, b2s (tools/g45_b2sel.py's registers), a: a lever is chosen for a function when
it lowers that function's count by at least PAY (a share) and MINS slots a step."""
import json, sys
PAY = 0.003; MINS = 20.0
args = sys.argv[1:]; d = args[0]; vs = []; selout = None; top = 0; i = 1; bname = "base"
while i < len(args):
    if args[i] == "-sel": selout = args[i + 1]; i += 2
    elif args[i] == "-base": bname = args[i + 1]; i += 2
    elif args[i] == "-top": top = int(args[i + 1]); i += 2
    else: vs.append(args[i]); i += 1
def load(v):
    r = {}
    for u in ("elf", "w00a"): r.update(json.load(open("%s/size_%s_%s.json" % (d, v, u))))
    return r
B = load(bname); tb = sum(x["slots"] for x in B.values())
print("%s: %d functions, %.0f PSP slots a step (ELF unit %.0f, deck unit %.0f)" % (bname, len(B), tb, sum(x["slots"] for k, x in B.items() if not k.startswith("S_")), sum(x["slots"] for k, x in B.items() if k.startswith("S_"))))
V = {v: load(v) for v in vs}
def pays(v, fn): return fn in V[v] and V[v][fn]["slots"] < B[fn]["slots"] * (1 - PAY) and B[fn]["slots"] - V[v][fn]["slots"] >= MINS
for v in vs:
    t = sum(x["slots"] for x in V[v].values()); lo = [f for f in B if pays(v, f)]
    hi = [f for f in B if f in V[v] and V[v][f]["slots"] > B[f]["slots"] * (1 + PAY) and V[v][f]["slots"] - B[f]["slots"] >= MINS]
    gain = sum(B[f]["slots"] - V[v][f]["slots"] for f in lo); loss = sum(V[v][f]["slots"] - B[f]["slots"] for f in hi)
    hb = sum(V[v][f]["bytes"] - B[f]["bytes"] for f in B if f in V[v]); cb = sum(V[v][f]["bytes_s"] for f in V[v])
    pb = sum(V[v][f]["bytes"] - B[f]["bytes"] for f in lo); pc = sum(V[v][f]["bytes_s"] for f in lo)
    print("%-5s everywhere: %9.0f slots a step (%+8.0f, %+5.2f%%); lowers %3d functions by %7.0f, raises %3d by %6.0f; the 200's code %+7d bytes, untouched copies %7d"
          % (v, t, t - tb, 100 * (t - tb) / tb, len(lo), gain, len(hi), loss, hb, cb))
    print("      where it pays: %+8.0f slots a step (%+5.2f%%) in %3d functions; their code %+7d bytes, untouched copies %7d" % (-gain, -100 * gain / tb, len(lo), pb, pc))
if top:
    rows = sorted(B.items(), key=lambda kv: -kv[1]["slots"])[:top]
    print("%-18s %9s %8s " % ("function", "base", "calls") + " ".join("%8s" % v for v in vs))
    for fn, x in rows: print("%-18s %9.0f %8.2f " % (fn, x["slots"], x["calls"]) + " ".join("%+8.0f" % (V[v][fn]["slots"] - x["slots"]) if fn in V[v] else "       -" for v in vs))
if selout:
    import os
    B2 = json.load(open(d + "/sel_b2s.json")) if os.path.exists(d + "/sel_b2s.json") else {}       # tools/g45_b2sel.py's registers
    sel = {}
    for fn in B:
        s = {"c": int(pays("c", fn)), "b1": int(pays("b1", fn)), "b2": B2.get(fn, {}).get("b2", []) if "b2s" in V and pays("b2s", fn) else [], "a": int(pays("a", fn))}
        if s["c"] or s["b1"] or s["b2"] or s["a"]: sel[fn] = s
    json.dump(sel, open(selout, "w"), indent=0)
    print("selection %s: %d functions: c %d, b1 %d, b2 %d, a %d" % (selout, len(sel), sum(s["c"] for s in sel.values()), sum(s["b1"] for s in sel.values()), sum(1 for s in sel.values() if s["b2"]), sum(s["a"] for s in sel.values())))
