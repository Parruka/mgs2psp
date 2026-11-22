# list one entry's batches: prim, sprites, area, texture, blend, tests
import struct, sys
f = open(sys.argv[1], 'rb').read(); want = int(sys.argv[2], 16)
(magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h) = struct.unpack_from('<12I', f, 0)
BF = '<HBB4B6B2B4h4I'; BS = struct.calcsize(BF); TF = '<4B4H4I'; TS = struct.calcsize(TF)
for i in range(nb):
    b = struct.unpack_from(BF, f, ob + i * BS)
    if b[-4] != want: continue
    tex, prim, flags, ztst, atst, aref, afail, ba, bb, bc, bd, fix, tfx, twop, pad, sx0, sy0, sx1, sy1, entry, first, count, px = b
    vs = [struct.unpack_from('<2fI3f', f, ov + (first + k) * 24) for k in range(count)]
    sa = 0; ta = 0; sizes = []
    if prim == 6:
        for k in range(0, count - 1, 2):
            p, q = vs[k], vs[k + 1]; a = abs(q[3]-p[3]) * abs(q[4]-p[4]); sa += a; ta += abs(q[0]-p[0]) * abs(q[1]-p[1]); sizes.append((abs(q[3]-p[3]), abs(q[4]-p[4])))
    t = struct.unpack_from(TF, f, ot + tex * TS) if flags & 2 else None
    tn = '-' if not t else ('fb%d' % t[1] if t[0] == 2 else 'T%d %dx%d' % (t[1], t[4], t[5]))
    msz = '' if not sizes else 'mean %.1fx%.1f' % (sum(s[0] for s in sizes) / len(sizes), sum(s[1] for s in sizes) / len(sizes))
    print('%5d prim %d n %4d area %7.0f tx/px %5.2f %-12s fl %02x z%d a%d/%02x blend %d%d%d%d fix %02x tfx %d px %7d %s sc %d,%d-%d,%d' % (i, prim, count, sa, ta / sa if sa else 0, tn, flags, ztst, atst, aref, ba, bb, bc, bd, fix, tfx, px, msz, sx0, sy0, sx1, sy1))
