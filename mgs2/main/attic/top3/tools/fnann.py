#!/usr/bin/env python3
"""fnann.py ELF PROF FUNC [OVL_ELF]: disassemble FUNC from ELF (or the overlay) with per-PC counts from a PspProf dump
(instr, I-miss, D-miss per window); prints per-step figures (80 steps). Model ms = (1.65 i + 68 im + 86 dm)/333000/80."""
import sys, subprocess, re
NM='/home/claude/pspsdk/pspdev/bin/psp-nm'; OD='/home/claude/pspsdk/pspdev/bin/psp-objdump'
elf, prof, fn = sys.argv[1:4]; ovl = sys.argv[4] if len(sys.argv) > 4 else None
P = {}
for l in open(prof):
    if l.startswith('#'): continue
    f = l.split()
    if len(f) >= 4: P[int(f[0],16)] = (int(f[1]), int(f[2]), int(f[3]))
for e in ([elf] + ([ovl] if ovl else [])):
    for l in subprocess.run([NM,'-S','--defined-only',e],capture_output=True,text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[3] == fn:
            a, sz = int(f[0],16), int(f[1],16); src = e; break
    else: continue
    break
d = subprocess.run([OD,'-d','--start-address=0x%x'%a,'--stop-address=0x%x'%(a+sz),src],capture_output=True,text=True).stdout
T=[0,0,0]; n=0
for l in d.split('\n'):
    m = re.match(r'\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(.*)', l)
    if not m: continue
    pc = int(m.group(1),16); c = P.get(pc,(0,0,0)); n+=1
    for j in range(3): T[j]+=c[j]
    print('%08x %9.1f %6.1f %6.1f  %s' % (pc, c[0]/80, c[1]/80, c[2]/80, m.group(2)))
print('# %s: %d instr in code; per step instr %.0f I %.0f D %.0f = %.3f ms' % (fn, n, T[0]/80, T[1]/80, T[2]/80, (1.65*T[0]+68*T[1]+86*T[2])/333000/80))
