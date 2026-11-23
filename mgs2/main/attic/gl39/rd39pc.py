# rd39pc.py: read an MGS2 harness gl39 PSP run on the PC.   python -I rd39pc.py <LOGS\gl39 folder>
# Standard library only, one file.  gl39 is a measurement build: from vb 24,000 each turn of 100 vblanks has a KIND (K0 everything drawn,
# K5 the stage program off, K7 nothing drawn with the Media Engine resting, K8 nothing drawn in a Media Engine turn, with a load loop on
# the ME by MGS2_LOAD_CYCLE), F1 on or off, the frame digests on or off (README.txt has the table).  M5 is NOT in this EBOOT.  Prints: the end and
# crash lines; the hash and digest fingerprints against the emulator run of the same package; the steps before vb 24,000 by who drew
# (MAIN-DRAWING STEPS); the turn table; the comparisons by scene (the coupling: K8 with each load against K7; K0 digest-off against K7;
# digests, F1); the coupling probe lines (what the main CPU did for the queue and the GE, a turn); the problem counters.
# It also reads a gl38 log folder (the windows and the main-drawing steps print; the gl39 lines are then missing).
import sys, os, re, hashlib
A = sys.argv[1]
EH, NH = '66499cdb584a', 62          # hash lines 'vb ee iop' of the emulator run
ED, EDZ, ND = '360c72b5289c', '4bcc8be013bd', 266   # digest lines 'vb list frame' / with the dz text
SCHED = [(0, 24000, 0, 1, 0, 0, 0, 'deck, change of scene'), (1, 24100, 0, 1, 0, 0, 0, 'first deck scene'), (2, 24200, 7, 1, 0, 0, 1, 'first deck scene'), (3, 24300, 8, 1, 0, 0, 1, 'first deck scene'), (4, 24400, 0, 1, 0, 0, 1, 'first deck scene'), (5, 24500, 0, 1, 0, 0, 1, 'first deck scene'), (6, 24600, 7, 1, 0, 0, 1, 'first deck scene'), (7, 24700, 8, 1, 0, 0, 1, 'first deck scene'), (8, 24800, 0, 1, 0, 0, 1, 'first deck scene'), (9, 24900, 7, 1, 0, 0, 1, 'first deck scene'), (10, 25000, 0, 1, 0, 0, 1, 'deck, change of scene'), (11, 25100, 0, 1, 0, 0, 1, 'nothing drawn'), (12, 25200, 0, 1, 0, 0, 1, 'nothing drawn'), (13, 25300, 7, 1, 0, 0, 1, 'nothing drawn'), (14, 25400, 8, 1, 0, 0, 1, 'nothing drawn'), (15, 25500, 0, 1, 0, 0, 1, 'nothing drawn'), (16, 25600, 7, 1, 0, 0, 0, 'nothing drawn'), (17, 25700, 8, 1, 0, 3, 0, 'nothing drawn'), (18, 25800, 0, 1, 0, 0, 1, 'nothing drawn'), (19, 25900, 0, 1, 0, 0, 1, 'nothing drawn'), (20, 26000, 0, 1, 0, 0, 1, 'deck, change of scene'), (21, 26100, 0, 1, 0, 0, 1, 'deck, change of scene'), (22, 26200, 0, 1, 0, 0, 1, 'steady deck'), (23, 26300, 0, 1, 0, 0, 1, 'steady deck'), (24, 26400, 7, 1, 0, 0, 1, 'steady deck'), (25, 26500, 8, 1, 0, 0, 1, 'steady deck'), (26, 26600, 8, 1, 0, 1, 1, 'steady deck'), (27, 26700, 8, 1, 0, 2, 1, 'steady deck'), (28, 26800, 8, 1, 0, 3, 1, 'steady deck'), (29, 26900, 0, 1, 0, 0, 1, 'steady deck'), (30, 27000, 5, 1, 0, 0, 1, 'steady deck'), (31, 27100, 7, 1, 0, 0, 1, 'steady deck'), (32, 27200, 0, 1, 0, 0, 0, 'steady deck'), (33, 27300, 8, 1, 0, 0, 0, 'steady deck'), (34, 27400, 8, 1, 0, 1, 1, 'steady deck'), (35, 27500, 8, 1, 0, 2, 1, 'steady deck'), (36, 27600, 8, 1, 0, 3, 1, 'steady deck'), (37, 27700, 7, 1, 0, 0, 1, 'steady deck'), (38, 27800, 0, 1, 0, 0, 1, 'steady deck'), (39, 27900, 5, 1, 0, 0, 1, 'steady deck'), (40, 28000, 0, 1, 0, 0, 1, 'steady deck'), (41, 28100, 7, 1, 0, 0, 1, 'steady deck'), (42, 28200, 8, 1, 0, 0, 1, 'steady deck'), (43, 28300, 8, 1, 0, 3, 1, 'steady deck'), (44, 28400, 0, 1, 0, 0, 1, 'steady deck'), (45, 28500, 7, 1, 0, 0, 1, 'steady deck'), (46, 28600, 0, 1, 0, 0, 1, 'steady deck'), (47, 28700, 0, 1, 0, 0, 1, 'change of scene'), (48, 28800, 0, 1, 0, 0, 0, 'change of scene'), (49, 28900, 0, 1, 0, 0, 0, 'change of scene'), (50, 29000, 0, 1, 0, 0, 1, 'rain begins'), (51, 29100, 0, 1, 0, 0, 1, 'rain wait, first turns'), (52, 29200, 0, 1, 0, 0, 1, 'rain wait, first turns'), (53, 29300, 7, 1, 0, 0, 1, 'rain wait, first turns'), (54, 29400, 0, 1, 0, 0, 1, 'rain wait, first turns'), (55, 29500, 0, 1, 0, 0, 1, 'rain wait'), (56, 29600, 0, 1, 1, 0, 1, 'rain wait'), (57, 29700, 7, 1, 1, 0, 1, 'rain wait'), (58, 29800, 8, 1, 1, 0, 1, 'rain wait'), (59, 29900, 8, 1, 1, 1, 1, 'rain wait'), (60, 30000, 8, 1, 1, 2, 1, 'rain wait'), (61, 30100, 8, 1, 1, 3, 1, 'rain wait'), (62, 30200, 0, 1, 1, 0, 1, 'rain wait'), (63, 30300, 0, 0, 1, 0, 1, 'rain wait'), (64, 30400, 7, 1, 1, 0, 0, 'rain wait'), (65, 30500, 0, 0, 0, 0, 0, 'rain wait'), (66, 30600, 0, 1, 1, 0, 1, 'rain wait'), (67, 30700, 7, 1, 0, 0, 1, 'rain wait'), (68, 30800, 0, 1, 0, 0, 1, 'rain walk'), (69, 30900, 0, 1, 1, 0, 1, 'rain walk'), (70, 31000, 7, 1, 1, 0, 1, 'rain walk'), (71, 31100, 8, 1, 0, 3, 1, 'rain walk'), (72, 31200, 0, 1, 1, 0, 1, 'rain walk'), (73, 31300, 0, 1, 0, 0, 1, 'the walk ends')]   # (turn, vb, kind, F1, digest off, load, MGS2_OPT2's bit 18, scene)
SCHED = [(t, vb, k, f1, d, l, 1, sc) for (t, vb, k, f1, d, l, _b18, sc) in SCHED]   # bit 18 (M5) does nothing in this EBOOT: turns are not split by it
KN = {0: 'K0 all drawn', 5: 'K5 stage program off', 7: 'K7 nothing, ME rests', 8: 'K8 nothing, ME turn'}
LN = ['no load', 'arithmetic', 'cached reads', 'uncached writes']
def rd(d, f):
    p = os.path.join(d, f)
    return open(p, encoding='latin-1').read().splitlines() if os.path.exists(p) else []
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
def num(pat, s, d=None):
    m = re.search(pat, s); return float(m.group(1)) if m else d
