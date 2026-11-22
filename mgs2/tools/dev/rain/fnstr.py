# fnstr.py <stage.bin> <addr>: the function holding <addr> in a stage image based at 0x1f1c00, its start, the strings it references
# (lui/addiu or lui/ori pairs), and its jal targets
import struct, sys
B = 0x1f1c00
d = open(sys.argv[1], 'rb').read(); A = int(sys.argv[2], 16)
def w(a): return struct.unpack_from('<I', d, a - B)[0]
end_text = w(B + 4)
s = A
while s > B + 0x10 and not (w(s - 8) == 0x03e00008): s -= 4
e = A
while e < end_text and w(e) != 0x03e00008: e += 4
print('function %06x-%06x (text end %06x)' % (s, e + 8, end_text))
hi = {}
for a in range(s, e + 8, 4):
    x = w(a); op = x >> 26; rs = (x >> 21) & 31; rt = (x >> 16) & 31; imm = x & 0xffff
    if op == 0x0f: hi[rt] = imm << 16
    elif op in (0x09, 0x0d) and rs in hi:
        t = hi[rs] + (imm - 0x10000 if (op == 0x09 and imm & 0x8000) else imm)
        if B <= t < B + len(d):
            o = t - B; z = d.find(b'\0', o, o + 64)
            st = d[o:z] if z > o else b''
            if len(st) >= 3 and all(32 <= c < 127 for c in st): print('  %06x: string at %06x %r' % (a, t, st.decode()))
    elif op == 3:
        print('  %06x: jal %06x' % (a, ((a + 4) & 0xf0000000) | ((x & 0x3ffffff) << 2)))
