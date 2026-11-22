#!/usr/bin/env python3
"""fn_ann.py <elf> <dump> <steps> <function>: one function's disassembly with each instruction's count, I-misses and D-misses per step
(<function>: a name, or its address in hex 0x...; static functions of the same name in several files: the one with the most instructions)"""
import sys, subprocess, bisect, re
B='/home/claude/pspsdk/pspdev/bin/'
E, D, steps, fn = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
syms = []
for l in subprocess.run([B+'psp-nm','-n',E],capture_output=True,text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0],16), p[2]))
addrs = sorted(set(a for a, _ in syms))
cand = [int(fn, 16)] if fn.startswith('0x') else sorted(set(a for a, n in syms if n == fn))
allc = {}
for l in open(D):
    if l[0] == '#': continue
    a, i, im, dm = l.split(); allc[int(a, 16)] = (int(i), int(im), int(dm))
def span(lo): return lo, addrs[bisect.bisect_right(addrs, lo)]
lo, hi = max((span(c) for c in cand), key=lambda r: sum(v[0] for a, v in allc.items() if r[0] <= a < r[1]))
cnt = {a: v for a, v in allc.items() if lo <= a < hi}
tot = sum(v[0] for v in cnt.values())
print("%s %#x-%#x (%d bytes): %.3f M instr per step" % (fn, lo, hi, hi - lo, tot / steps / 1e6))
for l in subprocess.run([B+'psp-objdump','-d','--start-address=%#x'%lo,'--stop-address=%#x'%hi,E],capture_output=True,text=True).stdout.splitlines():
    m = re.match(r'\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(.*)', l)
    if not m: continue
    a = int(m.group(1), 16); c = cnt.get(a, (0, 0, 0))
    print("%8d %5d %5d  %x  %s" % (c[0]//steps, c[1]//steps, c[2]//steps, a, m.group(2)[:70]))