la = rd(A, 'log.txt')
print('== END:', repr(la[-1]) if la else 'NO LOG', '| lines', len(la))
for s in la[-12:]:
    if s.startswith(('memory at the end', 'disc pack', 'stack')): print('  ', s[:240])
for s in la:
    if s.startswith(('bound: ', 'nat: gl38', 'picture: gl39', 'F1 totals', 'bound totals', 'native step 1 totals', 'natcull: totals')): print('  ', s[:700])
print('== CRASH:'); [print('  ', s[:200]) for s in rd(A, 'crash.txt')[:12]]
H, D = [], []
for s in la:
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append(('%s %s %s' % m.groups()[:3], m.group(4).strip()))
print('== HASHES (the game\'s state): %d lines, %s (expected %d, %s): %s' % (len(H), md(H), NH, EH, 'EQUAL' if md(H) == EH and len(H) == NH else 'DIFFERENT'))
if md(H) != EH: [print('  H', h) for h in H]
dd, ddz = md([a for a, _ in D]), md([a + ' ' + z for a, z in D])
print('== DIGESTS (the pictures\' lists in the digest-on turns): %d lines, %s / with dz %s (expected %d, %s / %s): %s' % (len(D), dd, ddz, ND, ED, EDZ,
      'EQUAL' if (dd, ddz) == (ED, EDZ) else 'DIFFERENT'))
