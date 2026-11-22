#!/usr/bin/env python3
"""make_pak.py <run log> <disc root> <out.pak>: the disc data of one scripted run, for the PSP harness (iop_host.h DISC_PACK).
The run log must hold the disc table ("disc: <path> lsn <hex> size <n>", printed at start) and MGS2_DISC_LOG=1 read lines.  The pack keeps
the table and only the sectors the run read (MOVIE.DAT skipped: the run serves it as end of stream); every other sector reads as zeros.
Format (little endian): "MGS2PAK1", u32 nfiles, u32 nranges; nfiles x {char path[64]; u32 lsn; u32 size}; nranges x {u32 lsn; u32 count;
u32 sector offset into the data}; then the data from the next 2048-byte boundary.  Game data: personal use only, never published."""
import re, struct, sys, os
def find(root, parts):   # the disc path under root, any letter case (a copy of the disc may have lower-case names)
    for c in parts:
        names = {n.upper(): n for n in os.listdir(root)}
        root = os.path.join(root, names.get(c.upper(), c))
    return root
log, root, out = sys.argv[1:4]
files = []; reads = {}
for l in open(log, errors='replace'):
    m = re.match(r'disc: (\\\S+)\s+lsn\s+([0-9a-f]+) size (\d+)$', l.strip())
    if m: files.append((m.group(1), int(m.group(2), 16), int(m.group(3)))); continue
    m = re.match(r'disc: vblank \d+ read (\S+) sector (\d+) x (\d+)', l)
    if m: reads.setdefault(m.group(1), set()).update(range(int(m.group(2)), int(m.group(2)) + int(m.group(3))))
lsn_of = {p: (lsn, size) for p, lsn, size in files}
for p, lsn, size in files:   # small files whole: MODULE.IPK, IOPRP23.IMG ... are also read by byte (fileio, iopheap), not logged per sector
    if size <= 2 << 20: reads.setdefault(p, set()).update(range((size + 2047) // 2048))
abs_secs = []
for p, secs in reads.items():
    if p.upper().endswith('MOVIE.DAT'): continue
    lsn, size = lsn_of[p]
    abs_secs += [(lsn + s, p, s) for s in sorted(secs)]
abs_secs.sort()
ranges = []
for a, p, s in abs_secs:
    if ranges and ranges[-1][0] + ranges[-1][1] == a and ranges[-1][2] == p: ranges[-1][1] += 1
    else: ranges.append([a, 1, p, s])
hdr = b'MGS2PAK1' + struct.pack('<II', len(files), len(ranges))
hdr += b''.join(struct.pack('<64sII', p.encode(), lsn, size) for p, lsn, size in files)
off = 0; rtab = b''
for r in ranges: rtab += struct.pack('<III', r[0], r[1], off); off += r[1]
hdr += rtab; hdr += b'\0' * (-len(hdr) % 2048)
with open(out, 'wb') as f:
    f.write(hdr)
    for a, n, p, s in ranges:
        host = find(root, p.strip('\\').split('\\'))
        with open(host, 'rb') as h:
            h.seek(s * 2048); d = h.read(n * 2048)
        f.write(d + b'\0' * (n * 2048 - len(d)))
print('%d files, %d ranges, %d sectors (%.1f MB)' % (len(files), len(ranges), off, off * 2048 / 1e6))
