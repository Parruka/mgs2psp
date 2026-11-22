# rd38pc.py: read an MGS2 harness gl38 PSP run on the PC.   python -I rd38pc.py <LOGS\gl38 folder>
# Standard library only, one file.  gl38 is a measurement build: from vb 24,000 each turn of 100 vblanks has a KIND (K0 everything drawn,
# K1..K7 one kind of drawing left out; README.txt has the table).  Prints: the end and crash lines; the hash and digest fingerprints
# against the emulator run of the same package; the turn table; per scene and processor each kind's ms a PSP step (a vblank) against the
# K0 turns of the same scene on the same run; F1 on against off; the new timers (the ME's hooks, list builds and records by part, a
# people group by stage); the ME's time by chain; the problem counters.
import sys, os, re, hashlib
A = sys.argv[1]
EH, NH = '66499cdb584a', 62          # hash lines 'vb ee iop' of the emulator run
ED, EDZ, ND = '9ae227a44404', '4c447e32a4ac', 916   # digest lines 'vb list frame' / with the dz text
SCHED = [(0, 24000, 0, 1, 0, 'deck, change of scene'), (1, 24100, 0, 1, 0, 'first deck scene'), (2, 24200, 3, 0, 0, 'first deck scene'), (3, 24300, 4, 0, 0, 'first deck scene'), (4, 24400, 0, 1, 1, 'first deck scene'), (5, 24500, 1, 1, 0, 'first deck scene'), (6, 24600, 2, 0, 0, 'first deck scene'), (7, 24700, 0, 0, 0, 'first deck scene'), (8, 24800, 6, 1, 0, 'first deck scene'), (9, 24900, 7, 1, 0, 'first deck scene'), (10, 25000, 0, 0, 0, 'deck, change of scene'), (11, 25100, 0, 0, 0, 'nothing drawn'), (12, 25200, 0, 1, 0, 'nothing drawn'), (13, 25300, 7, 1, 0, 'nothing drawn'), (14, 25400, 0, 0, 0, 'nothing drawn'), (15, 25500, 0, 0, 0, 'nothing drawn'), (16, 25600, 7, 1, 0, 'nothing drawn'), (17, 25700, 0, 1, 0, 'nothing drawn'), (18, 25800, 0, 0, 0, 'nothing drawn'), (19, 25900, 0, 0, 0, 'nothing drawn'), (20, 26000, 0, 1, 0, 'deck, change of scene'), (21, 26100, 0, 1, 0, 'deck, change of scene'), (22, 26200, 0, 0, 0, 'steady deck'), (23, 26300, 0, 0, 0, 'steady deck'), (24, 26400, 0, 1, 0, 'steady deck'), (25, 26500, 5, 1, 0, 'steady deck'), (26, 26600, 7, 0, 0, 'steady deck'), (27, 26700, 0, 0, 0, 'steady deck'), (28, 26800, 5, 1, 0, 'steady deck'), (29, 26900, 7, 1, 0, 'steady deck'), (30, 27000, 0, 0, 0, 'steady deck'), (31, 27100, 0, 0, 0, 'steady deck'), (32, 27200, 5, 1, 0, 'steady deck'), (33, 27300, 7, 1, 0, 'steady deck'), (34, 27400, 0, 0, 0, 'steady deck'), (35, 27500, 5, 0, 0, 'steady deck'), (36, 27600, 7, 1, 0, 'steady deck'), (37, 27700, 0, 1, 0, 'steady deck'), (38, 27800, 0, 0, 0, 'steady deck'), (39, 27900, 5, 0, 0, 'steady deck'), (40, 28000, 7, 1, 0, 'steady deck'), (41, 28100, 0, 1, 0, 'steady deck'), (42, 28200, 5, 0, 0, 'steady deck'), (43, 28300, 7, 0, 0, 'steady deck'), (44, 28400, 0, 1, 0, 'steady deck'), (45, 28500, 5, 1, 0, 'steady deck'), (46, 28600, 7, 0, 0, 'steady deck'), (47, 28700, 0, 0, 0, 'change of scene'), (48, 28800, 0, 1, 0, 'change of scene'), (49, 28900, 0, 1, 0, 'change of scene'), (50, 29000, 0, 0, 0, 'rain begins'), (51, 29100, 0, 0, 0, 'rain wait, first turns'), (52, 29200, 6, 1, 0, 'rain wait, first turns'), (53, 29300, 1, 1, 0, 'rain wait, first turns'), (54, 29400, 0, 0, 0, 'rain wait, first turns'), (55, 29500, 2, 0, 0, 'rain wait'), (56, 29600, 3, 1, 0, 'rain wait'), (57, 29700, 4, 1, 0, 'rain wait'), (58, 29800, 0, 0, 0, 'rain wait'), (59, 29900, 0, 0, 1, 'rain wait'), (60, 30000, 0, 1, 0, 'rain wait'), (61, 30100, 0, 1, 1, 'rain wait'), (62, 30200, 5, 0, 0, 'rain wait'), (63, 30300, 1, 0, 0, 'rain wait'), (64, 30400, 6, 1, 0, 'rain wait'), (65, 30500, 7, 1, 0, 'rain wait'), (66, 30600, 3, 0, 0, 'rain wait'), (67, 30700, 7, 0, 0, 'rain wait'), (68, 30800, 0, 1, 0, 'rain walk'), (69, 30900, 7, 1, 0, 'rain walk'), (70, 31000, 4, 0, 1, 'rain walk'), (71, 31100, 2, 0, 0, 'rain walk'), (72, 31200, 6, 1, 0, 'rain walk'), (73, 31300, 0, 1, 0, 'the walk ends')]   # (turn, vb, kind, re-use, F1, scene)
KN = ['K0 all drawn', 'K1 ship off', 'K2 people off', 'K3 sorted off (rain kept)', 'K4 rain off but the near sheet', 'K5 stage program off',
      'K6 ship+people+sorted+rain off', 'K7 nothing drawn, ME rests']
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
    if s.startswith(('bound: ', 'nat: gl38', 'F1 totals', 'bound totals', 'native step 1 totals', 'reuse totals', 'reuse rule totals', 'natcull: totals')): print('  ', s[:700])
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
print('== DIGESTS (the pictures\' lists, every kind\'s turns): %d lines, %s / with dz %s (expected %d, %s / %s): %s' % (len(D), dd, ddz, ND, ED, EDZ,
      'EQUAL' if (dd, ddz) == (ED, EDZ) else 'DIFFERENT'))
