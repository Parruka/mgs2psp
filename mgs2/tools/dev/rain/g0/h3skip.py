#!/usr/bin/env python3
"""h3skip.py: the two clip-flag reloads' MGS2_READ_SKIP ranges in xt3 (host offsets from rt_dgt_rd, [lo, hi)):
w01a's P313e60 0x313f4c (the same object as xt's: xt's offset 12fffcd moved with the function) and rain_gas_pers 0x3318f0 (w00a, built
with -g in xt3: the line table's ranges for that guest instruction's line).
CAVEAT: for w00a the line-range result held no rt_dgt_rd call; the offset used, f51bb5, came from addr2line on each rt_dgt_rd call of
S_w00a_P331160 (the reported line is the file line - 25 in that header; the return address is the call + 5)."""
import subprocess, re, sys
def nm(b):
    d = {}
    for l in subprocess.run(['nm', b], capture_output=True, text=True).stdout.split('\n'):
        p = l.split()
        if len(p) == 3: d[p[2]] = int(p[0], 16)
    return d
xt, x3 = nm('/home/claude/sbg0/xt/boot'), nm('/home/claude/sbg0/xt3/boot')
f = 'S_w01a_P313e60'; old = 0x12fffcd
assert xt[f] <= xt['rt_dgt_rd'] + old, 'old offset before the function'
nxt = min(v for v in xt.values() if v > xt[f]); assert xt['rt_dgt_rd'] + old < nxt, 'old offset past the function'
w01 = old - (xt[f] - xt['rt_dgt_rd']) + (x3[f] - x3['rt_dgt_rd'])
print('w01a P313e60+%x: xt %x -> xt3 %x' % (xt['rt_dgt_rd'] + old - xt[f], old, w01))
# w00a: the file line of /*3318f0*/ in wpl, less the #line 1 directive's file line
L = D = None
for i, l in enumerate(open('/home/claude/sbg0/wpl/gen_s_w00a.h', encoding='latin-1'), 1):
    if D is None and l.startswith('#line 1 '): D = i
    if l.startswith('/*3318f0*/'): L = i; break
rep = L - D; print('3318f0: file line', L, '#line at', D, '-> line', rep)
out = subprocess.run('objdump --dwarf=decodedline /home/claude/sbg0/xt3/boot', shell=True, capture_output=True, text=True).stdout
rows = []
for l in out.split('\n'):
    p = l.split()
    if len(p) >= 3 and p[0].endswith('gen_s_w00a.h') and p[1].isdigit() and p[2].startswith('0x'):
        rows.append((int(p[2], 16), int(p[1])))
rows.sort(); base = x3['rt_dgt_rd']; rng = []
for i, (a, ln) in enumerate(rows):
    if ln == rep and i + 1 < len(rows):
        rng.append((a - base, rows[i + 1][0] - base))
print('3318f0 ranges:', ' '.join('%x:%x' % r for r in rng))
print('XS=%s,%x:%x' % (','.join('%x:%x' % r for r in rng), w01, w01 + 1))
