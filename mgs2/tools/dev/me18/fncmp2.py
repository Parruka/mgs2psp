#!/usr/bin/env python3
"""fncmp2.py <a.o> <b.o>: fncmp.py with the sections' placement taken out: each function's code (disassembly with relocations,
addresses relative to its section) compared with '.text.hot.' / '.text.unlikely.' / '.text.startup.' read as '.text.' in the section
names and in every line, so a function that only moved between the hot and normal sections, or only calls one that moved, counts
as equal.  Prints the counts, every function whose code differs, and every function whose placement changed."""
import subprocess, sys, hashlib, re
OD = '/home/claude/pspsdk/pspdev/bin/psp-objdump'
PL = re.compile(r'\.text\.(hot|unlikely|startup|exit)\.')
def secs(o):
    p = subprocess.Popen([OD, '-d', '-r', '-z', o], stdout=subprocess.PIPE, text=True, errors='replace')
    out, cur, h, n = {}, None, None, 0
    for line in p.stdout:
        if line.startswith('Disassembly of section '):
            if cur: out[cur] = (place, h.hexdigest(), n)
            raw = line[len('Disassembly of section '):].strip().rstrip(':')
            m = PL.match(raw); place = m.group(1) if m else ''
            cur = PL.sub('.text.', raw); h = hashlib.md5(); n = 0
        elif cur is not None and line.strip():
            h.update(PL.sub('.text.', line).encode()); n += 1
    if cur: out[cur] = (place, h.hexdigest(), n)
    p.wait()
    return out
a, b = secs(sys.argv[1]), secs(sys.argv[2])
both = [k for k in a if k in b]
diff = [k for k in both if a[k][1] != b[k][1]]
moved = [k for k in both if a[k][0] != b[k][0]]
print('%s: %d sections, %s: %d; code equal %d, differs %d, only in the first %d, only in the second %d; placement changed %d' % (
    sys.argv[1], len(a), sys.argv[2], len(b), len(both) - len(diff), len(diff), len([k for k in a if k not in b]),
    len([k for k in b if k not in a]), len(moved)))
for k in diff: print('  code differs: %s (%d / %d lines; placement %s / %s)' % (k, a[k][2], b[k][2], a[k][0] or 'normal', b[k][0] or 'normal'))
for k in moved:
    if k not in diff: print('  only moved: %s (%s -> %s)' % (k, a[k][0] or 'normal', b[k][0] or 'normal'))
for k in [k for k in a if k not in b][:20]: print('  only in the first: %s' % k)
for k in [k for k in b if k not in a][:20]: print('  only in the second: %s' % k)
