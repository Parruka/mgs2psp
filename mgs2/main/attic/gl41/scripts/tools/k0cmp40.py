"""k0cmp40.py A.log B.log [lo hi]: gl40 (= build_gl39/tools/k0cmp39.py with sched40): the picture lines ('picture: vb N buffer .. list L frame F: counts; dz ..') of two logs (twin or
emulator; .gz read too) frame by frame, by the KIND of each frame's turn in the schedule (tools/sched40.py).  A frame's turn is that of
the hook it began at = its picture line's vb (bound turn t = (vb - 24000) // 100; checked on the twin: with the line before's vb the
turns' first frames fall in the wrong turn).  Prints, per kind,
the frames and how many differ (list or frame digest or counts; then the dz text), and lists the differing K0 frames."""
import sys, os, re, gzip
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched40
def rd(p):
    f = gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace'); D = {}
    for l in f:
        if l.startswith('picture: vb ') and ' list ' in l:
            h, _, z = l.rstrip('\n').partition('; dz '); D[int(l.split()[2])] = (h.split(' buffer ')[1].split(' ', 1)[1], z)
    return D
A, B = rd(sys.argv[1]), rd(sys.argv[2]); lo, hi = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, 1 << 30)
vbs = sorted(B); n = {}; d = {}; dz = {}; k0 = []; miss = 0; prev = None
for vb in vbs:
    beg = vb
    if not lo <= vb <= hi: continue
    if vb not in A: miss += 1; continue
    t = (beg - 24000) // 100; k = int(sched40.bound[t]) if 0 <= t < len(sched40.bound) and beg >= 24000 else 0
    n[k] = n.get(k, 0) + 1
    if A[vb][0] != B[vb][0]:
        d[k] = d.get(k, 0) + 1
        if k == 0: k0.append((vb, beg, t))
    if A[vb][1] != B[vb][1]: dz[k] = dz.get(k, 0) + 1
print('frames in B %d (vb %d..%d), not in A %d, in A and not in B %d' % (len(vbs), vbs[0], vbs[-1], miss, len([v for v in A if v not in B and lo <= v <= hi])))
for k in sorted(n): print('K%d: frames %d, list/frame/counts differ %d, dz text differs %d' % (k, n[k], d.get(k, 0), dz.get(k, 0)))
print('K0 frames differing (vb, begun at, turn):', k0[:40])
