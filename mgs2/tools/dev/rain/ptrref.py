# ptrref.py <stage.bin> <addr>...: lui/addiu (or lui/ori) pairs that build <addr> in a stage image based at 0x1f1c00, with the
# function (start after jr ra) holding each, and the strings that function references
import struct, sys
B = 0x1f1c00
d = open(sys.argv[1], 'rb').read()
def w(a): return struct.unpack_from('<I', d, a - B)[0]
end_text = w(B + 4)
def fstart(a):
    s = a
    while s > B + 0x10 and w(s - 8) != 0x03e00008: s -= 4
    return s
def strs(s):
    e = s; out = []; hi = {}
    while e < end_text and w(e) != 0x03e00008: e += 4
    for a in range(s, e + 8, 4):
        x = w(a); op = x >> 26; rs = (x >> 21) & 31; rt = (x >> 16) & 31; imm = x & 0xffff
        if op == 0x0f: hi[rt] = imm << 16
        elif op in (0x09, 0x0d) and rs in hi:
            t = hi[rs] + (imm - 0x10000 if (op == 0x09 and imm & 0x8000) else imm)
            if B <= t < B + len(d):
                o = t - B; z = d.find(b'\0', o, o + 64); st = d[o:z] if z > o else b''
                if len(st) >= 3 and all(32 <= c < 127 for c in st): out.append(st.decode())
    return out
for T in [int(x, 16) for x in sys.argv[2:]]:
    hi16 = (T + 0x8000) >> 16; lo = T & 0xffff
    for a in range(B, end_text, 4):
        x = w(a)
        if x >> 26 in (0x09, 0x0d) and x & 0xffff == lo:
            rs = (x >> 21) & 31
            for b in range(a - 4, max(B, a - 64), -4):
                y = w(b)
                if y >> 26 == 0x0f and (y >> 16) & 31 == rs:
                    if (y & 0xffff) == (hi16 if x >> 26 == 0x09 else T >> 16):
                        f = fstart(a); print('%06x built at %06x in function %06x; its strings: %s' % (T, a, f, strs(f)))
                    break
