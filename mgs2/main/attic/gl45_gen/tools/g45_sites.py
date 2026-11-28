#!/usr/bin/env python3
"""gl45 batch 3: what the levers' sites execute a step in the hot functions (twin counters on the split base headers): the
ceiling each lever has before the compiler's own side effects.  usage: g45_sites.py <counts.json> <steps> <rep_a_elf.json> <rep_a_w00a.json>
<split header> <map.json> [<split header> <map.json> ...]"""
import re, sys, json
D = json.load(open(sys.argv[1])); steps = float(sys.argv[2]); REP = {}
for f in sys.argv[3:5]: REP.update(json.load(open(f)))
pairs = sys.argv[5:]
M1 = "RD8B|RD16B|RD32B|RD64B|RDFB|WR8B|WR16B|WR32B|WR64B|WRFB"; M2 = "GLDUB|GSDUB|VLQC2B|VSQC2B"; ST = "RD8S|RD16S|RD32S|RD64S|RDFS|WR8S|WR16S|WR32S|WR64S|WRFS"
ACC = re.compile(r"\b(?:%s)\((r\d+),|\b(?:%s)\([^,()]+, (r\d+)," % (M1, M2)); STK = re.compile(r"\b(?:%s)\(" % ST)
CALL = re.compile(r"\b(?:S_\w+?_)?P[0-9a-f]{6}\(\)|\b(?:JALR|JSIF|SYSCALL)\(")
t = dict(lines=0.0, polls=0.0, stack=0.0, spasg=0.0, acc=0.0, accg=0.0, calls=0.0, callsout=0.0, ret=0.0, pollfn=0.0)
for k in range(0, len(pairs), 2):
    L = open(pairs[k]).read().split("\n"); M = json.load(open(pairs[k + 1])); LN = {int(a): b for a, b in M["lines"].items()}
    for fn, (a, b) in M["fn"].items():
        g = REP.get(fn, {}).get("a_guards", {}); haspoll = any("IRQ_POLLF" in L[n - 1] for n in range(a, b + 1)); first = True
        for n in range(a, b + 1):
            if n not in LN: continue
            w = D[LN[n]] / steps; x = L[n - 1]
            if x.startswith("/*"): t["lines"] += w
            t["polls"] += w * x.count("IRQ_POLLF"); t["stack"] += w * len(STK.findall(x)); t["spasg"] += w * len(re.findall(r"\br29 = ", x))
            for m in ACC.finditer(x):
                r = m.group(1) or m.group(2); t["acc"] += w
                if r in g: t["accg"] += w
            c = len(CALL.findall(x)); t["callsout"] += w * c
            if haspoll: t["pollfn"] += w * (2 * c + x.count("return;"))
        t["calls"] += D[LN[a + 2]] / steps if a + 2 in LN else 0.0
print("hot functions, a step: guest lines %.0f; calls in %.0f, calls out %.0f" % (t["lines"], t["calls"], t["callsout"]))
print("(c) polls executed %.0f: ceiling 2 slots each = %.0f; the function-wide form pays about %.0f back at the calls and returns of the functions that poll" % (t["polls"], 2 * t["polls"], t["pollfn"]))
print("(b1) stack accesses executed %.0f, $sp assignments %.0f: ceiling 1 slot each = %.0f" % (t["stack"], t["spasg"], t["stack"] - t["spasg"]))
print("(b2) base-register accesses executed %.0f (tools/g45_b2sel.py prints the gain through the chosen registers)" % t["acc"])
print("(a) of them through a guarded base %.0f: ceiling 2 slots each = %.0f" % (t["accg"], 2 * t["accg"]))
