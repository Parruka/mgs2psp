# kcmp.py <reference log> <kill log>: the x86 twin with MGS2_KILL_ACT against its reference (task #164): the state and sound hashes
# every 100 vblanks (first difference, how many differ after), the vblanks of the game's thread starts (area changes), the frames'
# counts in the rainy aft deck window, and the kill lines
import re, sys
def rd(p):
    H = {}; S = []; F = {}; K = []
    for l in open(p, errors='replace'):
        m = re.match(r'hash: vb (\d+) ee (\w+) iop (\w+)', l)
        if m: H[int(m.group(1))] = (m.group(2), m.group(3)); continue
        m = re.match(r'\[(\d+) vb, th \d+\] sys  40 CreateSema', l)
        if m: S.append(int(m.group(1))); continue
        m = re.match(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+): (\d+) words, (\d+) aux bytes, (\d+) batches, (\d+) PRIMs, (\d+) textures, (\d+) dropped', l)
        if m: F[int(m.group(1))] = tuple(int(x) for x in m.groups()[3:]); continue
        if l.startswith('kill:'): K.append(l.rstrip())
    return H, S, F, K
H0, S0, F0, _ = rd(sys.argv[1]); H1, S1, F1, K1 = rd(sys.argv[2])
vbs = sorted(set(H0) & set(H1))
fe = next((v for v in vbs if H0[v][0] != H1[v][0]), None); fi = next((v for v in vbs if H0[v][1] != H1[v][1]), None)
print('hash lines %d / %d; ee first differs at vb %s; iop first differs at vb %s' % (len(H0), len(H1), fe, fi))
if fe is not None:
    same = [v for v in vbs if v > fe and H0[v][0] == H1[v][0]]; print('ee equal again at (%d):' % len(same), same[:20])
if fi is not None:
    d = [v for v in vbs if v >= fi and H0[v][1] != H1[v][1]]; print('iop lines that differ from there: %d of %d (%s ...)' % (len(d), len([v for v in vbs if v >= fi]), d[:12]))
    same = [v for v in vbs if v > fi and H0[v][1] == H1[v][1]]; print('iop equal again at:', same[:20])
print('thread starts (area changes): ref', sorted(set(S0)), 'kill', sorted(set(S1)))
print('frames: ref %d kill %d' % (len(F0), len(F1)))
for a, b in ((29810, 29890), (30810, 30890), (33410, 33490)):
    w0 = [F0[v] for v in F0 if a <= v < b]; w1 = [F1[v] for v in F1 if a <= v < b]
    s = lambda w, i: sum(x[i] for x in w)
    print('window %d-%d: frames %d / %d; words %d / %d; batches %d / %d; PRIMs %d / %d; textures %d / %d' % (a, b, len(w0), len(w1), s(w0, 0), s(w1, 0), s(w0, 2), s(w1, 2), s(w0, 3), s(w1, 3), s(w0, 4), s(w1, 4)))
for k in K1:
    if 'first' in k: print(k)
print([k for k in K1 if k.startswith('kill: vb 37700')])
