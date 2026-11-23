#!/usr/bin/env python3
"""census2.py PROF ELF... : weighted census of generated functions (P*, S_*_P*): hi-word making (sra x,y,0x1f then sw x,off%8==4),
address checks (srl x,y,0x1c), per-step counts (80 steps)."""
import sys, subprocess, re
OD='/home/claude/pspsdk/pspdev/bin/psp-objdump'
prof=sys.argv[1]; P={}
for l in open(prof):
    if l.startswith('#'): continue
    f=l.split()
    if len(f)>=4: P[int(f[0],16)]=(int(f[1]),int(f[2]),int(f[3]))
tot=dict(gen=0, hi=0, srl28=0, beql=0, sw_s=0, lw_s=0)
genre=re.compile(r'^[0-9a-f]+ <(P[0-9a-f]+|S_\w+_P[0-9a-f]+)[^>]*>:')
for elf in sys.argv[2:]:
    p=subprocess.Popen([OD,'-d','-j',('.ovl' if 'ovl_' in elf else '.text'),elf],stdout=subprocess.PIPE,text=True)
    ingen=False; win=[]
    for l in p.stdout:
        if l and l[0] in '0123456789abcdef' and '>:' in l:
            ingen=bool(genre.match(l)); win=[]; continue
        if not ingen: continue
        m=re.match(r'\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(\S+)\s*(.*)',l)
        if not m: continue
        pc=int(m.group(1),16); op=m.group(2); a=m.group(3); c=P.get(pc,(0,0,0))[0]
        tot['gen']+=c
        if op=='srl' and a.endswith(',0x1c'): tot['srl28']+=c
        if op=='beql': tot['beql']+=c
        if op=='sw':
            mm=re.match(r'(\w+),(-?\d+)\((\w+)\)',a)
            if mm:
                r,off=mm.group(1),int(mm.group(2))
                if off%8==4 and any(w[0]==r for w in win): tot['hi']+=2*c   # the sra + the sw
        win=[w for w in win if w[1]<6]; win=[(w[0],w[1]+1) for w in win]
        if op=='sra' and a.endswith(',0x1f'):
            win.append((a.split(',')[0],0))
    p.wait()
for k,v in tot.items(): print('%-6s %10.0f a step  %.2f ms (instr part)'%(k, v/80, v/80*1.65/333000))
