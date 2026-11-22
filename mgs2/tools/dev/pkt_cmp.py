#!/usr/bin/env python3
"""pkt_cmp.py <a> <b> [--max N]: compares two MGS2_PKT_DUMP files of one vblank (boot.c pkt_vblank: records "PKT", channel, guest
address (| 1 for a chain tag, its fields that act only), byte count, then the bytes). Prints the record counts and, record by record in
order, the first differing records with their differing quadwords."""
import struct, sys
args = [a for a in sys.argv[1:] if not a.startswith('--')]
mx = int(sys.argv[sys.argv.index('--max') + 1]) if '--max' in sys.argv else 10
if '--max' in sys.argv: args.remove(str(mx))
def load(fn):
    d = open(fn, 'rb').read(); i = 0; out = []
    while i + 16 <= len(d):
        mg, ch, a, n = struct.unpack_from('<4I', d, i); i += 16
        if mg != 0x544b50: raise SystemExit('%s: bad record at %d' % (fn, i - 16))
        out.append((ch, a, d[i:i + n])); i += n
    return out
A = load(args[0]); B = load(args[1])
print('records: a %d (%d bytes)  b %d (%d bytes)' % (len(A), sum(len(r[2]) for r in A), len(B), sum(len(r[2]) for r in B)))
shown = 0
for k in range(max(len(A), len(B))):
    ra = A[k] if k < len(A) else None; rb = B[k] if k < len(B) else None
    if ra == rb: continue
    def hd(r): return 'none' if r is None else 'ch %d %s %07x %5d bytes' % (r[0], 'tag ' if r[1] & 1 else 'data', r[1] & ~1, len(r[2]))
    print('record %d: a %s | b %s' % (k, hd(ra), hd(rb)))
    if ra and rb:
        n = 0
        for q in range(0, max(len(ra[2]), len(rb[2])), 16):
            qa = ra[2][q:q + 16]; qb = rb[2][q:q + 16]
            if qa != qb:
                f = lambda x: ' '.join('%08x' % w for w in struct.unpack('<%dI' % (len(x) // 4), x))
                print('   +%05x  a %s\n           b %s' % (q, f(qa), f(qb))); n += 1
                if n >= 6: print('   ...'); break
    shown += 1
    if shown >= mx: break
