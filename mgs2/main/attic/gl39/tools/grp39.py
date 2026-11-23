#!/usr/bin/env python3
"""grp39.py PROFILE... (python3 -I): prof39.py outputs (run with a large TOP) summed by group, ms a PSP step [C model]: the game's code by
library (HD names) or stage file; the runtime by job."""
import sys, re, collections
def grp(side, name, obj):
    if side == 'game':
        m = re.search(r'\[(\w+)/(\w+)\]', name)
        if m: return 'game ' + m.group(1) + ('/' + m.group(2) if m.group(1) == 'libdg' else '')
        m = re.search(r'\(([\w.]+)\+', name)
        if m: return 'game stage ' + m.group(1)
        return 'game (no name)'
    n = name.split()[0]
    if re.match(r'vup|vuc_|vu_|vif0|vu0', n): return 'rt VU0 micro programs'
    if re.match(r'io_dma|gif_|rt_qcopy|rt_idiom_qcopy|ee_copy|io_flush|rt_store|rt_mm|memcpy|memset|rt_half', n): return 'rt DMA/GIF walk, copies, stores: ' + n
    if n.startswith('nat_') or n.startswith('rt_nat'): return 'rt native pieces (nat_*)'
    if re.match(r'gen_jalr|guest_call|k_|rt_syscall|rt_intc|rt_', n): return 'rt dispatch, kernel calls, interrupts'
    if obj in ('spu2_lite', 'iop_main') or n.startswith('F_'): return 'rt sound + IOP'
    if obj == 'xl_psp': return 'rt picture hand-over (xl_psp)'
    return 'rt other'
for p in sys.argv[1:]:
    G = collections.defaultdict(lambda: [0.0] * 4); side = None; print('#', p)
    for l in open(p):
        m = re.match(r' -- (\S+), top', l)
        if m: side = m.group(1); continue
        m = re.match(r'\s+([-+][\d.]+) = ([-+][\d.]+) ([-+][\d.]+) ([-+][\d.]+)  (.{52}) ?(\S*)', l)
        if not m or side in (None, 'ME'): continue
        g = grp(side, m.group(5).strip(), m.group(6))
        for j in range(4): G[g][j] += float(m.group(j + 1))
    tot = [sum(v[j] for v in G.values()) for j in range(4)]
    print('  %-62s %7.2f = instr %6.2f + I-miss %6.2f + D-miss %6.2f' % ('main CPU, all', *tot))
    for g, v in sorted(G.items(), key=lambda kv: -kv[1][0]):
        if abs(v[0]) >= 0.25: print('  %-62s %7.2f = instr %6.2f + I-miss %6.2f + D-miss %6.2f' % (g, *v))
