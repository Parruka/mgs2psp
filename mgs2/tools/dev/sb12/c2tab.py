import struct, math
def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
def bits(x): return struct.unpack('<I', struct.pack('<f', x))[0]
T = [f32(((d + 0.5) / 255.0) ** 2) for d in range(255)]
# integer fast path: S = n0^2+n1^2+n2^2 (4.12 normals); S_f >= S/2^24 (1 - 4 * 2^-23) in RZ; want S_f >= T[254] for every S >= SHI
t254 = T[254]
SHI = math.ceil(t254 * 2**24 / (1 - 1e-6))
print('T254 %.9g (0x%08x), SHI %d (sqrt %.6f of 4096: len %.7f)' % (t254, bits(t254), SHI, math.sqrt(SHI), math.sqrt(SHI) / 4096))
# check monotone, and print the C table
assert all(T[i] < T[i + 1] for i in range(254))
lines = []
for i in range(0, 255, 6):
    lines.append('  ' + ', '.join('0x%08xu' % bits(T[j]) for j in range(i, min(i + 6, 255))) + ',')
open('c2tab.inc', 'w').write('\n'.join(lines) + '\n')
print(len(lines), 'lines;', lines[0]); print(lines[-1])
