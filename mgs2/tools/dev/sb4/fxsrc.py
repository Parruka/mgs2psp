import sys
# fxsrc.py (task #114): the probe's 25f0 / 2870 calls with their vertex data address (XlCall.src), strips (MGS2_FX_STRIPS), the frames'
# thinned blocks and the report lines
idx = {}; keep = False
for line in sys.stdin:
    if line.startswith('call entry'):
        p = line.split(); vb = int(p[8]); idx[vb] = idx.get(vb, -1) + 1; keep = p[2] in ('19a9f0', '19ac78')
        if keep:
            t0 = int(p[12], 16)
            print(vb, idx[vb] + 1, p[2], '%x %x %02x' % (t0 & 0x3fff, t0 >> 37 & 0x3fff, int(p[14], 16) & 0xff), 'top', p[4], 'src', p[18])
        continue
    if keep and line.startswith('fx '):
        q = line.split(); print('  fx n', q[4], 'strips', q[6], 'same', q[10]); continue
    if keep and line.startswith('fs '): print('  ', line.rstrip()); continue
    if line.startswith(('thin vb', 'xlive gb: thinned', 'xlive mode: the rain', 'xlive gb: effect', 'xlive: vb 308')): print(line.rstrip())
