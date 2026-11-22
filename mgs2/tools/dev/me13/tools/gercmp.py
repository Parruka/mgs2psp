# gercmp.py <a.ger> <b.ger>: two GE replay files of the same frame (ge/replay_fmt.h): the header, the batches, the vertices and the data
# block compared, then each texture's record, texels and palette: which differ and by how many bytes (task #147: what a fresh texture
# cache changes at the take-back)
import struct, sys
HF = '<12I3f'; BF = '<HBB4B6B2B4h4I'; VS = 24; TF = '<4B4H4I'
def load(fn):
    f = open(fn, 'rb').read()
    h = struct.unpack_from(HF, f, 0)
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od = h[:10]
    bs = struct.calcsize(BF); ts = struct.calcsize(TF)
    T = [struct.unpack_from(TF, f, ot + i * ts) for i in range(nt)]
    return dict(f=f, h=h, vb=vb, nb=nb, nv=nv, nt=nt, nd=nd, ob=ob, ov=ov, ot=ot, od=od, bs=bs, ts=ts, T=T)
A = load(sys.argv[1]); B = load(sys.argv[2])
print('vb %d / %d: batches %d / %d, vertices %d / %d, textures %d / %d, data %d / %d bytes' % (A['vb'], B['vb'], A['nb'], B['nb'], A['nv'], B['nv'], A['nt'], B['nt'], A['nd'], B['nd']))
sa = A['f'][A['ob']:A['ob'] + A['nb'] * A['bs']]; sb = B['f'][B['ob']:B['ob'] + B['nb'] * B['bs']]
print('batches equal:', sa == sb, '; vertices equal:', A['f'][A['ov']:A['ov'] + A['nv'] * VS] == B['f'][B['ov']:B['ov'] + B['nv'] * VS])
# the frame's own data (before the textures' texels)
fd = lambda X: X['f'][X['od']:X['od'] + min(t[8] for t in X['T'] if t[0] == 1)] if any(t[0] == 1 for t in X['T']) else X['f'][X['od']:]
for i in range(min(A['nt'], B['nt'])):
    ta, tb = A['T'][i], B['T'][i]
    kind, psm, rep, mina, tw, th, bw, rows, data, size, pal, npal = ta
    rec = ta[:8] == tb[:8] and ta[9] == tb[9] and ta[11] == tb[11]
    if kind != 1:
        if not rec: print('texture %d: kind %d record differs: %s / %s' % (i, kind, ta, tb))
        continue
    da = A['f'][A['od'] + data:A['od'] + data + size]; db = B['f'][B['od'] + tb[8]:B['od'] + tb[8] + tb[9]]
    pa = A['f'][A['od'] + pal:A['od'] + pal + 4 * npal]; pb = B['f'][B['od'] + tb[10]:B['od'] + tb[10] + 4 * tb[11]]
    nd_ = sum(1 for x, y in zip(da, db) if x != y) + abs(len(da) - len(db))
    np_ = sum(1 for k in range(0, min(len(pa), len(pb)), 4) if pa[k:k + 4] != pb[k:k + 4]) + abs(len(pa) - len(pb)) // 4
    if not rec or nd_ or np_:
        print('texture %d: CLUT%d %dx%d (stored %d x %d rows), %d bytes, %d palette entries: record %s, texel bytes differing %d, palette entries differing %d'
              % (i, psm, tw, th, bw, rows, size, npal, 'equal' if rec else 'DIFFERS %s / %s' % (ta, tb), nd_, np_))
print('done')
