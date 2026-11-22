#!/usr/bin/env python3
"""one.py <widths json> <unit> <function>: parse one function under the json's widths; its taken / save slots / unknown-sp loads,
the stack stores and loads with their slots"""
import sys, os, json
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpo2 as W
import wpd as P
D = W.D
J = json.load(open(sys.argv[1])); u = sys.argv[2]; f = sys.argv[3]
tk = json.load(open('/home/claude/lean/w2/taken.json'))
for x in tk['taken'].values(): W.INDN.update(x)
U = D.load(u); D.ctx(U)
wr = set(J['wr'].get(f, []))
F = W.parse(f, U, D.emit(U, f, wr), wr)
W.E[f] = sum(1 << r for r in J['E'].get(f, []))
W.G_['E_IND'] = sum(1 << r for r in J['E_IND'])
P.WIDE[f] = frozenset(wr)
e0, wat, spat, unk = P.wfwd(F)
print('%s: taken %s savek %s unk %s ind %s open %s' % (f, F.taken, sorted(F.savek), unk, F.ind, F.open))
for i, (ev, falls, li) in enumerate(F.blocks):
    for j, (kind, d, c) in enumerate(ev):
        if kind == 'st': print('  st line %d %s sp %s' % (li, d, spat.get((i, j))))
        if kind == 'd' and d[1][0] in ('LDS', 'SP', 'SP?'): print('  d line %d r%d %s sp %s w %s' % (li, d[0], d[1], spat.get((i, j)), wat.get((i, j))))
