#!/usr/bin/env python3
"""gl45 batch 3: which base registers lever (b2) should take, from the twin's counters (a speed choice only).
usage: g45_b2sel.py <split base header> <map.json> ... <counts.json> <steps> <out selection.json>
A register is chosen in a function when its accesses executed (base-register forms) are more than RATIO times its assignments
executed (each assignment costs the one add that each access saves) and at least MINA a step."""
import re, sys, json
RATIO = 1.25; MINA = 5.0
args = sys.argv[1:]; outf = args[-1]; steps = float(args[-2]); D = json.load(open(args[-3])); pairs = args[:-3]
M1 = "RD8B|RD16B|RD32B|RD64B|RDFB|WR8B|WR16B|WR32B|WR64B|WRFB"; M2 = "GLDUB|GSDUB|VLQC2B|VSQC2B"
ACC = re.compile(r"\b(?:%s)\((r\d+),|\b(?:%s)\([^,()]+, (r\d+)," % (M1, M2)); ASG = re.compile(r"\b(r\d+) = |\bRC(?:MV|LD)\((r\d+),")
sel = {}; tot = [0.0, 0.0, 0.0]
for k in range(0, len(pairs), 2):
    L = open(pairs[k]).read().split("\n"); M = json.load(open(pairs[k + 1])); LN = {int(a): b for a, b in M["lines"].items()}
    for fn, (a, b) in M["fn"].items():
        acc = {}; asg = {}
        for n in range(a, b + 1):
            if n not in LN: continue
            w = D[LN[n]] / steps; t = L[n - 1]
            for m in ACC.finditer(t): r = m.group(1) or m.group(2); acc[r] = acc.get(r, 0.0) + w
            for m in ASG.finditer(t): r = m.group(1) or m.group(2); asg[r] = asg.get(r, 0.0) + w
        regs = sorted(r for r in acc if r != "r0" and acc[r] >= MINA and acc[r] > RATIO * asg.get(r, 0.0))
        tot[0] += sum(acc.values()); tot[1] += sum(acc[r] for r in regs); tot[2] += sum(asg.get(r, 0.0) for r in regs)
        if regs: sel[fn] = {"b2": regs}
json.dump(sel, open(outf, "w"), indent=0)
print("g45_b2sel: %d functions, %d registers; base-register accesses a step %.0f, of them through chosen registers %.0f (their assignments %.0f): about %.0f slots a step to gain" % (
    len(sel), sum(len(s["b2"]) for s in sel.values()), tot[0], tot[1], tot[2], tot[1] - tot[2]))
