import sys, struct
# pal.py GER b...: per batch, its texture's palette alpha range (PS2 0..0x80), aref, twopass, and the vertex alpha range (counts only)
fn = sys.argv[1]; ids = [int(a) for a in sys.argv[2:]]
b = open(fn, 'rb').read()
magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0)
for i in ids:
    r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i); t_, flags, atst, aref, afail, tfx, twop, first = r[0], r[2], r[4], r[5], r[6], r[12], r[13], r[20]
    tk, tpsm, rep, mina, tw, th, bw, rows, tdata, tsize, tpal, npal = struct.unpack_from('<4B4H4I', b, ot + 28 * t_)
    pal = struct.unpack_from('<%dI' % npal, b, od + tpal) if npal else ()
    al = [p >> 24 for p in pal]
    xo = od + first; nvx = struct.unpack_from('<I', b, xo + 164)[0]
    va = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k)[2] >> 24 for k in range(nvx)]
    print('b%d tex %d psm %d npal %d: palette alpha %s..%s, aref %#x atst %d afail %d twopass %d tfx %d, vertex alpha %d..%d' %
          (i, t_, tpsm, npal, min(al) if al else '-', max(al) if al else '-', aref, atst, afail, twop, tfx, min(va), max(va)))
