#!/usr/bin/env python3
"""gscap.py <capture> <SLES_503.83> [--frames]: what the game sends to the GS side per vblank (runtime/ee_io.h MGS2_GSCAP capture).
Walks the VIF1 command stream (DMA ch 1, tags with TTE), the GIF PATH2 packets inside it (DIRECT) and the GIF PATH3 packets (DMA ch 2), and
reports per vblank: VU1 micro program calls (MSCAL / MSCALF / MSCNT, named by the ELF's .DVP overlay that MPG put at that address),
UNPACK formats and bytes, MPG uploads, GS registers written with A+D, IMAGE (texture / CLUT) uploads, and how much of the UNPACK data is
the same (source address and bytes) as in the previous vblank, i.e. what a renderer could convert once and cache.
The capture is game data (personal use, never published); this tool prints numbers only."""
import struct, sys, hashlib, collections

cap, elf = sys.argv[1], sys.argv[2]
show_frames = '--frames' in sys.argv

# ---- VU1 programs: the ELF's .DVP.ovlytab pieces, grouped into programs (a piece at VU address 0 starts a program) ----
d = open(elf, 'rb').read()
e_shoff, = struct.unpack_from('<I', d, 0x20); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
secs = [struct.unpack_from('<IIIIII', d, e_shoff + i * shentsize) for i in range(shnum)]
def secname(o):
    base = secs[shstrndx][4]; return d[base + o:d.index(b'\0', base + o)].decode()
sec = {secname(s[0]): s for s in secs}
tab = sec['.DVP.ovlytab']; strt = sec['.DVP.ovlystrtab']
piece_ram = {}
pieces = []      # (program index, piece index, vu address, bytes)
prog = -1; prog_ram = []
for i in range(0, tab[5], 12):
    a, ram, vu = struct.unpack_from('<III', d, tab[4] + i)
    name = d[strt[4] + a:d.index(b'\0', strt[4] + a)].decode()
    body = sec[name]; data = d[body[4]:body[4] + body[5]]
    if vu == 0: prog += 1; prog_ram.append(ram)
    pieces.append((prog, len([p for p in pieces if p[0] == prog]), vu, data)); piece_ram[ram] = pieces[-1]
prog_name = ['P%02d@%06x' % (i, r) for i, r in enumerate(prog_ram)]

# ---- records ----
raw = open(cap, 'rb').read()
recs = [struct.unpack_from('<BBHI16s', raw, i) for i in range(0, len(raw), 24)]

GSREG = {0x00: 'PRIM', 0x01: 'RGBAQ', 0x06: 'TEX0_1', 0x07: 'TEX0_2', 0x08: 'CLAMP_1', 0x09: 'CLAMP_2', 0x14: 'TEX1_1', 0x15: 'TEX1_2',
         0x16: 'TEX2_1', 0x17: 'TEX2_2', 0x18: 'XYOFFSET_1', 0x19: 'XYOFFSET_2', 0x1a: 'PRMODECONT', 0x1b: 'PRMODE', 0x1c: 'TEXCLUT',
         0x22: 'SCANMSK', 0x34: 'MIPTBP1_1', 0x35: 'MIPTBP1_2', 0x36: 'MIPTBP2_1', 0x37: 'MIPTBP2_2', 0x3b: 'TEXA', 0x3d: 'FOGCOL',
         0x3f: 'TEXFLUSH', 0x40: 'SCISSOR_1', 0x41: 'SCISSOR_2', 0x42: 'ALPHA_1', 0x43: 'ALPHA_2', 0x44: 'DIMX', 0x45: 'DTHE',
         0x46: 'COLCLAMP', 0x47: 'TEST_1', 0x48: 'TEST_2', 0x49: 'PABE', 0x4a: 'FBA_1', 0x4b: 'FBA_2', 0x4c: 'FRAME_1', 0x4d: 'FRAME_2',
         0x4e: 'ZBUF_1', 0x4f: 'ZBUF_2', 0x50: 'BITBLTBUF', 0x51: 'TRXPOS', 0x52: 'TRXREG', 0x53: 'TRXDIR', 0x54: 'HWREG', 0x60: 'SIGNAL',
         0x61: 'FINISH', 0x62: 'LABEL'}
UFMT = {0: 'S-32', 1: 'S-16', 2: 'S-8', 4: 'V2-32', 5: 'V2-16', 6: 'V2-8', 8: 'V3-32', 9: 'V3-16', 10: 'V3-8', 12: 'V4-32', 13: 'V4-16',
        14: 'V4-8', 15: 'V4-5'}
PSM = {0: 'CT32', 1: 'CT24', 2: 'CT16', 0xa: 'CT16S', 0x13: 'T8', 0x14: 'T4', 0x1b: 'T8H', 0x24: 'T4HL', 0x2c: 'T4HH', 0x30: 'Z32',
       0x31: 'Z24', 0x32: 'Z16', 0x3a: 'Z16S'}