T = {}
def tk(a): return T.setdefault(a, {})
PN = ['logic', 'iop', 'gs', 'pipeline', 'translator', 'picture', 'ge-wait', 'other']
for s in la:
    m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()[2:]]
        if int(m.group(2)) == 100: tk(int(m.group(1)) - 100).update(mean=g[0], logic=g[1], pipe=g[2], iop=g[3], gsw=g[4], xl=g[5], pic=g[6], gew=g[7], mx=g[8])
        continue
    m = re.match(r'me turn vb (\d+)-(\d+) \(([\d.]+) ms\): the ME.s own time \(ms\): records ([\d.]+), thinning ([\d.]+), fills ([\d.]+), walks ([\d.]+), hooks ([\d.]+) '
                 r'\(frame ends and lists ([\d.]+), GE waits ([\d.]+), the rest ([\d.]+)\), hand-over ([\d.]+), idle ([\d.]+), all ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()[2:]]; a = int(m.group(1)); n = max(1, int(m.group(2)) - a)
        tk(a).update(me_ms=g[0], me_rec=g[1] / n, me_walks=g[4] / n, me_lists=g[6] / n, me_gew=g[7] / n, me_idle=g[10] / n, me_busy=(g[11] - g[10] - g[7]) / n,
                     wfill=num(r'the main CPU waited \(ms\): copies as they ended [\d.]+, fills ([\d.]+)', s, 0) / n,
                     whook=num(r'waited at hooks for the walk before ([\d.]+) ms', s, 0) / n); continue
    m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl38 bound: turn (-?\d+) kind (\d) \(mask (\d+)\), F1 (\d)', s)
    if m:
        g = m.groups(); a = int(g[1]); tk(a).update(who=g[0], full=int(g[2]) - a == 100, turn=int(g[3]), kind=int(g[4]), f1=int(g[6])); continue
    m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl39 probe: turn (-?\d+) kind (\d) digest (on|off) load (\d); service calls by part \(calls/timed/us\):(.*?); of them from transfers:(.*?); '
                 r'GE polls (\d+), found done (\d+), lists to the GE (\d+); posts (\d+); whole-cache write-backs by part \(n/us\):(.*?); ME load: posted (\d+), not posted \(the loop behind\) (\d+), '
                 r'run (\d+), blocks and ms by flavour: 1: (\d+) ([\d.]+) 2: (\d+) ([\d.]+) 3: (\d+) ([\d.]+)', s)
    if m:
        g = m.groups(); a = int(g[1]); n = max(1, int(g[2]) - a); sv = {}; wb = {}
        for q in re.findall(r'([a-z-]+) (\d+)/(\d+)/(\d+)', g[7]): sv[q[0]] = (int(q[1]) / n, (int(q[3]) / int(q[2]) * int(q[1]) / 1000.0 / n) if int(q[2]) else None)
        for q in re.findall(r'([a-z-]+) (\d+)/(\d+)', g[13]): wb[q[0]] = (int(q[1]) / n, int(q[2]) / 1000.0 / n)
        tk(a).update(pwho=g[0], pdig=g[5], pload=int(g[6]), sv=sv, wb=wb, polls=int(g[9]) / n, pdone=int(g[10]) / n, lists=int(g[11]) / n, posts=int(g[12]) / n,
                     ldp=int(g[14]), ldskip=int(g[15]), ldrun=int(g[16]), ldblk=[0, int(g[17]), int(g[19]), int(g[21])], ldms=[0, float(g[18]) / n, float(g[20]) / n, float(g[22]) / n]); continue
    m = re.match(r'nat turn vb (\d+)-(\d+) (me|main) turn arm (\d+)(.*)', s)
    if m:
        r = m.group(5); a = int(m.group(1))
        prob = {k: num(p, r, 0) for k, p in (('builder', r'builder run instead \(dropped records\) (\d+)'), ('noarr', r'recorded with no array (\d+)'),
                ('early', r'early fills for an array.s reuse (\d+)'), ('drop', r'fed word by word \d+, dropped (\d+)'), ('partly', r'partly \(dropped\) (\d+)'))}
        tk(a).update(groups=num(r'calls recorded \d+, groups (\d+)', r), left=num(r'groups left untaken (\d+)', r, 0), prob={k: v for k, v in prob.items() if v}); continue
    m = re.match(r'nat f1 vb (\d+)-\d+ \w+ turn arm \d+ F1 (\d) \([^)]*\): runs planned (\d+) \(groups (\d+), of them not written (\d+), without the fast form (\d+)\), runs not planned (\d+) '
                 r'\(state (\d+), a group without an MSCAL (\d+)\), planned runs with a problem (\d+); check mode: groups compared (\d+), DIFFERING (\d+)', s)
    if m:
        g = [int(x) for x in m.groups()]; tk(g[0]).update(f1g=g[3], f1nw=g[4], f1prob=g[9], f1diff=g[11]); continue