T = {}
def tk(a): return T.setdefault(a, {})
for s in la:
    m = re.match(r'window vb (\d+): 100 steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()[1:]]
        tk(int(m.group(1)) - 100).update(mean=g[0], logic=g[1], pipe=g[2], iop=g[3], gsw=g[4], pic=g[6], gew=g[7], mx=g[8]); continue
    m = re.match(r'me turn vb (\d+)-(\d+) \(([\d.]+) ms\): the ME.s own time \(ms\): records ([\d.]+), thinning ([\d.]+), fills ([\d.]+), walks ([\d.]+), hooks ([\d.]+) '
                 r'\(frame ends and lists ([\d.]+), GE waits ([\d.]+), the rest ([\d.]+)\), hand-over ([\d.]+), idle ([\d.]+), all ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()[2:]]; a = int(m.group(1)); n = max(1, int(m.group(2)) - a)
        tk(a).update(me_ms=g[0], me_rec=g[1] / n, me_walks=g[4] / n, me_lists=g[6] / n, me_gew=g[7] / n, me_idle=g[10] / n, me_busy=(g[11] - g[10] - g[7]) / n,
                     wfill=num(r'the main CPU waited \(ms\): copies as they ended [\d.]+, fills ([\d.]+)', s, 0) / n,
                     whook=num(r'waited at hooks for the walk before ([\d.]+) ms', s, 0) / n); continue
    m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl38 bound: turn (-?\d+) kind (\d) \(mask (\d+)\), F1 (\d); frames begun with a mask (\d+); transfers left out by chain:(.*?); '
                 r'native groups recorded and not drawn (\d+); rain owners. calls (\d+) \(sheets (\d+)\), left out (\d+), near sheet calls kept (\d+); chain 6.s other calls '
                 r'left out (\d+), its GS primitives (\d+); rain owners. blocks over the table (\d+), not right (\d+)', s)
    if m:
        g = m.groups(); a = int(g[1])
        tk(a).update(who=g[0], full=int(g[2]) - a == 100, turn=int(g[3]), kind=int(g[4]), f1=int(g[6]), bfr=int(g[7]), btr=g[8].strip(), natg=int(g[9]), rain=int(g[10]),
                     sheets=int(g[11]), rainout=int(g[12]), near=int(g[13]), c6out=int(g[14]), primout=int(g[15]), over=int(g[16]), notright=int(g[17])); continue
    m = re.match(r'nat turn vb (\d+)-(\d+) (me|main) turn arm (\d+)(.*)', s)
    if m:
        r = m.group(5); a = int(m.group(1))
        prob = {k: num(p, r, 0) for k, p in (('builder', r'builder run instead \(dropped records\) (\d+)'), ('noarr', r'recorded with no array (\d+)'),
                ('early', r'early fills for an array.s reuse (\d+)'), ('drop', r'fed word by word \d+, dropped (\d+)'), ('partly', r'partly \(dropped\) (\d+)'))}
        tk(a).update(arm=int(m.group(4)), groups=num(r'calls recorded \d+, groups (\d+)', r), left=num(r'groups left untaken (\d+)', r, 0),
                     nat=num(r'native time ([\d.]+)', r), prob={k: v for k, v in prob.items() if v}); continue
    m = re.match(r'nat reuse vb (\d+)-\d+ \w+ turn arm \d+ reuse (\d+) \([^)]*\): runs replayed (\d+) \(groups (\d+), skipping (\d+), in S (\d+)\), recorded (\d+) \(groups (\d+);.*?'
                 r'S differing (\d+), boxes differing (\d+)\).*?replays with a group not taken (\d+)', s)
    if m:
        g = [int(x) for x in m.groups()]; tk(g[0]).update(ru=g[1], urg=g[3], urecg=g[7], ubad=g[8] + g[9] + g[10]); continue
    m = re.match(r'nat f1 vb (\d+)-\d+ \w+ turn arm \d+ F1 (\d) \([^)]*\): runs planned (\d+) \(groups (\d+), of them not written (\d+), without the fast form (\d+)\), runs not planned (\d+) '
                 r'\(state (\d+), a group without an MSCAL (\d+)\), planned runs with a problem (\d+); check mode: groups compared (\d+), DIFFERING (\d+), written by the plan and not '
                 r'by the notes (\d+)', s)
    if m:
        g = [int(x) for x in m.groups()]; tk(g[0]).update(f1g=g[3], f1nw=g[4], f1bar=g[5], f1np=g[6], f1prob=g[9], f1cmp=g[10], f1diff=g[11]); continue
    m = re.match(r'me turn vb (\d+)-(\d+) gl38 timers \([^)]*\): hooks (\d+), lists (\d+): frame ends ([\d.]+), frame ends and lists ([\d.]+), results and cache out ([\d.]+), frame starts '
                 r'([\d.]+), texture cache.s frame ([\d.]+); records (\d+): out of the cache ([\d.]+), extents copied ([\d.]+), native groups copied ([\d.]+); the list build by part, in '
                 r'(\d+) of the lists \(one in \d+; a clock read at each change of the batches. kind, (\d+) of them; ms in those lists\): depth ([\d.]+), set-up and clear ([\d.]+), GS '
                 r'primitives. batches ([\d.]+) \((\d+)\), ship batches ([\d.]+) \((\d+)\), effects. batches ([\d.]+) \((\d+)\), people.s batches ([\d.]+) \((\d+)\), list.s end ([\d.]+), '
                 r'digests ([\d.]+), sum ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()]; a = int(g[0]); n = max(1.0, g[1] - g[0]); sc = (g[3] / g[13] if g[13] else 0.0) / n   # a timed list's ms -> all the lists', a step
        tk(a).update(tm=dict([('frame ends', g[4] / n), ('frame ends and lists', g[5] / n), ('results and cache out', g[6] / n), ('frame starts', g[7] / n),
            ('texture cache frame', g[8] / n), ('rec: out of the cache', g[10] / n), ('rec: extents copied', g[11] / n), ('rec: native groups copied', g[12] / n),
            ('list: depth', g[15] * sc), ('list: set-up and clear', g[16] * sc), ('list: GS primitives', g[17] * sc), ('list: ship', g[19] * sc), ('list: effects', g[21] * sc),
            ('list: people', g[23] * sc), ('list: end', g[25] * sc), ('list: digests', g[26] * sc), ('list: sum', g[27] * sc)]),
            tmn=dict(lists=g[3], timed=g[13], changes=g[14], records=g[9], b_gs=g[18], b_ship=g[20], b_fx=g[22], b_people=g[24])); continue
    m = re.match(r'people split vb (\d+)-\d+ me turn \([^)]*\): lane 2.s groups in the timed walks (\d+), of them timed at the walk (\d+), at their flush (\d+); us a timed group by stage '
                 r'\(each holds one read\); at the walk:(.*?), sum ([\d.]+); at the flush:(.*?), sum ([\d.]+)', s)
    if m:
        def st(x):
            o = []
            for y in x.split(','):
                y = y.strip(); k = y.rfind(' ')
                if k > 0:
                    try: o.append((y[:k], float(y[k + 1:])))
                    except ValueError: pass
            return o
        w = st(m.group(5)); f = st(m.group(7))
        # (the stage names hold commas: 'to the hold: test, GS state and texture' and 'strips, GE data and passes' records' and 'header, bones and lights')
        tk(int(m.group(1))).update(pp=dict(seen=int(m.group(2)), walk=int(m.group(3)), flush=int(m.group(4)), w=w, ws=float(m.group(6)), f=f, fs=float(m.group(8)))); continue
    m = re.match(r'nat split vb (\d+)-\d+ me turn arm \d+ fast (\d+) \(gl3[0-9]; ticks a ms (\d+)\): groups in the timed merge runs (\d+), of them timed by stage (\d+) '
                 r'\(vertices (\d+), left out by the cut (\d+)\); clock read pairs (\d+), mean ([\d.]+) us a read; us a timed group by stage \(each holds one read\):'
                 r'(.*?); sum ([\d.]+); the turn.s native time ([\d.]+) ms = ([\d.]+) us a group', s)
    if m:
        g = m.groups(); stg = []
        for x in g[9].split(','):
            x = x.strip(); k = x.rfind(' '); stg.append((x[:k], float(x[k + 1:])))
        tk(int(g[0])).update(sp=dict(ticks=int(g[2]), seen=int(g[3]), timed=int(g[4]), read=float(g[8]), st=stg, sum=float(g[10]), nat=float(g[11]), natg=float(g[12]))); continue
    m = re.match(r'me chains vb (\d+)-(\d+) me turn arm \d+ fast (\d+) \([^)]*\): lists read (\d+), transfers in a list (\d+), not in one (\d+); chain: transfers, ms, '
                 r'converter calls, native ms \(groups\):(.*); the ME.s walks ([\d.]+) ms', s)
    if m:
        c = {}; n = max(1, int(m.group(2)) - int(m.group(1)))
        for x in m.group(7).split(';'):
            mm = re.match(r'(.*?): (\d+), ([\d.]+), (\d+), ([\d.]+) \((\d+)\)$', x.strip())
            if mm:
                k = mm.group(1); k = 'ends' if k.startswith('the transfers') else 'other' if k.startswith('other') else k
                c[k] = (float(mm.group(3)) / n, int(mm.group(4)))
        tk(int(m.group(1))).update(ch=c)
S = {vb: (t, k, r, f, sc) for (t, vb, k, r, f, sc) in SCHED}
def f(x, w='%.2f'): return '-' if x is None else (w % x if isinstance(x, float) else str(x))
def av(rows, k):
    v = [r[k] for r in rows if r.get(k) is not None]; return sum(v) / len(v) if v else None
print('== NOTE: every figure is ms a PSP step (a vblank) unless it says otherwise.  step = the window\'s mean; main CPU: logic, pipe (the draw pipeline), gsw (GS walk), gew (GE wait);')
print('   ME (its own time a step): busy (all but idle and GE waits), walks, lists (frame ends and lists), rec (records).  A K7 turn is a main-CPU turn: the ME rests, no ME figures.')
print('== TURNS  vb | t scene | kind (log) R F1 who | step logic pipe gsw gew | ME busy walks lists rec | main waited: fills, hooks | ship groups, F1 not written, left out')
bad = []
for vb in sorted(S):
    t, k, r, f1, sc = S[vb]; x = T.get(vb, {})
    if x.get('kind') is not None and (x['kind'] != k or x.get('f1') != f1): bad.append(vb)
    if x.get('prob') or x.get('left') or x.get('ubad') or x.get('f1prob') or x.get('f1diff') or x.get('notright'): pass
    out = ('transfers ' + x['btr'] if x.get('btr') else '') + (' natg %d' % x['natg'] if x.get('natg') else '') + (' rain %d of %d, near kept %d' % (x['rainout'], x['rain'], x['near'])
          if x.get('rain') else '') + (' chain6 %d' % x['c6out'] if x.get('c6out') else '') + (' OVER THE TABLE %d' % x['over'] if x.get('over') else '')
    print('%d | t%-2d %-22s | K%d (%s) R%d F%d %-4s | %s %s %s %s %s | %s %s %s %s | %s %s | %s %s |%s' % (vb, t, sc[:22], k, f(x.get('kind')), r, f1, x.get('who', '?'),
          f(x.get('mean')), f(x.get('logic')), f(x.get('pipe')), f(x.get('gsw')), f(x.get('gew')), f(x.get('me_busy')), f(x.get('me_walks')), f(x.get('me_lists')), f(x.get('me_rec')),
          f(x.get('wfill')), f(x.get('whook')), f(x.get('groups'), '%.0f'), f(x.get('f1nw')), out))
print('== SCHEDULE CHECK: turns whose logged kind or F1 is not the schedule\'s: %s' % (bad or 'none'))
COLS = ['mean', 'logic', 'pipe', 'gsw', 'gew', 'me_busy', 'me_walks', 'me_lists', 'me_rec']
HDR = 'step logic pipe gsw gew | ME busy walks lists rec'
def rows(scene, k, r=None, f1=None):
    return [T[vb] for vb in sorted(S) if S[vb][4] == scene and S[vb][1] == k and (r is None or S[vb][2] == r) and (f1 is None or S[vb][3] == f1) and T.get(vb, {}).get('mean') is not None]
def line(tag, rs):
    a = [av(rs, c) for c in COLS]
    return '  %-34s n %d | ' % (tag, len(rs)) + ' '.join(f(v) for v in a[:5]) + ' | ' + ' '.join(f(v) for v in a[5:])
def delta(tag, rs, ref, note=''):
    a = [av(rs, c) for c in COLS]; b = [av(ref, c) for c in COLS]
    d = ['-' if x is None or y is None else '%+.2f' % (x - y) for x, y in zip(a, b)]
    print('  %-34s n %d (K0 turns %d) | %s | %s  %s' % (tag, len(rs), len(ref), ' '.join(d[:5]), ' '.join(d[5:]), note))
for scene, byr in (('rain wait, first turns', 0), ('rain wait', 1), ('rain walk', 0), ('steady deck', 0), ('first deck scene', 0), ('nothing drawn', 0)):
    k0 = rows(scene, 0, None, 0)
    if not k0 and not rows(scene, 0): continue
    print('== SCENE: %s.  Means, ms a PSP step: %s' % (scene, HDR))
    if scene == 'rain wait, first turns': print('   (29,100-29,500: the scene still settles, and the frame digests -- an ME cost in every later rain turn, see NEW TIMERS list: digests -- only begin at 29,490, so these turns have their own K0 turns, at both ends)')
    if scene == 'rain wait': print('   (29,500-30,800, frame digests in every turn; the K0 turns are in the middle, 29,800-30,200; the last two turns 30,600-30,800 are second copies in a heavier part of the scene)')
    for k in range(8):
        for r in ((0, 1) if byr and k in (0, 2, 3, 4) else (None,)):
            for f1 in ((0, 1) if k == 0 or scene == 'rain walk' else (None,)):
                rs = rows(scene, k, r, f1)
                if rs: print(line('%s%s%s' % (KN[k], '' if r is None else ' R%d' % r, '' if f1 is None else ' F1 %d' % f1), rs))
    print('   each kind MINUS the K0 turns of this scene (F1 0; with the same re-use arm where the kind draws the ship in the rain wait): %s' % HDR)
    for k in range(1, 8):
        for r in ((0, 1) if byr and k in (2, 3, 4) else (None,)):
            rs = rows(scene, k, r)
            if not rs: continue
            if byr and k == 5: r2 = S[[vb for vb in S if S[vb][4] == scene and S[vb][1] == 5][0]][2]; ref = rows(scene, 0, r2, 0); note = '(K0 R%d)' % r2
            else: ref = rows(scene, 0, r, 0); note = ''
            if k == 7: note += ' (the ME rests: its columns are what the ME no longer does; step and the main columns are the coupling)'
            if ref: delta('%s%s' % (KN[k], '' if r is None else ' R%d' % r), rs, ref, note)
    for r in ((0, 1) if byr else (None,)):
        a, b = rows(scene, 0, r, 1), rows(scene, 0, r, 0)
        if a and b: delta('F1 on minus off, K0%s' % ('' if r is None else ' R%d' % r), a, b)
    if scene in ('steady deck', 'rain wait, first turns', 'rain wait'):
        print('   K0 turns one by one (the kind of the turn before: the carry-over of a K7 or K5 turn; and the scene\'s own drift):')
        for vb in sorted(S):
            if S[vb][4] == scene and S[vb][1] == 0 and T.get(vb, {}).get('mean') is not None:
                x = T[vb]; p = S.get(vb - 100, (0, 0))[1]
                print('    vb %d after K%d: %s' % (vb, p, ' '.join(f(x.get(c)) for c in COLS)))
print('== SHIP GROUPS, us a native group on the ME (the turn\'s native time / groups) and the nat split stages (us a timed group), rain turns that draw the ship:')
for vb in sorted(S):
    x = T.get(vb, {}); sp = x.get('sp')
    if sp and sp['seen'] and S[vb][4].startswith('rain'):
        d = dict(sp['st'])
        print('  %d K%d R%d F1 %d: groups %d, %.2f us a group; register %.2f, walk and pointers %.2f, A+D %.2f, host call %.2f, texture %.2f, positions %.2f; sum of stages %.2f; F1: groups not written %s of %s planned'
              % (vb, S[vb][1], S[vb][2], S[vb][3], sp['seen'], sp['natg'], d.get('register', 0), d.get('walk and pointers', 0), d.get('A+D packet', 0), d.get('host call and GS state', 0),
                 d.get('texture', 0), d.get('positions', 0), sp['sum'], f(x.get('f1nw')), f(x.get('f1g'))))
print('== NEW TIMERS, the ME\'s own ms a PSP step by part, means over each scene\'s turns of a kind (list parts: from the lists timed, one in 4, scaled to all):')
for scene in ('rain wait, first turns', 'rain wait', 'rain walk', 'steady deck', 'first deck scene'):
    for k in range(7):
        rs = [T[vb] for vb in sorted(S) if S[vb][4] == scene and S[vb][1] == k and T.get(vb, {}).get('tm')]
        if not rs: continue
        keys = list(rs[0]['tm'])
        print('  %s, %s (n %d): %s' % (scene, KN[k], len(rs), '; '.join('%s %.2f' % (q, sum(r['tm'][q] for r in rs) / len(rs)) for q in keys)))
print('== PEOPLE SPLIT, us a timed people group (lane 2, P06) by stage, means over the scene\'s K0 turns (each stage holds one clock read):')
for scene in ('rain wait, first turns', 'rain wait', 'rain walk', 'first deck scene'):
    rs = [T[vb]['pp'] for vb in sorted(S) if S[vb][4] == scene and S[vb][1] == 0 and T.get(vb, {}).get('pp') and T[vb]['pp']['walk']]
    if not rs: continue
    n = len(rs)
    print('  %s (n %d turns; groups a turn %d, timed at the walk %d, at the flush %d): at the walk sum %.2f, at the flush sum %.2f' % (scene, n, sum(r['seen'] for r in rs) / n,
          sum(r['walk'] for r in rs) / n, sum(r['flush'] for r in rs) / n, sum(r['ws'] for r in rs) / n, sum(r['fs'] for r in rs) / n))
print('   (the stages, one line a rain K0 turn, as logged:)')
for s in la:
    m = re.match(r'people split vb (\d+)-', s)
    if m and S.get(int(m.group(1)), (0, 9, 0, 0, ''))[1] == 0 and S[int(m.group(1))][4].startswith('rain'): print('   ', s[s.find('at the walk:'):][:520])
print('== THE ME\'S WALKS BY CHAIN, ms a PSP step (calls a turn), means over each scene\'s K0 turns (a chain is a transfer of the frame\'s list; its number is its place, so chain N is not the same thing in every scene):')
for scene in ('rain wait, first turns', 'rain wait', 'rain walk', 'steady deck', 'first deck scene'):
    rs = [T[vb]['ch'] for vb in sorted(S) if S[vb][4] == scene and S[vb][1] == 0 and T.get(vb, {}).get('ch')]
    if not rs: continue
    ks = sorted(set(k for r in rs for k in r), key=lambda q: (not q.isdigit(), int(q) if q.isdigit() else 0, q))
    print('  %s (n %d): %s' % (scene, len(rs), '; '.join('%s: %.2f (%d)' % (k, sum(r.get(k, (0, 0))[0] for r in rs) / len(rs), sum(r.get(k, (0, 0))[1] for r in rs) / len(rs)) for k in ks
          if sum(r.get(k, (0, 0))[0] for r in rs) / len(rs) >= 0.05)))
print('== PROBLEM COUNTERS (all should be none / 0):')
pr = [(vb, T[vb]['prob']) for vb in sorted(T) if T[vb].get('prob')]
print('  nat (builder, no array, early fills, dropped, partly):', pr or 'none')
print('  groups left untaken by turn (the emulator run of this package has the same):', [(vb, int(T[vb]['left'])) for vb in sorted(T) if T[vb].get('left')] or 'none')
print('  re-use (S / boxes differing, replays with a group not taken): %d;  F1: planned runs with a problem %d, check DIFFERING %d;  bound: rain owners\' blocks not right %d, over the table %d (README: the first deck scene)'
      % (sum(T[vb].get('ubad', 0) for vb in T), sum(T[vb].get('f1prob', 0) for vb in T), sum(T[vb].get('f1diff', 0) for vb in T), sum(T[vb].get('notright', 0) for vb in T),
         sum(T[vb].get('over', 0) for vb in T)))
b = rd(A, 'battery.txt')
if b: print('== BATTERY: first', b[0][:120], '| last', b[-1][:120])
