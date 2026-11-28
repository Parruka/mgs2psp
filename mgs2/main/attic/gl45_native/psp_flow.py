#!/usr/bin/env python3
"""gl45 study, batch 2: weighted PSP instruction slots of one function, by class.
usage: psp_flow.py <objdump -dlr listing of the function (debug object)> <spec> [-l]
spec: lines `<hex offset> <probability the conditional branch is taken>` (# comments).  Flow 1.0 enters at offset 0 and is pushed through
the code: a conditional branch splits it by its probability (a likely branch's delay slot runs only when taken), calls are not followed,
`jr ra` ends.  Region-test branches (after srl ..,0x1c) and poll branches (after addiu ..,-64) default to 0; any other conditional with
flow and no spec is listed.  Output: slots a call by class; -l lists every instruction with its flow and class."""
import re, sys
lst, specf = sys.argv[1], sys.argv[2]; listing = '-l' in sys.argv
recs = {}; order = []; cur = None; fn = None
for l in open(lst):
    l = l.rstrip("\n")
    m = re.match(r"^(/.*?):(\d+)( \(discriminator \d+\))?$", l)
    if m: cur = (m.group(1).split("/")[-1], int(m.group(2))); continue
    m = re.match(r"^([A-Za-z_0-9]+)\(\):$", l)
    if m: fn = m.group(1); continue
    m = re.match(r"^\s+([0-9a-f]+):\t(\S+)\s*(.*)$", l)
    if m: a = int(m.group(1), 16); recs[a] = dict(op=m.group(2), args=m.group(3), line=cur, fn=fn, rel=""); order.append(a); continue
    m = re.match(r"^\s+[0-9a-f]+: (R_MIPS_\S+)\s+(\S+)", l)
    if m and order: recs[order[-1]]["rel"] = m.group(2)
spec = {}
for l in open(specf):
    l = l.split('#')[0].split()
    if len(l) >= 2: spec[int(l[0], 16)] = float(l[1])
def tgt(a): return int(re.search(r'([0-9a-f]+) <', recs[a]["args"]).group(1), 16)
def iscond(op): return op[0] == 'b' and op not in ('b', 'break', 'bal')
def islikely(op): return op.endswith('l') and op not in ('bal',)
# static tags: region tags (srl x,y,0x1c and their host-stack copies), poll values
tagreg = set(); tagslot = set(); cls = {}
FRAME = None
def regs(args): return re.findall(r'\b(zero|at|v[01]|a[0-3]|t[0-9]|s[0-8]|k[01]|gp|sp|ra)\b', args)
for i, a in enumerate(order):
    r = recs[a]; op, args = r["op"], r["args"]; rg = regs(args); c = None
    prev = recs[order[i - 1]] if i else None
    m = re.match(r'(\w+),(-?\d+)\((\w+)\)', args)
    if r["rel"] == "k_left" or (op == 'addiu' and args.endswith(',-64') and rg[0] == rg[1]): c = 'poll'
    elif op in ('blez', 'bgtz', 'blezl', 'bgtzl') and prev and prev["op"] == 'addiu' and prev["args"].endswith(',-64'): c = 'poll'
    elif op in ('blez', 'bgtz') and i > 1 and recs[order[i - 2]]["op"] == 'addiu' and recs[order[i - 2]]["args"].endswith(',-64'): c = 'poll'
    elif r["rel"] == "rt_poll_slow": c = 'poll'
    elif (op == 'ext' and args.endswith(',0x0,0x19')) or (op == 'addu' and 's7' in rg[1:]): c = 'addr'
    elif op == 'srl' and args.endswith(',0x1c'): c = 'addr'; tagreg.add(rg[0])
    elif op == 'sra' and args.endswith(',0x1f'): c = 'half64'
    elif r["rel"] == "rt_mm": c = 'addr'
    elif m and m.group(3) == 'sp':
        off = int(m.group(2))
        if op == 'sw' and m.group(1) in tagreg: tagslot.add(off); c = 'addr'
        elif op == 'lw' and off in tagslot: tagreg.add(m.group(1)); c = 'addr'
        else: c = 'hostframe'
    elif m and m.group(3) == 's8':
        off = int(m.group(2))
        c = 'half64' if off % 8 == 4 else ('regload' if op == 'lw' else 'writeback')
    elif iscond(op) and any(x in tagreg for x in rg[:2]): c = 'addr'
    elif op == 'addiu' and args.startswith('sp,sp,'): c = 'hostframe'
    elif op == 'jr': c = 'hostframe'
    elif op == 'nop': c = 'nop'
    elif r["rel"] == "R": c = 'hostframe'
    if c is None: c = 'guest'
    # a register redefined by a non-tag instruction stops being a tag
    if c != 'addr' and rg and op not in ('sw', 'sh', 'sb', 'beq', 'bne', 'beql', 'bnel', 'beqz', 'bnez', 'beqzl', 'bnezl', 'jr', 'blez', 'bgtz', 'bltz', 'bgez') and rg[0] in tagreg:
        tagreg.discard(rg[0])
    cls[a] = c
