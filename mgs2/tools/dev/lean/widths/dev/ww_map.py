#!/usr/bin/env python3
"""ww_map.py <out.log> <boot binary> [base hex]: the 'wwatch:' lines with the writer named (nm; PIE loaded at base, setarch -R)"""
import bisect, subprocess, sys
base = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x555555554000
syms = []
for l in subprocess.run(['nm', '-n', sys.argv[2]], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
addrs = [s[0] for s in syms]
for l in open(sys.argv[1], errors='replace'):
    if not l.startswith('wwatch:'): continue
    p = l.split(); x = int(p[5], 16) - base; i = bisect.bisect_right(addrs, x) - 1
    print(' '.join(p[1:5]), '%s+%x' % (syms[i][1], x - syms[i][0]), ' '.join(p[6:]))
