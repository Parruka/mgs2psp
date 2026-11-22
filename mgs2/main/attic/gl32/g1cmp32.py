"""g1cmp31.py A.log B.log [cycle] (gl31 G1): two twin logs frame by frame (vb): the 'picture: vb N buffer .. list L frame F: <counts>; dz
<text>' line (list+frame digests, the counts, the dz text with sub:), the 'picture: vb N content C' line and, when both logs have them,
the natb / natz / natq lines.  With a cycle (e.g. 0,0,3,3: MGS2_NAT_CYCLE's arms, turns of 100 vb from vb 24,001) the differences are
counted by the arm of the frame's turn; a turn's first picture is listed apart (it belongs to the arm before).  Prints the hash lines'
comparison too ('hash:' lines)."""
import sys, re, collections
def rd(p):
    D = dict(pic={}, dz={}, con={}, natb={}, natz={}, natq={}, hash={})
    for l in open(p, errors='replace'):
        if l.startswith('picture: vb '):
            w = l.split(); vb = int(w[2])
            if w[3] == 'content': D['con'][vb] = w[4]
            elif ' list ' in l:
                h, _, z = l.rstrip('\n').partition('; dz '); D['pic'][vb] = h; D['dz'][vb] = z
        elif l.startswith('natb: vb ') or l.startswith('natz: vb ') or l.startswith('natq: vb '):
            w = l.split(); D[w[0][:4]][int(w[2])] = l.rstrip('\n')
        elif l.startswith('hash: ') or l.startswith('vb ') and ' ee ' in l:
            m = re.search(r'vb (\d+)', l)
            if m: D['hash'].setdefault(int(m[1]), []).append(l.rstrip('\n'))
    return D
A = rd(sys.argv[1]); B = rd(sys.argv[2]); cyc = [int(x) for x in sys.argv[3].split(',')] if len(sys.argv) > 3 else None
def arm(vb):
    if not cyc or vb <= 24000: return 0
    return cyc[((vb - 24001) // 100) % len(cyc)]
vbs = sorted(B['pic']); first = set(); prev = None
for vb in vbs:
    t = (vb - 24001) // 100 if vb > 24000 else -1
    if cyc and t != prev and t >= 0 and arm(vb) != (arm(vb - 100) if t > 0 else 0): first.add(vb)
    prev = t
print('%s: %d picture lines, %d content; %s: %d, %d; same frames: %s' % (sys.argv[1], len(A['pic']), len(A['con']), sys.argv[2], len(B['pic']), len(B['con']),
      sorted(A['pic']) == sorted(B['pic']) and sorted(A['con']) == sorted(B['con'])))
for k, nm in (('pic', 'list/frame digests + counts'), ('dz', 'dz text (arm, 42, oth, pair, sub, fb, bias, w, aux)'), ('con', 'content digest'), ('natb', 'natb'), ('natz', 'natz'), ('natq', 'natq')):
    a, b = A[k], B[k]
    if not a or not b: print('%-52s: not in both logs (%d, %d)' % (nm, len(a), len(b))); continue
    t = collections.Counter(); bad = collections.defaultdict(list)
    for vb in sorted(b):
        c = 'first picture of a turn' if vb in first else 'arm %d' % arm(vb)
        t[c] += 1
        if a.get(vb) != b[vb]: bad[c].append(vb)
    print('%-52s: %d lines, differ %d' % (nm, len(b), sum(len(x) for x in bad.values())) + ''.join('; %s: %d of %d' % (c, len(bad.get(c, [])), t[c]) for c in sorted(t)))
    for c in sorted(bad): print('      %s: vb %d..%d, first %s' % (c, bad[c][0], bad[c][-1], bad[c][:8]))
