#!/usr/bin/env python3
"""prof_mix.py <dump> <disasm> [<disasm> ...]: the executed PSP instruction mix of a PspProf per-PC dump (speed_notes §48).  <disasm>:
`psp-objdump -d --no-show-raw-insn` of the ELF and the stage overlay the dump ran.  Prints the share of loads, stores and branches, the
guest memory check (`srl 28`), the RAM mask (`ext 0,25`), the sign word of 64-bit stores (`sra 31`), the loads / stores by what they
reach (the machine state in the PSP scratchpad, the host stack, guest RAM; base register traced back in the same function) and by context
(function entry / exit, around a call, body), the machine state by variable (psp/spr_state.ld), and the calls per 100 steps."""
import re, sys, collections, bisect
dump, dis = sys.argv[1], sys.argv[2:]
ins = []; idx = {}; nf = 0
for f in dis:
    for ln in open(f):
        if re.match(r'^[0-9a-f]+ <.*>:', ln): nf += 1; continue
        m = re.match(r'\s+([0-9a-f]+):\s+(\S+)\s*(.*)', ln)
        if m: idx[int(m.group(1), 16)] = len(ins); ins.append((m.group(2), m.group(3), nf))
C = {}
for l in open(dump):
    if l[0] != '#': a, c, i, m = l.split(); C[int(a, 16)] = int(c)
MEM = set('lw lh lhu lb lbu lwc1 lv.s lv.q lwl lwr sw sh sb swc1 sv.s sv.q swl swr'.split())
BR = set('jal jalr jr j b beq bne beqz bnez blez bgtz bltz bgez beql bnel beqzl bnezl bc1t bc1f bc1tl bc1fl blezl bgtzl bltzl bgezl'.split())
def dst(o, args):
    if o in MEM or o in BR or o in ('nop', 'sync', 'cache', 'mtc1', 'ctc1') or o.startswith('v') or o.startswith('c.') or o.endswith('.s') \
       or o.startswith('cvt'): return None
    p = args.split(','); return p[0].strip() if p else None
def trace(i, reg, depth=0):
    """what reg holds before instruction i: ('state', value) for a scratchpad address, ('ram', None), ('const', v), or None"""
    j = i - 1; fn = ins[i][2]; n = 0
    while j >= 0 and ins[j][2] == fn and n < 400:
        o2, a2, _ = ins[j]
        if dst(o2, a2) == reg:
            p = [x.strip() for x in a2.split(',')]
            if o2 == 'addu' and 's7' in p[1:]: return ('ram', None)
            if o2 == 'lui':
                try: return ('const', int(p[1], 16) << 16)
                except ValueError: return None
            if o2 in ('addiu', 'move') and depth < 6:
                if p[1] == 's7': return ('ram', None)
                t = trace(j, p[1], depth + 1)
                if t and t[0] == 'const' and o2 == 'addiu': return ('const', t[1] + int(p[2], 0))
                return t
            return None
        j -= 1; n += 1
    return None
SYM = [(0x10000, 'VF'), (0x10200, 'VN'), (0x10400, 'R'), (0x10500, 'RH'), (0x10600, 'F'), (0x10680, 'HI/LO'), (0x106a0, 'ACC'), (0x106a4, 'CC'),
       (0x106a8, 'SAR'), (0x106ac, 'k_left'), (0x106b0, 'k_tbase/poll_at'), (0x106c0, 'VI'), (0x10700, 'VACC'), (0x10710, 'VU0 flags'),
       (0x10730, 'misc'), (0x10800, 'VUMEM'), (0x11800, 'VUPROG'), (0x14000, '?')]
SA = [s for s, _ in SYM]
fb = {}
for i, x in enumerate(ins): fb.setdefault(x[2], [i, i])[1] = i
JAL = set(i for i, x in enumerate(ins) if x[0] in ('jal', 'jalr')); JR = set(i for i, x in enumerate(ins) if x[0] == 'jr' and x[1].strip() == 'ra')
near = lambda i, S, b, a: any(k in S for k in range(i - a, i + b + 1))
tot = sum(C.values()); pat = collections.Counter(); tgt = collections.Counter(); ctx = collections.Counter(); var = collections.Counter()
for a, c in C.items():
    i = idx.get(a)
    if i is None: continue
    o, args, fn = ins[i]
    if o in MEM: pat['loads' if o[0] == 'l' else 'stores'] += c
    if o in BR: pat['branches and jumps'] += c
    if o == 'srl' and args.endswith(',0x1c'): pat['srl 28 (guest memory check)'] += c
    if o == 'ext' and args.endswith(',0x0,0x19'): pat['ext 0,25 (RAM mask)'] += c
    if o == 'sra' and args.endswith(',0x1f'): pat['sra 31 (sign word of a 64-bit store)'] += c
    if o in ('jal', 'jalr'): pat['calls'] += c
    if o not in MEM: continue
    m = re.search(r'(-?\w*)\((\w+)\)', args)
    if not m: continue
    b = m.group(2)
    if b == 'sp': k = 'host stack'
    elif b == 's7': k = 'guest RAM'
    else:
        t = trace(i, b)
        if t and t[0] == 'ram': k = 'guest RAM'
        elif t and t[0] == 'const' and 0x10000 <= t[1] + int(m.group(1) or '0', 0) < 0x14000:
            k = 'machine state'; ad = t[1] + int(m.group(1) or '0', 0)
            var['%s %s' % (SYM[bisect.bisect_right(SA, ad) - 1][1], 'load' if o[0] == 'l' else 'store')] += c
        else: k = 'other or not traced'
    tgt[k] += c
    if k in ('host stack', 'machine state'):
        f0 = fb[fn][0]
        w = 'entry' if i - f0 < 40 else 'exit' if near(i, JR, 30, 0) else 'call' if near(i, JAL, 24, 8) else 'body'
        ctx['%s, %s' % (k, w)] += c
pc = lambda v: 100.0 * v / tot
print('%.2f M instructions per step (100-step dump)' % (tot / 100 / 1e6))
for k, v in pat.most_common(): print('  %-38s %5.2f%%' % (k, pc(v)) if k != 'calls' else '  calls per step %.0f' % (v / 100))
print('loads and stores by what they reach:')
for k, v in tgt.most_common(): print('  %-38s %5.2f%%' % (k, pc(v)))
print('machine state and host stack by context:')
for k, v in ctx.most_common(): print('  %-38s %5.2f%%' % (k, pc(v)))
print('machine state by variable:')
for k, v in var.most_common(14): print('  %-38s %5.2f%%' % (k, pc(v)))
