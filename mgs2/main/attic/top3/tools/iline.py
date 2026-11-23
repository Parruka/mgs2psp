#!/usr/bin/env python3
"""iline.py PROF ELF OVL: I-misses by 64-byte line for generated code (gen.o / gen_s_w00a.o functions = P..., S_w00a_P...) vs the rest
of the main CPU; per step (80).  Lines touched in the window, misses per line per step distribution."""
import sys, subprocess, bisect, collections, re
NM='/home/claude/pspsdk/pspdev/bin/psp-nm'
prof, elf, ovl = sys.argv[1:4]
S=[]
for e in (elf, ovl):
    for l in subprocess.run([NM,'-S','--defined-only',e],capture_output=True,text=True).stdout.split('\n'):
        f=l.split()
        if len(f)==4 and f[2] in 'tTwW': S.append((int(f[0],16),int(f[1],16),f[3]))
S.sort(); st=[s[0] for s in S]
def nm(pc):
    k=bisect.bisect_right(st,pc)-1
    return S[k][2] if k>=0 and pc<S[k][0]+S[k][1] else '?'
gen=re.compile(r'^(S_\w+?_)?P[0-9a-f]{6}')
L=collections.defaultdict(lambda:[0,0,0]); kind={}
for l in open(prof):
    if l.startswith('#'): continue
    f=l.split()
    if len(f)<4: continue
    pc=int(f[0],16); ln=pc>>6
    x=L[ln]; x[0]+=int(f[1]); x[1]+=int(f[2]); x[2]+=1
    if ln not in kind: kind[ln]='gen' if gen.match(nm(pc)) else 'other'
for k in ('gen','other'):
    ls=[v for ln,v in L.items() if kind[ln]==k and v[0]>0]
    mi=sum(v[1] for v in ls)/80.0
    print('%s: lines executed in window %d (%.0f KB); I-miss %.0f a step; lines with misses %d' % (k, len(ls), len(ls)*64/1024, mi, sum(1 for v in ls if v[1])))
    b=collections.Counter(); bm=collections.Counter()
    for v in ls:
        r=v[1]/80.0
        key='0' if r==0 else '<0.5' if r<0.5 else '0.5-1' if r<=1.0 else '1-2' if r<=2 else '2-5' if r<=5 else '5-20' if r<=20 else '>20'
        b[key]+=1; bm[key]+=r
    for key in ('0','<0.5','0.5-1','1-2','2-5','5-20','>20'): print('   misses/step per line %-6s lines %6d  misses/step %8.0f' % (key, b[key], bm[key]))
