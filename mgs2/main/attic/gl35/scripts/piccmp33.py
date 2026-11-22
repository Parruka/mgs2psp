"""piccmp31.py REFDIR NEWDIR NEWLOG [CYCLE] (gl31 G3): the emulator's pictures of two runs, same names (shot_<frame vb>.bmp,
live_<vb>.bmp): byte-identical or, per differing picture, the pixels whose luma (299 R + 587 G + 114 B) / 1000 differs by 2+, 6+ and
more than 8 levels and the largest difference.  Each picture's frame (a live capture's from NEWLOG's 'gl28 live vb N ... frame vb F'
line) is compared with the reference's shot of that same frame when it has one (a run with a shot at every frame shows other frames in its
live captures), else with the same name; it gets the stub's arm of its 100-vblank turn (CYCLE, default 0,0,3,3 from vb 24,001; a turn's first picture belongs to the arm
before) and whether it lies in 29,490-31,400.  Counts only: no picture leaves the container."""
import sys, os, re, struct, collections
import numpy as np
def load(p):
    b = open(p, 'rb').read()
    off = struct.unpack_from('<I', b, 10)[0]; w, h = struct.unpack_from('<ii', b, 18); bpp = struct.unpack_from('<H', b, 28)[0]
    assert bpp == 24, p
    rs = (w * 3 + 3) & ~3
    a = np.frombuffer(b, np.uint8, rs * abs(h), off).reshape(abs(h), rs)[:, :w * 3].reshape(abs(h), w, 3)
    if h > 0: a = a[::-1]
    a = a.astype(np.int32)
    return (a[:, :, 2] * 299 + a[:, :, 1] * 587 + a[:, :, 0] * 114) // 1000, a
ref, new, log = sys.argv[1:4]; cyc = [int(x) for x in (sys.argv[4] if len(sys.argv) > 4 else '0,0,3,3').split(',')]
lf = {}
for l in open(log, errors='replace'):
    m = re.match(r'picture: gl28 live vb (\d+) .*?: buffer \d frame vb (-?\d+)', l)
    if m: lf[int(m[1])] = int(m[2])
def arm(vb): return 0 if vb <= 24000 else cyc[((vb - 24001) // 100) % len(cyc)]
def first(vb): return vb > 24000 and (vb - 24001) % 100 < 2 and arm(vb) != arm(vb - 100)
A = set(n for n in os.listdir(ref) if n.endswith('.bmp')); B = set(n for n in os.listdir(new) if n.endswith('.bmp'))
names = sorted(A & B, key=lambda n: (int(re.sub(r'\D', '', n)), n))
T = collections.Counter(); rows = []
for n in names:
    k = n.split('_')[0]; v = int(re.sub(r'\D', '', n)); f = v if k == 'shot' else lf.get(v)
    cl = ('frame unknown' if f is None else ('arm %d%s' % (arm(f), ' (first picture of the turn)' if first(f) else '')) + (', in 29490-31400' if 29490 <= f <= 31400 else ', outside 29490-31400'))
    T[(k, cl, 'pictures')] += 1
    rn = n
    if k == 'live' and f is not None and os.path.exists(os.path.join(ref, 'shot_%d.bmp' % f)): rn = 'shot_%d.bmp' % f; T[(k, cl, "against the reference's shot of the same frame")] += 1
    if open(os.path.join(ref, rn), 'rb').read() == open(os.path.join(new, n), 'rb').read(): T[(k, cl, 'byte-identical')] += 1; continue
    (la, ca), (lb, cb) = load(os.path.join(ref, rn)), load(os.path.join(new, n)); d = np.abs(lb - la); dc = np.abs(cb - ca).max(axis=2)
    if not dc.any(): T[(k, cl, 'same pixels, file bytes differ')] += 1; continue
    T[(k, cl, 'differing')] += 1
    rows.append((n, f, cl, int((dc > 0).sum()), int((d >= 2).sum()), int((d >= 6).sum()), int((d > 8).sum()), int(d.max())))
print('%s (reference) against %s: %d names in both, only in the reference %d, only in the new %d %s' % (ref, new, len(names), len(A - B), len(B - A), sorted(B - A)[:6]))
for key in sorted(T): print('  %-5s %-58s %-32s %d' % (key[0], key[1], key[2], T[key]))
print('differing pictures (name, frame vb, class, pixels with any channel different, luma by 2+, 6+, over 8, largest luma difference):')
for r in rows: print('  ', r)
if not rows: print('   none')
