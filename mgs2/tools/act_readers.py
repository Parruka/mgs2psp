#!/usr/bin/env python3
"""act_readers.py <MGS2_ACT_TRACE file>: per writer actor (ordered by logic cost, prof/actors_busy_v7.txt), the words per step read by the
renderer (DG_ActLast, VIF1 DMA), by itself one vblank later or older, by other actors, by the main thread or other threads.  Actor names
come from /mnt/project-files/mgs2/prof/actor_list_vb24050.txt and /mnt/project-files/mgs2/prof/actor_list_vb24980.txt (actor lists dumped from RAM, ephemeral).
Addresses and counts only."""
import collections, sys
names=collections.defaultdict(list)
for f in ('/mnt/project-files/mgs2/prof/actor_list_vb24050.txt','/mnt/project-files/mgs2/prof/actor_list_vb24980.txt'):
    for l in open(f):
        p=l.split()
        if len(p)==4 and p[2]=='act':
            a=int(p[3],16)
            if p[1] not in names[a]: names[a].append(p[1])
names[0x11d9d0]=['DG_ActFirst']; names[0x11da90]=['DG_ActLast']; names[0x13ee58]=['gvd_Act']
special={0:'loaded',1:'main',2:'thread'}
for ch in range(10): special[3+ch]='dma%d'%ch
def nm(x): return special[x] if x in special else names.get(x,['%06x'%x])[0]
cost={}
for l in open('/mnt/project-files/mgs2/prof/actors_busy_v7.txt'):
    p=l.split()
    if p and not l.startswith('#') and p[0]!='act':
        try: cost[int(p[0],16)]=float(l[44:52])
        except: pass
rows=[]
for l in open(sys.argv[1]):
    if l.startswith('#'): continue
    w,r,c,n,wd=l.split(); rows.append((int(w,16),int(r,16),int(c),int(n),int(wd)))
REND={0x11da90,4}   # DG_ActLast and VIF1 DMA (dma1 = id 4)
by=collections.defaultdict(lambda: collections.Counter())
for w,r,c,n,wd in rows:
    if r in REND: k='render'
    elif r==w: k='self_prev' if c==1 else 'self_old'
    elif r in (1,2): k='main/thread'
    else: k='others'
    by[w][k]+=wd
print('%-20s %6s | words read per step by: %8s %8s %8s %8s %8s | top other readers (words/step)'%('writer','ms','render','self+1','self+2','others','main/th'))
for w in sorted(by, key=lambda a:-cost.get(a,0)):
    if w<16 or cost.get(w,0)<0.1: continue
    b=by[w]
    oth=collections.Counter()
    for ww,r,c,n,wd in rows:
        if ww==w and r!=w and r not in REND and r not in (1,2): oth[nm(r)]+=wd
    print('%-20s %6.2f | %8.0f %8.0f %8.0f %8.0f %8.0f | %s'%(nm(w)[:20],cost.get(w,0),b['render']/100,b['self_prev']/100,b['self_old']/100,b['others']/100,b['main/thread']/100,
          ', '.join('%s %.0f'%(k,v/100) for k,v in oth.most_common(4))))
