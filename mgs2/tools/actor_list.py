#!/usr/bin/env python3
"""actor_list.py <32 MB guest RAM dump>: the actors in RAM: structs whose +0x18 is 0x13fe90 and whose +0x28 names a source file
(elf_notes.md: +0 next, +4 act, +0x14 die, +0x28 name).  Prints the count, then "struct name act" lines."""
import struct, sys
d=open(sys.argv[1],'rb').read()
def st(a):
    if not (0x100000<=a<0x2000000): return None
    e=d.find(b'\0',a,a+40); s=d[a:e]
    return s.decode() if e>a and all(32<c<127 for c in s) else None
out=[]
for o in range(0x100000,len(d)-0x30,4):
    if struct.unpack_from('<I',d,o+0x18)[0]==0x13fe90:
        n=st(struct.unpack_from('<I',d,o+0x28)[0]); act=struct.unpack_from('<I',d,o+4)[0]
        if n: out.append((o,n,act))
print(len(out))
for o,n,a in out: print('%06x %-24s act %06x'%(o,n,a))
