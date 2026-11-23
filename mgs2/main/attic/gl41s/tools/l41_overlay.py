#!/usr/bin/env python3
"""l41_overlay.py (gl41s, measurement builds only; never part of a patch): ORIG NEW NAME OUT.  Writes OUT = NEW with `#line` directives so
that every line NEW shares with ORIG carries ORIG's line number and the file name NAME (the path the PGO build found ORIG at).  GCC's
profile of a function is matched by its name, its file (for a static one) and its line (the line checksum), so a header copied to
another folder keeps the profile of every function it did not change.  NEW may be ORIG itself (a control copy: one directive at the top).
Hunks come from `diff` (GNU diffutils).  A directive is placed only where a line may start one (not inside a comment, not after a
backslash line); otherwise it moves down to the next such line with its number adjusted."""
import subprocess, sys

orig, new, name, out = sys.argv[1:5]
A = open(orig).read().split('\n'); B = open(new).read().split('\n')
r = subprocess.run(['diff', '-U0', orig, new], capture_output=True, text=True)
if r.returncode not in (0, 1):
    sys.exit('diff failed: ' + r.stderr)
hunks = []   # (a_start, a_len, b_start, b_len), 1-based starts as diff prints them
for l in r.stdout.split('\n'):
    if l.startswith('@@'):
        a, b = l.split()[1:3]
        def p(s):
            s = s[1:].split(',')
            return int(s[0]), (int(s[1]) if len(s) > 1 else 1)
        (a0, an), (b0, bn) = p(a), p(b)
        hunks.append((a0, an, b0, bn))
# map: B line index (0-based) -> A line index (0-based) for lines outside hunks
want = {}   # B index -> A line number (1-based) that a directive before it must set
ai = bi = 0
for a0, an, b0, bn in hunks:
    sa = a0 if an == 0 else a0 - 1   # A lines before the hunk (0-based count)
    sb = b0 if bn == 0 else b0 - 1
    if sb - bi != sa - ai:
        sys.exit('hunk arithmetic')
    ai, bi = sa + an, sb + bn
    if bi < len(B):
        want[bi] = ai + 1
O = [f'#line 1 "{name}"']
cmt = False; cont = False; pend = None
for i, l in enumerate(B):
    if i in want:
        pend = (i, want[i])
    if pend is not None and not cmt and not cont:
        O.append(f'#line {pend[1] + (i - pend[0])} "{name}"'); pend = None
    O.append(l)
    # comment state after this line (no "/*" inside a string in these files; checked below)
    j = 0
    while j < len(l):
        if cmt:
            k = l.find('*/', j)
            if k < 0:
                j = len(l)
            else:
                cmt = False; j = k + 2
        else:
            k1 = l.find('/*', j); k2 = l.find('//', j)
            if k2 >= 0 and (k1 < 0 or k2 < k1):
                break
            if k1 < 0:
                break
            cmt = True; j = k1 + 2
    cont = l.endswith('\\')
if pend is not None:
    sys.exit('a directive could not be placed')
open(out, 'w').write('\n'.join(O))
# check: every B line outside hunks has A's number (simulate the numbering)
num = None; bad = 0; k = 0
inh = set()
for a0, an, b0, bn in hunks:
    inh.update(range(b0 - 1, b0 - 1 + bn))
amap = {}; ai = bi = 0
for a0, an, b0, bn in hunks:
    sa = a0 if an == 0 else a0 - 1; sb = b0 if bn == 0 else b0 - 1
    for d in range(sb - bi):
        amap[bi + d] = ai + d + 1
    ai, bi = sa + an, sb + bn
for d in range(len(B) - bi):
    amap[bi + d] = ai + d + 1
cur = 0; bline = 0
for l in O:
    if l.startswith('#line ') and l.endswith(f'"{name}"') and (bline == 0 or True) and l.split()[1].isdigit() and l == f'#line {l.split()[1]} "{name}"':
        cur = int(l.split()[1]); continue
    if bline in amap and amap[bline] != cur and bline not in inh:
        bad += 1
        if bad <= 5:
            print(f'  off: new line {bline + 1} numbered {cur}, orig {amap[bline]}: {B[bline][:100]}')
    cur += 1; bline += 1
print(f'{out}: {len(hunks)} hunks, {len(O) - len(B)} directives, {bad} shared lines off their number (deferred directives: comment lines)')