def f(x, w='%.2f'): return '-' if x is None else (w % x if isinstance(x, float) else str(x))
def av(rows, k):
    v = [r[k] for r in rows if r.get(k) is not None]; return sum(v) / len(v) if v else None
print('== NOTE: every figure is ms a PSP step (a vblank) unless it says otherwise.  step = the 100-step window\'s mean; main CPU: logic (the step minus the timed parts),')
print('   pipe (the draw pipeline), gsw (GS walk), xl (translator), pic (picture), gew (GE wait); ME (its own time a step): busy (all but idle and GE waits), walks, lists, rec.')
# ---- the steps before the turns (vb < 24,000): the Media Engine draws in every other 100 vblanks, the main CPU in the others
print('== NOTE: M5 (the game kernel\'s interrupt walk, MGS2_OPT2 bit 18) is NOT in this EBOOT: the bit does nothing here, and the turns are not split by it.')
print('== MAIN-DRAWING STEPS before vb 24,000 (every other 100 vblanks the main CPU makes the picture itself: "main"; in the others the ME does: "me").  vb | who | step logic pipe gsw xl pic gew')
pre = [vb for vb in sorted(T) if vb < 24000 and T[vb].get('mean') is not None]
for vb in pre:
    x = T[vb]; who = x.get('who') or x.get('pwho') or ('main' if (vb // 100) & 1 else 'me')
    if vb >= 15000 or x['mean'] >= 20.0: print('  %5d | %-4s | %s' % (vb, who, ' '.join(f(x.get(c)) for c in ('mean', 'logic', 'pipe', 'gsw', 'xl', 'pic', 'gew'))))
for lo, hi, nm in ((15700, 16100, 'tanker bow (15,700-16,100)'), (20000, 24000, '20,000-24,000'), (0, 24000, 'all windows before 24,000')):
    for w in ('main', 'me'):
        rs = [T[vb] for vb in pre if lo <= vb < hi and (T[vb].get('who') or T[vb].get('pwho') or ('main' if (vb // 100) & 1 else 'me')) == w]
        if rs: print('  mean of %-28s %-4s n %3d | %s' % (nm, w, len(rs), ' '.join(f(av(rs, c)) for c in ('mean', 'logic', 'pipe', 'gsw', 'xl', 'pic', 'gew'))))
S = {vb: (t, k, f1, d, l, m5, sc) for (t, vb, k, f1, d, l, m5, sc) in SCHED}
def dg(vb): return 'none' if vb + 99 < 29490 else 'off' if S[vb][3] else 'on'
print('== TURNS  vb | t scene | kind F1 digests load | who | step logic pipe gsw xl pic gew | ME busy walks lists rec idle load | posts, whole-cache write-backs (n, ms), service calls in logic (n, est ms)')
bad = []
for vb in sorted(S):
    t, k, f1, d, l, m5, sc = S[vb]; x = T.get(vb, {})
    if x.get('kind') is not None and (x['kind'] != k or x.get('f1') != f1): bad.append((vb, 'kind/F1'))
    if x.get('pdig') is not None and ((x['pdig'] == 'off') != bool(d) or x['pload'] != l): bad.append((vb, 'digest/load'))
    wbn = sum(v[0] for v in x.get('wb', {}).values()) if x.get('wb') is not None else None; wbm = sum(v[1] for v in x.get('wb', {}).values()) if x.get('wb') is not None else None
    sl = x.get('sv', {}).get('logic', (None, None)) if x.get('sv') is not None else (None, None)
    print('%d | t%-2d %-22s | %-22s F%d dig %-4s %-15s | %-4s | %s | %s %s | %s %s %s, %s %s' % (vb, t, sc[:22], KN[k], f1, dg(vb), LN[l] if k == 8 else '-', x.get('who', '?'),
          ' '.join(f(x.get(c)) for c in ('mean', 'logic', 'pipe', 'gsw', 'xl', 'pic', 'gew')), ' '.join(f(x.get(c)) for c in ('me_busy', 'me_walks', 'me_lists', 'me_rec', 'me_idle')),
          f(sum(x['ldms']) if x.get('ldms') else None, '%.1f'), f(x.get('posts')), f(wbn), f(wbm, '%.3f'), f(sl[0]), f(sl[1], '%.3f')))
print('== SCHEDULE CHECK: turns whose logged kind, F1, digest or load is not the schedule\'s: %s' % (bad or 'none'))
COLS = ['mean', 'logic', 'pipe', 'gsw', 'xl', 'pic', 'gew', 'me_busy', 'me_lists']
HDR = 'step logic pipe gsw xl pic gew | ME busy lists'
def rows(scene, k, **kw):
    o = []
    for vb in sorted(S):
        t, kk, f1, d, l, m5, sc = S[vb]
        if sc != scene or kk != k or T.get(vb, {}).get('mean') is None: continue
        if any(v is not None and dict(f1=f1, d=dg(vb), l=l, m5=m5)[q] != v for q, v in kw.items()): continue
        o.append(T[vb])
    return o
def line(tag, rs):
    a = [av(rs, c) for c in COLS]
    print('  %-52s n %d | ' % (tag, len(rs)) + ' '.join(f(v) for v in a[:7]) + ' | ' + ' '.join(f(v) for v in a[7:]))
def delta(tag, rs, ref, note=''):
    if not rs or not ref: return
    a = [av(rs, c) for c in COLS]; b = [av(ref, c) for c in COLS]
    d = ['-' if x is None or y is None else '%+.2f' % (x - y) for x, y in zip(a, b)]
    print('  %-52s n %d - %d | %s | %s  %s' % (tag, len(rs), len(ref), ' '.join(d[:7]), ' '.join(d[7:]), note))
for scene in ('rain wait', 'rain wait, first turns', 'rain walk', 'steady deck', 'nothing drawn', 'first deck scene'):
    if not any(S[vb][6] == scene and T.get(vb, {}).get('mean') is not None for vb in S): continue
    print('== SCENE: %s.  Means, ms a PSP step: %s' % (scene, HDR))
    for k in (0, 5, 7, 8):
        for d in ('on', 'off', 'none'):
            for l in ((0, 1, 2, 3) if k == 8 else (0,)):
                for f1 in (1, 0):
                    for m5 in (1, 0):
                        rs = rows(scene, k, d=d, l=l, f1=f1, m5=m5)
                        if rs: line('%s, digests %s%s, F1 %s' % (KN[k], d, ', ' + LN[l] if k == 8 else '', 'on' if f1 else 'OFF'), rs)
    print('   differences (the first minus the second): %s' % HDR)
    for d in ('off', 'none'):
        k7 = rows(scene, 7, d=d, m5=1, f1=1)
        for l in (0, 1, 2, 3): delta('K8 %s minus K7 (the ME busy this way: logic = coupling)' % LN[l], rows(scene, 8, d=d, l=l), k7)
        delta('K8 uncached writes minus K8 no load', rows(scene, 8, d=d, l=3), rows(scene, 8, d=d, l=0))
        delta('K0 digests %s minus K7 (drawing\'s cost on the main CPU)' % d, rows(scene, 0, d=d, f1=1, m5=1), k7)
        delta('K0 digests %s minus K8 no load' % d, rows(scene, 0, d=d, f1=1, m5=1), rows(scene, 8, d=d, l=0))
        delta('K5 minus K0 (the stage program)', rows(scene, 5, d=d), rows(scene, 0, d=d, f1=1, m5=1))
    delta('K0 digests on minus off (the digest)', rows(scene, 0, d='on', f1=1, m5=1), rows(scene, 0, d='off', f1=1, m5=1))
    delta('K0 F1 OFF minus on (digests off)', rows(scene, 0, d='off', f1=0), rows(scene, 0, d='off', f1=1, m5=1))
    delta('K0 digests on, F1 OFF minus digests off, F1 on (gl38\'s K0 against gl39\'s play K0)', rows(scene, 0, d='on', f1=0), rows(scene, 0, d='off', f1=1))
print('== COUPLING PROBES by turn from vb 24,000 (a step\'s means): what the main CPU did for the queue and the GE, by the timed part it was in ("logic" = none: this time is inside')
print('   the step\'s logic figure).  service: calls a step and their estimated ms a step (one call in 8 timed); write-backs: whole-cache write-backs of a post, a step, and ms a step (all timed)')
for vb in sorted(S):
    x = T.get(vb, {})
    if x.get('sv') is None: continue
    t, k, f1, d, l, m5, sc = S[vb]
    print('  %d t%-2d K%d %-4s dig %-4s load %d | logic %s | service: %s | GE polls %s (done %s), lists %s, posts %s | write-backs: %s | ME load: posted %d, skipped %d, run %d, blocks %s, ms a step %s' % (
          vb, t, k, x.get('pwho', '?'), dg(vb), l, f(x.get('logic')), ', '.join('%s %.1f (%s ms)' % (p, x['sv'][p][0], f(x['sv'][p][1], '%.3f')) for p in PN if p in x['sv']) or '-',
          f(x['polls'], '%.1f'), f(x['pdone'], '%.1f'), f(x['lists'], '%.1f'), f(x['posts'], '%.1f'), ', '.join('%s %.1f (%.3f ms)' % (p, x['wb'][p][0], x['wb'][p][1]) for p in PN if p in x['wb']) or '-',
          x['ldp'], x['ldskip'], x['ldrun'], x['ldblk'][1:], ['%.1f' % v for v in x['ldms'][1:]]))
print('== PROBLEM COUNTERS (all should be none / 0):')
pr = [(vb, T[vb]['prob']) for vb in sorted(T) if T[vb].get('prob')]
print('  nat (builder, no array, early fills, dropped, partly):', pr or 'none')
print('  groups left untaken by turn (the emulator run of this package has the same):', [(vb, int(T[vb]['left'])) for vb in sorted(T) if T[vb].get('left')] or 'none')
print('  F1: planned runs with a problem %d, check DIFFERING %d;  ME load entries not posted (the loop was behind): %d' % (sum(T[vb].get('f1prob', 0) for vb in T),
      sum(T[vb].get('f1diff', 0) for vb in T), sum(T[vb].get('ldskip', 0) for vb in T if vb >= 24000)))
for s in la:
    if 'NAT PROBLEMS' in s or s.startswith('nat: PROBLEMS') or 'STOPPED' in s or 'TAKEN BACK' in s or 'take-back' in s and 'none' not in s: print('  ', s[:300])
b = rd(A, 'battery.txt')
if b: print('== BATTERY: first', b[0][:120], '| last', b[-1][:120])
