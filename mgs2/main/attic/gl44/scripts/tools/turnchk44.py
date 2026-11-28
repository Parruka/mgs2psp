"""turnchk44.py LOG (gl44, python3 -I): each of gl44's 74 turns in a log (the PSP's, the emulator's, or the twin's) against tools/sched44.py:
the OPT2 word in force ('gl41 levers vb 24100+100t: opt2 W', the PSP only), and the turn lines of B ('b43 turn vb a-b: B on/off') and V44
('v44 turn vb a-b: V44 on/off; line_sph stubbed S, by G0 G; rain.c entries skipped K, in place and run R; short tails T, placed first P'),
with a = 24001 + 100 t (t73's line comes at the next turn: the PSP's run ends at 31,400 without it).  A V turn must show V44 on, S > 0, G = 0, K > 0, R = 0; a turn without V: V44 off, S = 0, K = 0; T = 0 always.
A B turn must show B on, a turn without B (rain turns) B off.  Prints the table and the mismatches."""
import sys, os, re, gzip
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched44 as S
p = sys.argv[1]; f = gzip.open(p, 'rt', errors='replace') if p.endswith('.gz') else open(p, errors='replace')
LV = {}; B = {}; V = {}
for l in f:
    m = re.match(r'gl41 levers vb (\d+): opt2 ([0-9a-f]{8})', l)
    if m: LV[int(m.group(1))] = m.group(2); continue
    m = re.match(r'b43 turn vb (\d+)-(\d+): B (on|off)', l)
    if m: B[int(m.group(1))] = m.group(3); continue
    m = re.match(r'v44 turn vb (\d+)-(\d+): V44 (on|off); line_sph stubbed (\d+), by G0 (\d+); rain.c entries skipped (\d+), in place and run (\d+); short tails (\d+), placed first (\d+)', l)
    if m: V[int(m.group(1))] = (m.group(3),) + tuple(int(x) for x in m.group(4, 5, 6, 7, 8, 9))
bad = []; nw = nb = nv = 0; tot = [0] * 6
print('turn kind word(sched) word(log) B(log) V44(log) stubbed byG0 skipped run tails first')
for t in range(S.N):
    k = S.K[t]; w = S.opt2(k); a = 24001 + 100 * t; lw = LV.get(a + 99, '-'); b = B.get(a, '-'); v = V.get(a)
    if lw != '-':
        nw += 1
        if lw != w: bad.append((t, 'word %s, the schedule %s' % (lw, w)))
    isB = k in 'BCDbcd'; isV = k in 'bcd'
    if b != '-':
        nb += 1
        if (b == 'on') != isB: bad.append((t, 'B %s' % b))
    if v:
        nv += 1; tot = [x + y for x, y in zip(tot, v[1:])]
        if (v[0] == 'on') != isV: bad.append((t, 'V44 %s' % v[0]))
        if isV and not (v[1] > 0 and v[2] == 0 and v[3] > 0 and v[4] == 0): bad.append((t, 'V turn counters %s' % (v[1:],)))
        if not isV and (v[1] or v[3]): bad.append((t, 'stubbed or skipped without V %s' % (v[1:],)))
        if v[5]: bad.append((t, 'short tails %d' % v[5]))
    elif isV and t >= 50 and t < S.N - 1: bad.append((t, 'no v44 turn line'))   # (the last turn's line comes at the next turn: none on the PSP, whose run ends at 31,400)
    if t >= 49 or v: print('t%-3d %s %s %s %-3s %-3s %s' % (t, k, w, lw, b, v[0] if v else '-', ' '.join(str(x) for x in v[1:]) if v else ''))
print('turns with the OPT2 word logged %d, with a b43 line %d, with a v44 line %d; v44 totals: stubbed %d, by G0 %d, skipped %d, in place and run %d, short tails %d, placed first %d' % ((nw, nb, nv) + tuple(tot)))
print('turnchk44: %s' % ('PASS' if not bad else 'FAIL %d: %s' % (len(bad), bad[:20])))
