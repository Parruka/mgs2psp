# actlcg.py <stage.bin> <name.c>...: for each actor file name in a stage image (based at 0x1f1c00): the GV_SetNamedActor (0x13f378)
# calls that pass the name (a3) with the act (a1) and die (a2) they pass; then the functions reachable from the act by jal inside the
# image, each with its LCG sites (loads of 0x1fc954: lui 0x20 + lw -0x36ac) and its size
import struct, sys
B = 0x1f1c00
d = open(sys.argv[1], 'rb').read()
def w(a): return struct.unpack_from('<I', d, a - B)[0]
end_text = w(B + 4)
def fstart(a):
    s = a
    while s > B + 0x10 and w(s - 8) != 0x03e00008: s -= 4
    return s
def fend(s):
    e = s
    while e < end_text and w(e) != 0x03e00008: e += 4
    return e + 8
def regval(a, r, lim=40):
    # the value built in register r before a (lui/addiu/ori), scanning back
    lo = None
    for b in range(a - 4, max(B, a - 4 * lim), -4):
        y = w(b); op = y >> 26; rt = (y >> 16) & 31; rs = (y >> 21) & 31
        if op in (9, 13) and rt == r and lo is None:
            lo = (op, y & 0xffff, rs)
            if rs != r: r = rs
        elif op == 15 and rt == r:
            hi = (y & 0xffff) << 16
            if lo is None: return hi
            return hi + ((lo[1] - 0x10000) if (lo[0] == 9 and lo[1] & 0x8000) else lo[1])
    return None
def lcg_sites(s, e):
    n = []
    for a in range(s, e, 4):
        x = w(a)
        if x >> 26 in (0x23, 0x09) and x & 0xffff == 0xc954:
            rs = (x >> 21) & 31
            for b in range(a - 4, max(B, a - 48), -4):
                y = w(b)
                if y >> 26 == 15 and (y >> 16) & 31 == rs:
                    if y & 0xffff == 0x20: n.append(a)
                    break
    return n
def reach(f0):
    seen = [f0]; i = 0
    while i < len(seen):
        s = seen[i]; e = fend(s); i += 1
        for a in range(s, e, 4):
            x = w(a)
            if x >> 26 == 3:
                t = ((a + 4) & 0xf0000000) | ((x & 0x3ffffff) << 2)
                if B <= t < end_text and t not in seen: seen.append(t)
    return seen
for nm in sys.argv[2:]:
    sb = nm.encode() + b'\0'; i = 0; addrs = []
    while True:
        i = d.find(sb, i)
        if i < 0: break
        addrs.append(B + i); i += 1
    for a in range(B, end_text, 4):
        x = w(a)
        if x >> 26 == 3 and ((x & 0x3ffffff) << 2) == 0x13f378:
            n3 = regval(a + 4, 7)
            if n3 in addrs:
                act = regval(a + 4, 5); die = regval(a + 4, 6)
                print('%s: GV_SetNamedActor at %06x (in %06x): act %s die %s' % (nm, a, fstart(a), act and '%06x' % act, die and '%06x' % die))
                if act:
                    tot = 0
                    for f in reach(act):
                        e = fend(f); ls = lcg_sites(f, e); tot += len(ls)
                        print('   %06x %5d B  LCG sites %d %s' % (f, e - f, len(ls), ' '.join('%06x' % x for x in ls)))
                    print('   LCG sites reachable: %d' % tot)