class Frame:
    def __init__(s, vb):
        s.vb = vb; s.c = collections.Counter(); s.calls = collections.Counter(); s.ufmt = collections.Counter(); s.ubytes = collections.Counter()
        s.regs = collections.Counter(); s.images = []; s.unpacks = []; s.mpg = []; s.tex0 = collections.Counter(); s.prims = collections.Counter()
        s.batch_unpacks = []; s.cur_batch = 0
frames = []; F = None

class Gif:
    def __init__(s, path): s.path = path; s.nloop = 0; s.blt = 0; s.trxreg = 0; s.trxpos = 0; s.img = None
    def qw(s, q, a=0):
        lo, hi = struct.unpack('<QQ', q)
        if not s.nloop:
            s.nloop = lo & 0x7fff; s.flg = (lo >> 58) & 3; s.nreg = (lo >> 60) & 15 or 16; s.regs = hi; s.ri = 0
            if s.flg == 0 and (lo >> 46) & 1: F.prims[(lo >> 47) & 7] += 1
            s.left = s.nloop if s.flg in (2, 3) else s.nloop * s.nreg
            F.c['gif%d tags' % s.path] += 1
            if s.flg in (2, 3) and s.nloop:
                w = (s.trxreg & 0xfff), (s.trxreg >> 32) & 0xfff
                s.img = [s.path, s.nloop * 16, PSM.get((s.blt >> 56) & 0x3f, hex((s.blt >> 56) & 0x3f)), w, None, hashlib.sha1(), (s.blt >> 32) & 0x3fff]
                F.images.append(s.img)
            if not s.left: s.nloop = 0
            return
        if s.flg == 0:
            r = (s.regs >> (4 * s.ri)) & 15
            if r == 0xe:
                a = hi & 0xff; F.regs[GSREG.get(a, hex(a))] += 1
                if a == 0x50: s.blt = lo
                elif a == 0x52: s.trxreg = lo
                elif a in (6, 7): F.tex0[('%s' % PSM.get((lo >> 20) & 0x3f, hex((lo >> 20) & 0x3f)), 1 << ((lo >> 26) & 15), 1 << ((lo >> 30) & 15))] += 1
            elif r == 0: F.prims[lo & 7] += 1
            s.ri = (s.ri + 1) % s.nreg; s.left -= 1
            if not s.left: s.nloop = 0
        elif s.flg == 1:
            s.left -= min(2, s.left)
            if not s.left: s.nloop = 0
        else:
            if s.img[4] is None: s.img[4] = a
            s.img[5].update(q); s.left -= 1
            if not s.left: s.nloop = 0
g2, g3 = Gif(2), Gif(3)

