#!/usr/bin/env python3
"""p06_census.py <p6 dump> [...]: what the P06 calls of a capture hold and kick (gsref GSREF_P06DUMP; renderer_notes 11.13 step 4,
task #63), for the converter's design: calls and vertices by entry; the header (count, pass flags, ADC mask, clip); the tag templates
and REGS variants; the A+D packets' registers; per-vertex input ranges (position, blend weight, normal length, flag bits, the UV sets,
the colour factor); the lights; and what was kicked (tags, PRIM, vertices per pass, ADC set by the program, clipper fans).
Reads game data from the container; prints numbers only."""
import struct, sys, math
from collections import Counter, defaultdict

ENT = {0x19fe08: '+0', 0x1a0160: '+358', 0x1a0540: '+738', 0x1a08c0: '+ab0'}


def f32(u): return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def vuf(u):
    e = u & 0x7f800000
    if not e: return 0.0 if not (u >> 31) else -0.0
    if e == 0x7f800000: u = (u & 0x80000000) | 0x7f7fffff
    return f32(u)


def s32(u): return u - (1 << 32) if u & 0x80000000 else u


def read(path):
    d = open(path, 'rb').read(); o = 0
    while o + 32 <= len(d):
        mg, vb, call, ent, top, nt, nr, nq = struct.unpack_from('<8I', d, o); o += 32
        assert mg == 0x32433650, hex(mg)
        ab = [struct.unpack_from('<4I', d, o + 16 * k) for k in range(16)]; o += 256
        T = [struct.unpack_from('<4I', d, o + 16 * k) for k in range(nq)]; o += 16 * nq
        tags = []
        for k in range(nt):
            w = struct.unpack_from('<14Q', d, o); o += 112
            tags.append(w)
        recs = []
        for k in range(nr):
            w = struct.unpack_from('<10i3f4B', d, o); o += 56
            recs.append(w)
        yield dict(vb=vb, call=call, ent=ent, top=top, ab=ab, T=T, tags=tags, recs=recs)


class MM:
    def __init__(s): s.lo = None; s.hi = None; s.n = 0
    def add(s, v):
        s.n += 1
        if s.lo is None or v < s.lo: s.lo = v
        if s.hi is None or v > s.hi: s.hi = v
    def __str__(s): return '-' if s.lo is None else ('%g..%g' % (s.lo, s.hi))


