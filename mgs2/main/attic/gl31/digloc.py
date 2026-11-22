#!/usr/bin/env python3
"""digloc.py DIGFILE TWIN_DIGEST OTHER_DIGEST (gl31 dbg; python3 -I): DIGFILE is the instrumented twin's hashed stream of one frame's
gl_digest (pieces <u32 n><n bytes>; each piece hashed as 32-bit words then single bytes: h = (h ^ unit) * 0x100000001b3).  Checks that
the stream gives TWIN_DIGEST, then looks for the ONE unit whose change would give OTHER_DIGEST (the emulator's digest of the same
frame): with forward states F (twin) and backward states B (from OTHER_DIGEST through the twin's units), a single changed unit i has
F_i ^ B_i = old ^ new, a value of at most 32 bits; every later position gives a 64-bit mix.  Prints the piece, the offset and the two values."""
import sys, struct
P = 0x100000001b3; M = (1 << 64) - 1; PI = pow(P, -1, 1 << 64)
b = open(sys.argv[1], 'rb').read(); tw = int(sys.argv[2], 16); ot = int(sys.argv[3], 16)
U = []; pieces = []; o = 0
while o < len(b):
    n = struct.unpack_from('<I', b, o)[0]; o += 4; pieces.append((len(U), n)); i = 0
    while i + 4 <= n: U.append((struct.unpack_from('<I', b, o + i)[0], len(pieces) - 1, i, 4)); i += 4
    while i < n: U.append((b[o + i], len(pieces) - 1, i, 1)); i += 1
    o += n
F = [0xcbf29ce484222325]
for u in U: F.append(((F[-1] ^ u[0]) * P) & M)
print('%s: %d pieces, %d units; pieces (first unit, bytes): %s%s' % (sys.argv[1], len(pieces), len(U), pieces[:8], ' ...' if len(pieces) > 8 else ''))
print('stream digest %016x; the twin line\'s %016x: %s' % (F[-1], tw, 'equal' if F[-1] == tw else 'NOT EQUAL'))
h = ot; hits = []
for k in range(len(U) - 1, -1, -1):
    h = ((h * PI) & M) ^ U[k][0]
    d = h ^ F[k]
    if d >> 32 == 0 and d and (U[k][3] == 4 or d < 256): hits.append((k, d))
print('single-unit explanations of %016x: %d' % (ot, len(hits)))
for k, d in hits:
    v, pc, off, sz = U[k]
    print('  unit %d: piece %d (of %d bytes) offset %d (0x%x), %d bytes: twin %0*x, other %0*x (xor %x)' % (k, pc, pieces[pc][1], off, off, sz, 2 * sz, v, 2 * sz, v ^ d, d))
    lo = max(pieces[pc][0], k - 12); hi = min(pieces[pc][0] + (pieces[pc][1] + 3) // 4, k + 12)
    print('   twin units around it: ' + ' '.join(('[%08x]' if j == k else '%08x') % U[j][0] for j in range(lo, min(hi, len(U)))))
