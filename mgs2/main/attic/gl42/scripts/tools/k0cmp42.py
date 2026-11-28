"""k0cmp42.py A.log B.log [lo hi]: gl42 (= build_gl41/tools/k0cmp41.py with sched42's kinds; native kinds N, 2, R): the picture lines of two
logs (twin or emulator; .gz read too) frame by frame, by the KIND of each frame's turn in tools/sched42.py (turn t = (vb - 24000) // 100 of
the picture line's vb; '-' outside the turns: before 24,000 or from 31,400).  Per kind: frames, list/frame/counts differ, dz text differs, and content differs ('picture: vb N
content C', the twin's line and gl41's PSP line; frames with a content line in both logs).  Lists the differing frames of kinds other than N
(with MGS2_NGE on, N frames' list and frame digests differ from a switch-off run by design; their content must not)."""
import sys, os, gzip
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched42
def rd(p):
    f = gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace'); D = {}; C = {}
    for l in f:
        if not l.startswith('picture: vb '): continue
        if ' list ' in l:
            h, _, z = l.rstrip('\n').partition('; dz '); D[int(l.split()[2])] = (h.split(' buffer ')[1].split(' ', 1)[1], z)
        elif ' content ' in l:
            w = l.split()
            if len(w) == 5 and w[3] == 'content': C[int(w[2])] = w[4]
    return D, C
(A, AC), (B, BC) = rd(sys.argv[1]), rd(sys.argv[2]); lo, hi = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, 1 << 30)
def kind(vb):
    t = (vb - 24000) // 100
    return sched42.K[t] if vb >= 24000 and t < sched42.N else '-'
vbs = sorted(B); n = {}; d = {}; dz = {}; nc = {}; dc = {}; bad = []; miss = 0
for vb in vbs:
    if not lo <= vb <= hi: continue
    if vb not in A: miss += 1; continue
    k = kind(vb); n[k] = n.get(k, 0) + 1
    if A[vb][0] != B[vb][0]:
        d[k] = d.get(k, 0) + 1
        if k not in ('N', '2', 'R'): bad.append((vb, k))
    if A[vb][1] != B[vb][1]: dz[k] = dz.get(k, 0) + 1
    if vb in AC and vb in BC:
        nc[k] = nc.get(k, 0) + 1
        if AC[vb] != BC[vb]: dc[k] = dc.get(k, 0) + 1; bad.append((vb, k + ' content'))
print('frames in B %d (vb %d..%d), not in A %d, in A and not in B %d' % (len(vbs), vbs[0] if vbs else -1, vbs[-1] if vbs else -1, miss,
      len([v for v in A if v not in B and lo <= v <= hi])))
KN = dict(sched42.KN); KN['-'] = 'outside the turns: before 24,000 or from 31,400'
for k in sorted(n): print('%s (%s): frames %d, list/frame/counts differ %d, dz text differs %d; content lines in both %d, content differs %d' % (
    k, KN[k], n[k], d.get(k, 0), dz.get(k, 0), nc.get(k, 0), dc.get(k, 0)))
print('differing frames (not N/2/R list/frame) (vb, kind):', bad[:40])