def main():
    calls = Counter(); verts = Counter(); hdr = Counter(); tmpl = Counter(); ad = Counter(); kick = Counter()
    mm = defaultdict(MM); bits = defaultdict(Counter); zs = defaultdict(Counter); ws = defaultdict(Counter)
    out = Counter(); alpha = Counter(); nmax = Counter()
    for path in sys.argv[1:]:
        for c in read(path):
            e = ENT.get(c['ent'], hex(c['ent'])); T = c['T']; h = T[0x14]
            n = h[0] & 0xffff; calls[e] += 1; verts[e] += n; nmax[e] = max(nmax[e], n)
            hdr[(e, '%04x' % h[1], '%08x' % h[2], h[3] != 0, h[0] >> 16)] += 1
            tmpl[(e, 'tag %08x %08x %08x %08x' % T[0x15], 'fan %08x %08x %08x %08x' % T[0x16], 'regs %08x %08x %08x %08x' % T[0x17],
                  'out %08x %08x %08x %08x' % T[0x18])] += 1
            for off, bit in ((0xe8, 0), (0xf0, 0x10), (0xf8, 0x700)):   # the A+D packets of the passes the call draws: registers written
                if bit and not (h[1] & bit): continue
                tg = T[off]; nl = tg[0] & 0x7fff; nreg = (tg[1] >> 28) or 16; rg = tg[2] | tg[3] << 32; adr = []; k = 0
                for l in range(nl):
                    for r in range(nreg):
                        k += 1
                        if (rg >> (4 * r) & 15) == 0xe and off + k < len(T): adr.append('%02x' % (T[off + k][2] & 0xff))
                ad[(e, '%x' % off, 'nloop %d nreg %d regs %x' % (nl, nreg, rg), tuple(adr))] += 1
            # lights: rows T+4..6 hold the lights transposed (re/vu1_programs.md 2.2)
            for L in range(3):
                v = [vuf(T[4 + r][L]) for r in range(3)]
                mm[(e, 'light %d length' % L)].add(round(math.sqrt(sum(x * x for x in v)), 4))
            for k in range(4):
                for j in range(3): mm[(e, 'colour T+%x[%d]' % (8 + k, j))].add(round(vuf(T[8 + k][j]), 3))
            # per vertex
            for i in range(n):
                q = [T[0x100 + 6 * i + s] for s in range(6)] if 0x100 + 6 * i + 5 < len(T) else None
                if q is None: break
                p = [s32(x) for x in q[0]]
                for k, a in enumerate('xyz'): mm[(e, 'pos ' + a)].add(p[k])
                mm[(e, 'pos w (4.12 weight on 358/ab0)')].add(p[3])
                nrm = [s32(x) / 4096.0 for x in q[1][:3]]
                mm[(e, '|normal|')].add(round(math.sqrt(sum(x * x for x in nrm)), 3))
                fl = q[1][3] & 0xffffffff
                for b in range(32):
                    if fl >> b & 1: bits[e][b] += 1
                bits[e]['n'] += 1
                for s, need in ((2, 0), (3, 0x10), (4, 0x100)):   # slots 3 and 4 only when their pass reads them (else stale)
                    if need and not (h[1] & need): continue
                    mm[(e, 'slot %d s' % s)].add(s32(q[s][0])); mm[(e, 'slot %d t' % s)].add(s32(q[s][1]))
                    zs[(e, s)][s32(q[s][2])] += 1; ws[(e, s)][s32(q[s][3])] += 1
                if c['ent'] in (0x1a0540, 0x1a08c0):
                    for k in range(4): mm[(e, 'slot 5[%d]' % k)].add(s32(q[5][k]))
            # what was kicked
            tags = c['tags']; recs = c['recs']
            kick[(e, len(tags))] += 1
            fans = sum(1 for r in recs if r[14] == 5)
            out[(e, 'header w %s: calls %s fans' % ('!= 0' if h[3] else '0', 'with' if fans else 'without'))] += 1
            per = Counter((r[15], r[14]) for r in recs)   # (tag index, prim)
            for (ti, pr), m in per.items():
                tg = tags[ti] if ti < len(tags) else None
                regs = ('%016x' % tg[1]) if tg else '?'
                out[(e, 'prim %d' % pr, 'regs ' + regs)] += m
            for r in recs:
                if r[14] == 4: alpha[(e, r[7])] += 1
            # the program's ADC against the data's, per strip kick of n vertices
            strips = defaultdict(list)
            for r in recs:
                if r[14] == 4: strips[r[15]].append(r)
            for ti, rs in strips.items():
                if len(rs) != n: out[(e, 'strip kick with %d of %d vertices' % (len(rs), n))] += 1; continue
                for i, r in enumerate(rs):
                    din = T[0x101 + 6 * i][3] >> 15 & 1 if 0x101 + 6 * i < len(T) else 0
                    out[(e, 'adc data %d kicked %d' % (din, r[13]))] += 1
    print('calls by entry: ' + ', '.join('%s %d (%d vertices, at most %d a call)' % (k, calls[k], verts[k], nmax[k]) for k in sorted(calls)))
    print('\nheader (entry, y pass flags, z, w != 0, x >> 16): calls')
    for k, v in sorted(hdr.items(), key=lambda x: -x[1])[:30]: print('  %s: %d' % (k, v))
    print('\ntemplates T+0x15 tag, 0x16 fan, 0x17 REGS variants, 0x18 output tag: calls')
    for k, v in sorted(tmpl.items(), key=lambda x: -x[1])[:20]: print('  %s: %d' % (k, v))
    print('\nA+D packets of the passes drawn (entry, offset, tag, A+D register addresses): calls')
    for k, v in sorted(ad.items(), key=lambda x: (x[0][0], x[0][1], -x[1]))[:40]: print('  %s: %d' % (k, v))
    print('\nranges:')
    for k in sorted(mm, key=lambda x: (x[0], x[1])): print('  %s %s: %s (%d)' % (k[0], k[1], mm[k], mm[k].n))
    print('\nflag bits of slot 1 w (vertices with the bit / all):')
    for e in sorted(bits): print('  %s: %s of %d' % (e, ', '.join('bit %d %d' % (b, bits[e][b]) for b in range(32) if bits[e][b]), bits[e]['n']))
    print('\nslot z and w values (top 6):')
    for k in sorted(zs): print('  %s slot %d: z %s; w %s' % (k[0], k[1], zs[k].most_common(6), ws[k].most_common(6)))
    print('\nkicked: tags per call (entry, tags): calls')
    for k, v in sorted(kick.items()): print('  %s: %d' % (k, v))
    print('\nkicked vertices (entry, prim, tag REGS) and ADC data/kicked:')
    for k, v in sorted(out.items()): print('  %s: %d' % (k, v))
    print('\nkicked alpha (strip vertices):', sorted(alpha.items())[:12])


main()
