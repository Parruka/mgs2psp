#!/usr/bin/env python3
"""vcen_sum.py <out log>...: the volume census of the x86 live check (boot/xlive_chk.c MGS2_XLIVE_VCEN=1, renderer_notes 11.12) by
vblank kind.  The walk at vblank N carries the packets of the draw call of vblank N - 1 (renderer_notes 11.2), so vblank N is "shown"
when that call ran in full ("dgc: vb N-1 0", MGS2_PKT_HASH window) and "reduced" when it ran reduced ("dgc: vb N-1 1"); "none": no call.  Per kind: vblanks, then the mean (and max) per vblank of VIF1
words, command words, UNPACK / MPG / DIRECT / other data words, GIF quadwords by path and their IMAGE quadwords, DMA quadwords, VU1
calls (converter / rule only / no rule), front-end primitives; the UNPACK vectors by format; and the walk's packet hashes against the
exact walk's ("xlive: vb N h" = "pkt: vb N h")."""
import re, sys
FMT = {0: 'S-32', 1: 'S-16', 2: 'S-8', 4: 'V2-32', 5: 'V2-16', 6: 'V2-8', 8: 'V3-32', 9: 'V3-16', 10: 'V3-8', 12: 'V4-32', 13: 'V4-16',
       14: 'V4-8', 15: 'V4-5'}
def fmt(f): return FMT.get(f & 15, '?%x' % f) + (' masked' if f & 16 else '')
KEYS = ['w', 'cw', 'up', 'mpg', 'dir', 'oth', 'g1', 'g2', 'g3', 'i2', 'i3', 'qw']
for path in sys.argv[1:]:
    dgc = {}; vc = {}; xh = {}; ph = {}
    for ln in open(path, errors='replace'):
        if ln.startswith('dgc: vb '):
            p = ln.split(); dgc.setdefault(int(p[2]), []).append(int(p[3]) & 1)
        elif ln.startswith('xlive vc: vb '):
            p = ln.split(); v = int(p[3]); d = {}
            i = 4
            while i < len(p) and p[i] != 'calls': d[p[i]] = int(p[i + 1]); i += 2
            d['conv'], d['rule'], d['none'] = int(p[i + 1]), int(p[i + 2]), int(p[i + 3])
            d['prims'] = int(p[i + 5]); d['uq'] = {int(x.split(':')[0], 16): int(x.split(':')[1]) for x in p[i + 7:]}
            vc[v] = d
        else:
            m = re.match(r'xlive: vb (\d+) ([0-9a-f]{16})$', ln.strip())
            if m: xh[int(m.group(1))] = m.group(2); continue
            m = re.match(r'pkt: vb (\d+) ([0-9a-f]{16})', ln.strip())
            if m: ph[int(m.group(1))] = m.group(2)
    eq = sum(1 for v in xh if ph.get(v) == xh[v])
    print('== %s: %d vblanks (%d-%d); the walk\'s packet hash equal to the exact walk\'s on %d / %d' % (path, len(vc), min(vc), max(vc), eq, len(xh)))
    kinds = {}
    for v, d in vc.items():
        k = dgc.get(v - 1)
        kind = 'none' if not k else 'shown' if 0 in k else 'reduced'
        if k and len(k) > 1: kind += ' (%d calls)' % len(k)
        kinds.setdefault(kind, []).append(d)
    cols = KEYS + ['conv', 'rule', 'none', 'prims']
    print('%-18s %5s ' % ('kind', 'n') + ' '.join('%9s' % c for c in cols))
    for kind, L in sorted(kinds.items()):
        n = len(L)
        print('%-18s %5d ' % (kind + ' mean', n) + ' '.join('%9.0f' % (sum(d[c] for d in L) / n) for c in cols))
        print('%-18s %5s ' % ('  max', '') + ' '.join('%9d' % max(d[c] for d in L) for c in cols))
        uq = {}
        for d in L:
            for f, x in d['uq'].items(): uq[f] = uq.get(f, 0) + x
        tot = sum(uq.values()) or 1
        print('   UNPACK vectors per vblank by format: ' + ', '.join('%s %.0f (%.1f%%)' % (fmt(f), x / n, 100 * x / tot) for f, x in sorted(uq.items(), key=lambda t: -t[1])))
        m = lambda c: sum(d[c] for d in L) / n
        print('   bytes per vblank: VIF1 %.0f KB (UNPACK data %.0f KB, MPG %.0f KB, DIRECT %.0f KB: IMAGE %.0f KB, the rest %.0f KB); '
              'path 3 %.0f KB (IMAGE %.0f KB)' % (m('w') * 4 / 1024, m('up') * 4 / 1024, m('mpg') * 4 / 1024, m('dir') * 4 / 1024,
              m('i2') * 16 / 1024, (m('g2') - m('i2')) * 16 / 1024, m('g3') * 16 / 1024, m('i3') * 16 / 1024))