vu_owner = [None] * 2048          # VU1 micro memory (16 KB) in 8-byte instructions: (program, piece) that MPG put there
class Vif:
    def __init__(s): s.left = 0; s.cl = s.wl = 1; s.buf = []; s.cmd = 0; s.addr0 = 0; s.mpg_at = 0
    def word(s, w, a):
        if s.left:
            if not s.buf: s.addr0 = a
            s.buf.append(w); s.left -= 1
            if s.cmd in (0x50, 0x51) and len(s.buf) == 4: g2.qw(struct.pack('<4I', *s.buf), s.addr0); s.buf = []
            elif not s.left: s.done()
            return
        s.cmd = (w >> 24) & 0x7f; imm = w & 0xffff; num = (w >> 16) & 0xff; s.imm = imm; s.num = num; s.buf = []
        c = s.cmd; F.c['vif cmd %02x' % c] += 1
        if c == 0x01: s.cl = imm & 0xff; s.wl = (imm >> 8) & 0xff
        elif c == 0x20: s.left = 1
        elif c in (0x30, 0x31): s.left = 4
        elif c == 0x4a: s.left = (num or 256) * 2; s.mpg_at = imm
        elif c in (0x50, 0x51): s.left = (imm or 65536) * 4
        elif c in (0x14, 0x15, 0x17):
            at = imm if c != 0x17 else None
            owner = vu_owner[at] if at is not None and at < 2048 else None
            name = prog_name[owner[0]] + ('+%x' % (at * 8)) if owner else ('MSCNT' if at is None else 'unknown@%x' % (at * 8))
            F.calls[name] += 1; F.batch_unpacks.append(F.cur_batch); F.cur_batch = 0
        elif (c & 0x60) == 0x60:
            vl, vn = c & 3, (c >> 2) & 3; n = num or 256
            if s.wl > s.cl and s.wl: n = (n // s.wl) * s.cl + (n % s.wl if n % s.wl < s.cl else s.cl)
            bits = n * (32 >> vl) * (vn + 1); s.left = (bits + 31) // 32
            if not s.left: s.done()
    def done(s):
        c = s.cmd
        if c == 0x4a:
            p = piece_ram.get(s.addr0)      # the MPG data is DMA'd straight from the overlay piece's RAM copy
            F.mpg.append((p[0], p[1]) if p else ('?', len(s.buf) * 4))
            for k in range(len(s.buf) // 2):
                if s.mpg_at + k < 2048: vu_owner[s.mpg_at + k] = (p[0], p[1]) if p else None
        elif (c & 0x60) == 0x60:
            fmt = UFMT.get(c & 0xf, hex(c & 0xf)); data = struct.pack('<%dI' % len(s.buf), *s.buf)
            F.ufmt[fmt] += 1; F.ubytes[fmt] += len(data); F.cur_batch += 1
            F.unpacks.append((s.addr0, hashlib.sha1(data).digest()[:8], len(data), fmt, s.num or 256))
        s.buf = []
vif = Vif()

tte = {1: 0, 2: 0}
for kind, ch, _, a, q in recs:
    if kind == 2:
        F = Frame(a); frames.append(F); continue
    if F is None: continue
    if kind == 3:
        chcr, madr, qwc, tadr = struct.unpack('<4I', q); tte[ch] = chcr & 0x40; F.c['dma ch%d starts' % ch] += 1; continue
    F.c['dma ch%d %s' % (ch, 'tags' if kind == 1 else 'qwords')] += 1
    if ch == 1:
        w = struct.unpack('<4I', q)
        if kind == 1:
            if tte[1]: vif.word(w[2], a + 8); vif.word(w[3], a + 12)
        else:
            for i in range(4): vif.word(w[i], a + 4 * i)
    elif ch == 2 and kind == 0: g3.qw(q, a)

print('capture: %d records, %d vblanks (%d..%d)' % (len(recs), len(frames), frames[0].vb, frames[-1].vb))
prev = set(); prev_img = set()
for F in frames:
    keys = set((u[0], u[1]) for u in F.unpacks)
    same = sum(u[2] for u in F.unpacks if (u[0], u[1]) in prev); tot = sum(u[2] for u in F.unpacks)
    print('\n== vblank %d' % F.vb)
    print('  DMA: VIF1 %d starts, %d tags, %d qwords (%.1f KB); GIF %d starts, %d qwords' % (F.c['dma ch1 starts'], F.c['dma ch1 tags'],
          F.c['dma ch1 qwords'], F.c['dma ch1 qwords'] * 16 / 1024, F.c['dma ch2 starts'], F.c['dma ch2 qwords']))
    print('  VU1 calls: %d  %s' % (sum(F.calls.values()), ', '.join('%s %d' % kv for kv in F.calls.most_common())))
    if F.batch_unpacks: print('  UNPACKs per call: mean %.1f, max %d' % (sum(F.batch_unpacks) / len(F.batch_unpacks), max(F.batch_unpacks)))
    print('  UNPACK: %d, %.1f KB  %s' % (sum(F.ufmt.values()), tot / 1024, ', '.join('%s %d (%.1f KB)' % (k, F.ufmt[k], F.ubytes[k] / 1024) for k, _ in F.ufmt.most_common())))
    if prev: print('  UNPACK data also in the previous vblank (same address and bytes): %.1f KB of %.1f KB (%.0f%%)' % (same / 1024, tot / 1024, 100 * same / max(tot, 1)))
    print('  MPG uploads: %d  %s' % (len(F.mpg), ' '.join('%s.%s' % (prog_name[p] if isinstance(p, int) else p, k) for p, k in F.mpg[:24])))
    print('  GIF tags: path2 %d, path3 %d; PRIM types %s' % (F.c['gif2 tags'], F.c['gif3 tags'], dict(F.prims)))
    print('  GS registers (A+D): %s' % ', '.join('%s %d' % kv for kv in F.regs.most_common()))
    if F.tex0: print('  TEX0 (format, w, h): %s' % ', '.join('%s %dx%d: %d' % (k[0], k[1], k[2], v) for k, v in F.tex0.most_common(12)))
    if F.images:
        by = collections.Counter(); nb = collections.Counter()
        for p, n, psm, wh, src, h, dbp in F.images: by[(p, psm)] += n; nb[(p, psm)] += 1
        print('  IMAGE uploads: %s' % ', '.join('path%d %s %d x, %.1f KB' % (k[0], k[1], nb[k], by[k] / 1024) for k in by))
        ik = set((im[4], im[5].digest(), im[6]) for im in F.images)
        if prev_img: print('  IMAGE data also in the previous vblank (same source, bytes and GS address): %.1f KB of %.1f KB'
                           % (sum(im[1] for im in F.images if (im[4], im[5].digest(), im[6]) in prev_img) / 1024, sum(by.values()) / 1024))
        prev_img = ik
    vc = ' '.join('%s %d' % (k[8:], v) for k, v in sorted(F.c.items()) if k.startswith('vif cmd'))
    print('  VIF1 commands: %s' % vc)
    prev = keys
