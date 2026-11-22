# refs.py <stage.bin> <addr>...: every jal to <addr> and every data word equal to <addr> in a stage image based at 0x1f1c00,
# with the function (start) holding each jal and the strings that function references
import struct, sys
B = 0x1f1c00
d = open(sys.argv[1], 'rb').read()
def w(a): return struct.unpack_from('<I', d, a - B)[0]
end_text = w(B + 4)
def fstart(a):
    s = a
    while s > B + 0x10 and w(s - 8) != 0x03e00008: s -= 4
    return s
for T in [int(x, 16) for x in sys.argv[2:]]:
    print('target %06x' % T)
    for a in range(B, B + len(d) - 3, 4):
        x = w(a)
        if a < end_text and x >> 26 == 3 and ((a + 4) & 0xf0000000) | ((x & 0x3ffffff) << 2) == T:
            print('  jal at %06x in function %06x' % (a, fstart(a)))
        if x == T: print('  word at %06x (%s)' % (a, 'text' if a < end_text else 'data'))