# region tests found by their slow side: a helper call (rt_mm, rt_st*, rt_ld*) within a few instructions of one side
SLOWP = {}
def slow_side(a0):
    a = a0
    for _ in range(7):
        if a not in recs: return False
        r = recs[a]
        if r["op"] == 'jal': return r["rel"].startswith(('rt_mm', 'rt_st', 'rt_ld'))
        if r["op"][0] == 'b' or r["op"] in ('jr', 'j'): return False
        a += 4
    return False
for a in order:
    op = recs[a]["op"]
    if iscond(op) and cls[a] != 'poll':
        st, sf = slow_side(tgt(a)), slow_side(a + 8)
        if st != sf and op.rstrip('l') in ('beq', 'bne'): cls[a] = 'addr'; SLOWP[a] = 0.0 if st else 1.0
flow = {a: 0.0 for a in order}; pend = {0: 1.0}; unresolved = {}; calls = {}
it = 0
while pend:
    pc = max(pend, key=pend.get); f = pend.pop(pc); it += 1
    if f < 1e-7 or it > 400000: continue
    while True:
        r = recs[pc]; op = r["op"]; flow[pc] += f
        if op == 'jr': flow[pc + 4] += f; break
        if op in ('jal', 'jalr'):
            flow[pc + 4] += f; calls[r["rel"]] = calls.get(r["rel"], 0) + f; pc += 8; continue
        if op == 'b': flow[pc + 4] += f; pc = tgt(pc); continue
        if iscond(op):
            if pc in spec: p = spec[pc]
            elif cls[pc] == 'poll': p = 1.0 if op.startswith('bgtz') else 0.0   # the taken side of bgtz is 'no poll due'
            elif pc in SLOWP: p = SLOWP[pc]
            elif cls[pc] == 'addr': p = 1.0 if op.startswith('bne') else 0.0
            else: p = 0.0; unresolved[pc] = unresolved.get(pc, 0) + f
            flow[pc + 4] += f * p if islikely(op) else f
            if p > 0: pend[tgt(pc)] = pend.get(tgt(pc), 0) + f * p
            f *= 1 - p; pc += 8
            if f < 1e-7: break
            continue
        pc += 4
tot = {}
for a in order: tot[cls[a]] = tot.get(cls[a], 0) + flow[a]
T = sum(tot.values())
print("slots a call: %.1f" % T)
for k in sorted(tot, key=tot.get, reverse=True): print("  %-10s %8.1f  %5.1f%%" % (k, tot[k], 100 * tot[k] / T))
print("calls a call:", {k: round(v, 3) for k, v in calls.items()})
for pc in sorted(unresolved):
    if unresolved[pc] >= 0.004: print("UNRESOLVED %x %s %s  line %s flow %.3f" % (pc, recs[pc]["op"], recs[pc]["args"], recs[pc]["line"], unresolved[pc]))
if listing:
    for a in order:
        r = recs[a]
        if flow[a] > 0: print("%5x %9.3f %-9s %-6s %-26s %s:%d %s" % (a, flow[a], cls[a], r["op"], r["args"], r["line"][0][:7], r["line"][1], r["rel"]))
