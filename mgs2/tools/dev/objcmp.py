"""objcmp.py <old.o> <new.o>: per function section (-ffunction-sections), are the code bytes and the relocations (offset, type, symbol name)
the same?  Prints counts and the differing sections."""
import struct, sys
def load(fn):
    d = open(fn, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    S = []
    for i in range(shnum):
        nm, ty, fl, ad, off, sz, ln, inf, al, es = struct.unpack_from('<10I', d, shoff + i * shentsize)
        S.append(dict(nm=nm, ty=ty, off=off, sz=sz, link=ln, info=inf, es=es))
    st = S[shstrndx]
    name = lambda o, s=st: d[s['off'] + o: d.index(b'\0', s['off'] + o)].decode()
    for s in S: s['name'] = name(s['nm'])
    symtab = [s for s in S if s['ty'] == 2][0]; strtab = S[symtab['link']]
    syms = []
    for k in range(symtab['sz'] // 16):
        nm, val, sz, info, oth, shn = struct.unpack_from('<IIIBBH', d, symtab['off'] + 16 * k)
        n = name(nm, strtab) if nm else (S[shn]['name'] if shn < len(S) and (info & 15) == 3 else '')
        syms.append(n)
    out = {}
    for i, s in enumerate(S):
        if s['ty'] == 1 and s['name'].startswith('.text'):
            out[s['name']] = [d[s['off']: s['off'] + s['sz']], []]
    for s in S:
        if s['ty'] == 9 and S[s['info']]['name'] in out:
            r = out[S[s['info']]['name']][1]
            for k in range(s['sz'] // 8):
                o, inf = struct.unpack_from('<II', d, s['off'] + 8 * k); r.append((o, inf & 255, syms[inf >> 8]))
    return out
a = load(sys.argv[1]); b = load(sys.argv[2])
same = diff = 0; only_a = sorted(set(a) - set(b)); only_b = sorted(set(b) - set(a)); D = []
for k in sorted(set(a) & set(b)):
    if a[k] == b[k]: same += 1
    else: diff += 1; D.append((k, len(a[k][0]), len(b[k][0]), a[k][0] == b[k][0]))
print('sections: %d same, %d differ, %d only in old, %d only in new' % (same, diff, len(only_a), len(only_b)))
for k, la, lb, bytes_same in D[:40]: print('  differ %-40s %6d -> %6d bytes%s' % (k, la, lb, ' (bytes same, relocations differ)' if bytes_same else ''))
for k in only_a[:20]: print('  only old', k, len(a[k][0]))
for k in only_b[:20]: print('  only new', k, len(b[k][0]))
