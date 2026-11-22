#!/usr/bin/env python3
# pcdis.py <dump> <elf> <func> [steps]: disassembly of one function with each instruction's count a step (PspProf dump), I and D misses
import sys, subprocess
dump, elf, fn = sys.argv[1], sys.argv[2], sys.argv[3]; steps = float(sys.argv[4]) if len(sys.argv) > 4 else 80
P = '/home/claude/pspsdk/pspdev/bin/'
lo = size = None
for l in subprocess.run([P + 'psp-nm', '-S', elf], capture_output=True, text=True).stdout.splitlines():
    t = l.split()
    if len(t) == 4 and t[3] == fn: lo, size = int(t[0], 16), int(t[1], 16)
cnt = {}
for l in open(dump):
    if l.startswith('#'): continue
    t = l.split(); pc = int(t[0], 16)
    if lo <= pc < lo + size: cnt[pc] = (int(t[1]), int(t[2]), int(t[3]))
out = subprocess.run([P + 'psp-objdump', '-d', '--start-address=0x%x' % lo, '--stop-address=0x%x' % (lo + size), elf], capture_output=True, text=True).stdout
for l in out.splitlines():
    s = l.strip()
    if ':' in s and s.split(':')[0].strip() and all(ch in '0123456789abcdef' for ch in s.split(':')[0].strip()) and '\t' in l:
        pc = int(s.split(':')[0], 16); c = cnt.get(pc, (0, 0, 0))
        print('%9.1f %6.1f %6.1f  %s' % (c[0] / steps, c[1] / steps, c[2] / steps, l.split('\t', 1)[1] if '\t' in l else l))
