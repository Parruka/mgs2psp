#!/usr/bin/env python3
"""calls.py ELF OVL CALLS [names...]: caller->callee counts per step (80 steps) from a PspProf .calls file, by function name.
With names: only edges where caller or callee is one of them."""
import sys, subprocess, bisect, collections
NM='/home/claude/pspsdk/pspdev/bin/psp-nm'
elf, ovl, cf = sys.argv[1:4]; want = set(sys.argv[4:])
S=[]
for e in (elf, ovl):
    for l in subprocess.run([NM,'-S','--defined-only',e],capture_output=True,text=True).stdout.split('\n'):
        f=l.split()
        if len(f)==4 and f[2] in 'tTwW': S.append((int(f[0],16),int(f[1],16),f[3]))
S.sort(); st=[s[0] for s in S]
def nm(pc):
    k=bisect.bisect_right(st,pc)-1
    return S[k][2] if k>=0 and pc<S[k][0]+S[k][1] else '?%x'%pc
E=collections.Counter()
for l in open(cf):
    f=l.split()
    if len(f)!=3: continue
    a,b,c=int(f[0],16),int(f[1],16),int(f[2])
    E[(nm(a),nm(b))]+=c
tot=collections.Counter()
for (a,b),c in E.items(): tot[b]+=c
for (a,b),c in sorted(E.items(),key=lambda x:-x[1]):
    if want and a not in want and b not in want: continue
    print('%9.1f  %-40s -> %s'%(c/80.0,a,b))
