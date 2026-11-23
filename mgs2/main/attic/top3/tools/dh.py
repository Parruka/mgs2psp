import sys, subprocess, bisect, collections
NM='/home/claude/pspsdk/pspdev/bin/psp-nm'
dh, elf = sys.argv[1:3]
S=[]
for l in subprocess.run([NM,'-S','--defined-only',elf],capture_output=True,text=True).stdout.split('\n'):
    f=l.split()
    if len(f)==4 and f[2] in 'bBdDrR': S.append((int(f[0],16),int(f[1],16),f[3]))
S.sort(); st=[s[0] for s in S]
G0=0x090c0c00; GS=0x2000000
reg=collections.Counter(); sym=collections.Counter(); gr=collections.Counter()
for l in open(dh):
    f=l.split(); a=int(f[0],16); c=int(f[1])/80.0
    if G0<=a<G0+GS:
        g=a-G0
        if g<0x4000: reg['guest scratchpad (16 KB)']+=c
        else: reg['guest RAM']+=c; gr[g>>16]+=c
    elif 0x10000<=a<0x14000: reg['PSP scratchpad']+=c
    else:
        k=bisect.bisect_right(st,a)-1
        if k>=0 and a<S[k][0]+max(S[k][1],64): sym[S[k][2]]+=c; reg['host statics']+=c
        else: reg['host other (stack/heap/ME)']+=c
for k,v in reg.most_common(): print('%-30s %8.0f' % (k,v))
print('guest RAM by 64 KB (top 25):')
for k,v in gr.most_common(25): print('   %07x %7.0f' % (k<<16, v))
print('host statics (top 15):')
for k,v in sym.most_common(15): print('   %-28s %7.0f' % (k,v))
