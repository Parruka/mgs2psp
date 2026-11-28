# rd42in.py: read an MGS2 harness gl42in PSP run (gl42 with the indoor settings file, run to vb 37,800) on the PC.   python -I rd42in.py <LOGS\gl42in folder>
# = rd42pc.py with the indoor schedule to vb 37,800 (tools/sched42in.py), the stretches 'to the door', 'indoor' and 'indoor, past the cycle', and this run's expected fingerprints.
# Standard library only, one file.  gl42 is the renderer checkpoint, a measurement build like gl39-gl41: from vb 24,000 each turn of 100
# vblanks has a KIND (README.txt has the table): K-A everything drawn, today's path; K-N1 the ship native (native step 1); K-N2 the ship
# and the people native (native step 2); K-N2R K-N2 with only chains 2-4 drawn (bound kind 9); K7 nothing drawn (the main CPU alone, the
# ME resting).  C2 on everywhere (DEVIATIONS.md B7); P1, P3, M5, F1 on; XP 3 (7 with the check in the digest turns t55, t73).
# Prints: the end and crash lines; the hash, digest and content fingerprints against the emulator run of the same package; HEAP; the
# carve after the opening (the growth); the turn table with a SCHEDULE CHECK; by stretch (deck, rain wait, walk) each kind's means and
# K-N1 / K-N2 / K-N2R minus K-A and K-A minus K7 with main, ME and P = max(main, ME); each lever turn against its K-A neighbours; the
# people by turn with the reuse content hash's time a step (MGS2_NGE_HPROBE); native step 1 by turn; the problem counters.
# main = the step minus the GE wait (the main CPU's own time a step, its waits for the ME included in the GE wait); ME = the ME's busy
# time a step (all but idle and GE waits); P = max(main, ME).  Every figure is ms a PSP step (a vblank) unless it says otherwise.
import sys, os, re, hashlib
A = sys.argv[1]
EH, NH = 'ea5ddb8a15ee', 124          # hash lines 'vb ee iop' of the emulator run
ED, EDZ, ND = '88e5c3ee0456', '40cdae675359', 161   # digest lines 'vb list frame' / with the dz text
EC, NC = 'c5a6029df50c', 161          # content lines 'vb content'
SCHED = [(0, 24000, 'A', 0, 0, 1, 3, 'deck, change of scene', 'deck', 1), (1, 24100, '2', 2, 0, 1, 3, 'first deck scene', 'deck', 1), (2, 24200, 'A', 0, 0, 1, 3, 'first deck scene', 'deck', 1), (3, 24300, 'R', 2, 9, 1, 3, 'first deck scene', 'deck', 1), (4, 24400, 'A', 0, 0, 1, 3, 'first deck scene', 'deck', 1), (5, 24500, 'N', 1, 0, 1, 3, 'first deck scene', 'deck', 1), (6, 24600, 'A', 0, 0, 1, 3, 'first deck scene', 'deck', 1), (7, 24700, '2', 2, 0, 1, 3, 'first deck scene', 'deck', 1), (8, 24800, 'A', 0, 0, 1, 3, 'first deck scene', 'deck', 1), (9, 24900, 'R', 2, 9, 1, 3, 'first deck scene', 'deck', 1), (10, 25000, 'A', 0, 0, 1, 3, 'deck, change of scene', 'deck', 1), (11, 25100, '2', 2, 0, 1, 3, 'deck (first play)', 'deck', 1), (12, 25200, 'A', 0, 0, 1, 3, 'deck (first play)', 'deck', 1), (13, 25300, 'R', 2, 9, 1, 3, 'deck (first play)', 'deck', 1), (14, 25400, 'A', 0, 0, 1, 3, 'deck (first play)', 'deck', 1), (15, 25500, 'N', 1, 0, 1, 3, 'deck (first play)', 'deck', 1), (16, 25600, 'A', 0, 0, 1, 3, 'deck (first play)', 'deck', 1), (17, 25700, '2', 2, 0, 1, 3, 'deck (first play)', 'deck', 1), (18, 25800, 'A', 0, 0, 1, 3, 'deck (first play)', 'deck', 1), (19, 25900, '7', 0, 7, 1, 3, 'deck (first play)', 'deck', 1), (20, 26000, 'A', 0, 0, 1, 3, 'deck, change of scene', 'deck', 1), (21, 26100, '2', 2, 0, 1, 3, 'deck, change of scene', 'deck', 1), (22, 26200, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (23, 26300, 'R', 2, 9, 1, 3, 'steady deck', 'deck', 1), (24, 26400, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (25, 26500, 'N', 1, 0, 1, 3, 'steady deck', 'deck', 1), (26, 26600, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (27, 26700, '2', 2, 0, 1, 3, 'steady deck', 'deck', 1), (28, 26800, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (29, 26900, 'R', 2, 9, 1, 3, 'steady deck', 'deck', 1), (30, 27000, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (31, 27100, '2', 2, 0, 1, 3, 'steady deck', 'deck', 1), (32, 27200, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (33, 27300, 'R', 2, 9, 1, 3, 'steady deck', 'deck', 1), (34, 27400, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (35, 27500, 'N', 1, 0, 1, 3, 'steady deck', 'deck', 1), (36, 27600, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (37, 27700, '2', 2, 0, 1, 3, 'steady deck', 'deck', 1), (38, 27800, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (39, 27900, '7', 0, 7, 1, 3, 'steady deck', 'deck', 1), (40, 28000, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (41, 28100, '2', 2, 0, 1, 3, 'steady deck', 'deck', 1), (42, 28200, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (43, 28300, 'R', 2, 9, 1, 3, 'steady deck', 'deck', 1), (44, 28400, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (45, 28500, 'N', 1, 0, 1, 3, 'steady deck', 'deck', 1), (46, 28600, 'A', 0, 0, 1, 3, 'steady deck', 'deck', 1), (47, 28700, '2', 2, 0, 1, 3, 'change of scene', 'deck', 1), (48, 28800, 'A', 0, 0, 1, 3, 'change of scene', 'deck', 1), (49, 28900, '7', 0, 7, 1, 3, 'change of scene', 'deck', 1), (50, 29000, 'A', 0, 0, 1, 3, 'rain begins', 'rain wait', 1), (51, 29100, '2', 2, 0, 1, 3, 'rain wait, first turns', 'rain wait', 1), (52, 29200, 'A', 0, 0, 1, 3, 'rain wait, first turns', 'rain wait', 1), (53, 29300, 'R', 2, 9, 1, 3, 'rain wait, first turns', 'rain wait', 1), (54, 29400, 'A', 0, 0, 1, 3, 'rain wait, first turns', 'rain wait', 1), (55, 29500, '2', 2, 0, 0, 7, 'rain wait', 'rain wait', 1), (56, 29600, 'A', 0, 0, 1, 3, 'rain wait', 'rain wait', 1), (57, 29700, 'N', 1, 0, 1, 3, 'rain wait', 'rain wait', 1), (58, 29800, 'A', 0, 0, 1, 3, 'rain wait', 'rain wait', 1), (59, 29900, '2', 2, 0, 1, 3, 'rain wait', 'rain wait', 1), (60, 30000, 'A', 0, 0, 1, 3, 'rain wait', 'rain wait', 1), (61, 30100, 'R', 2, 9, 1, 3, 'rain wait', 'rain wait', 1), (62, 30200, 'A', 0, 0, 1, 3, 'rain wait', 'rain wait', 1), (63, 30300, '7', 0, 7, 1, 3, 'rain wait', 'rain wait', 1), (64, 30400, 'A', 0, 0, 1, 3, 'rain wait', 'rain wait', 1), (65, 30500, '2', 2, 0, 1, 3, 'rain wait', 'rain wait', 1), (66, 30600, 'A', 0, 0, 1, 3, 'rain wait (heavier)', 'rain wait', 1), (67, 30700, 'R', 2, 9, 1, 3, 'rain wait (heavier)', 'rain wait', 1), (68, 30800, 'A', 0, 0, 1, 3, 'rain walk', 'walk', 1), (69, 30900, '2', 2, 0, 1, 3, 'rain walk', 'walk', 1), (70, 31000, 'A', 0, 0, 1, 3, 'rain walk', 'walk', 1), (71, 31100, 'R', 2, 9, 1, 3, 'rain walk', 'walk', 1), (72, 31200, 'A', 0, 0, 1, 3, 'rain walk', 'walk', 1), (73, 31300, '2', 2, 0, 0, 7, 'the walk ends', 'walk', 1), (74, 31400, 'A', 0, 0, 1, 3, 'to the door', 'to the door', 1), (75, 31500, 'A', 0, 0, 1, 3, 'to the door', 'to the door', 1), (76, 31600, 'A', 0, 0, 1, 3, 'to the door', 'to the door', 1), (77, 31700, 'A', 0, 0, 1, 3, 'to the door', 'to the door', 1), (78, 31800, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (79, 31900, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (80, 32000, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (81, 32100, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (82, 32200, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (83, 32300, 'N', 1, 0, 1, 3, 'indoor', 'indoor', 1), (84, 32400, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (85, 32500, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (86, 32600, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (87, 32700, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (88, 32800, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (89, 32900, '2', 2, 0, 0, 7, 'indoor', 'indoor', 1), (90, 33000, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (91, 33100, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (92, 33200, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (93, 33300, 'N', 1, 0, 1, 3, 'indoor', 'indoor', 1), (94, 33400, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (95, 33500, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (96, 33600, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (97, 33700, '7', 0, 7, 1, 3, 'indoor', 'indoor', 1), (98, 33800, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (99, 33900, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (100, 34000, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (101, 34100, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (102, 34200, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (103, 34300, 'N', 1, 0, 1, 3, 'indoor', 'indoor', 1), (104, 34400, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (105, 34500, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (106, 34600, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (107, 34700, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (108, 34800, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (109, 34900, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (110, 35000, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (111, 35100, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (112, 35200, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (113, 35300, 'N', 1, 0, 1, 3, 'indoor', 'indoor', 1), (114, 35400, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (115, 35500, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (116, 35600, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (117, 35700, '7', 0, 7, 1, 3, 'indoor', 'indoor', 1), (118, 35800, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (119, 35900, '2', 2, 0, 0, 7, 'indoor', 'indoor', 1), (120, 36000, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (121, 36100, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (122, 36200, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (123, 36300, 'N', 1, 0, 1, 3, 'indoor', 'indoor', 1), (124, 36400, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (125, 36500, '2', 2, 0, 1, 3, 'indoor', 'indoor', 1), (126, 36600, 'A', 0, 0, 1, 3, 'indoor', 'indoor', 1), (127, 36700, 'R', 2, 9, 1, 3, 'indoor', 'indoor', 1), (128, 36800, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (129, 36900, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (130, 37000, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (131, 37100, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (132, 37200, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (133, 37300, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (134, 37400, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (135, 37500, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (136, 37600, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0), (137, 37700, 'D', 0, 0, 0, 0, 'indoor, past the cycle', 'indoor, past the cycle', 0)]   # (turn, vb, kind, NGE digit, bound kind, digest off, XP, scene, stretch, F1): tools/sched42in.py py
EXPNOP = [25100, 25300, 25700]   # K-N2 / K-N2R turns with no people records in the emulator run of this package (no people in the scene)
KN = {'D': 'past the cycle', 'A': 'K-A all drawn', 'N': 'K-N1 ship native', '2': 'K-N2 ship+people', 'R': 'K-N2R bound 9', '7': 'K7 nothing drawn'}
BK = {'D': 0, 'A': 0, 'N': 0, '2': 0, 'R': 9, '7': 7}
def rd(d, f):
    p = os.path.join(d, f)
    return open(p, encoding='latin-1').read().splitlines() if os.path.exists(p) else []
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
def num(pat, s, d=None):
    m = re.search(pat, s); return float(m.group(1)) if m else d
la = rd(A, 'log.txt')
print('== END:', repr(la[-1]) if la else 'NO LOG', '| lines', len(la))
for s in la[-14:]:
    if s.startswith(('memory at the end', 'disc pack', 'stack', 'memory (gl41)')): print('  ', s[:400])
for s in la:
    if s.startswith(('bound: ', 'gl40 XP totals', 'F1 totals', 'bound totals', 'natcull: totals', 'nge totals', 'nge converter', 'nge: ', 'nge people totals', 'np6: ', 'xc nge6')): print('  ', s[:900])
print('== CRASH:'); [print('  ', s[:200]) for s in rd(A, 'crash.txt')[:12]]
H, D, C = [], [], []
for s in la:
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append(('%s %s %s' % m.groups()[:3], m.group(4).strip())); continue
    m = re.match(r'picture: vb (\d+) content ([0-9a-f]+)$', s)
    if m: C.append('%s %s' % m.groups())
print('== HASHES (the game\'s state; gl42\'s own hashref: the people native change it from vb 24,100): %d lines, %s (expected %d, %s): %s' % (
      len(H), md(H), NH, EH, 'EQUAL' if md(H) == EH and len(H) == NH else 'DIFFERENT'))
if md(H) != EH: [print('  H', h) for h in H]
for s in la:
    if s.startswith('split:'): print('  ', s[:200])
dd, ddz = md([a for a, _ in D]), md([a + ' ' + z for a, z in D])
print('== DIGESTS (the pictures\' lists in the digest-on turns t55, t73, t89, t119): %d lines, %s / with dz %s (expected %d, %s / %s): %s' % (len(D), dd, ddz, ND, ED, EDZ,
      'EQUAL' if (dd, ddz) == (ED, EDZ) else 'DIFFERENT'))
print('== CONTENT (the pictures\' content, a native batch hashed as today\'s; the twin\'s lines): %d lines, %s (expected %d, %s): %s' % (len(C), md(C), NC, EC,
      'EQUAL' if (md(C), len(C)) == (EC, NC) else 'DIFFERENT'))
print('== HEAP (the gate on the emulator: >= 32 KB at the worst moment, 0 textures refused for want of memory):')
for s in la:
    if s.startswith(('memory (gl41)', 'picture heap (gl41)', 'me picture loop (gl41)', 'free memory:', 'memory at the start', 'picture: native step 1 ')) or 'textures refused for want of memory' in s or 'malloc can still' in s or 'headroom' in s:
        print('  ', s[:700])
print('== THE CARVE AFTER THE OPENING (step 2: each region 256 KB until vb 24,000, then 384 KB with the people\'s block; the people from vb 24,100):')
GROWN = None
for s in la:
    if s.startswith('picture: step 2') or s.startswith('nge: step 2'):
        print('  ', s[:400]); m = re.match(r'picture: step 2: vb (\d+): grown', s)
        if m: GROWN = int(m.group(1))
print('   grown at vb %s (%s)' % (GROWN, 'NOT GROWN' if GROWN is None else 'in time: the people start at vb 24,100' if GROWN < 24100 else 'LATE: after the people\'s start'))
T = {}; LV = []
def tk(a): return T.setdefault(a, {})
last_nge = None
for s in la:
    m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()[2:]]
        if int(m.group(2)) == 100: tk(int(m.group(1)) - 100).update(mean=g[0], logic=g[1], pipe=g[2], iop=g[3], gsw=g[4], xl=g[5], pic=g[6], gew=g[7], mx=g[8], main=g[0] - g[7])
        continue
    m = re.match(r'me turn vb (\d+)-(\d+) \(([\d.]+) ms\): the ME.s own time \(ms\): records ([\d.]+), thinning ([\d.]+), fills ([\d.]+), walks ([\d.]+), hooks ([\d.]+) '
                 r'\(frame ends and lists ([\d.]+), GE waits ([\d.]+), the rest ([\d.]+)\), hand-over ([\d.]+), idle ([\d.]+), all ([\d.]+)', s)
    if m:
        g = [float(x) for x in m.groups()[2:]]; a = int(m.group(1)); n = max(1, int(m.group(2)) - a)
        tk(a).update(me_ms=g[0], me_rec=g[1] / n, me_walks=g[4] / n, me_lists=g[6] / n, me_gew=g[7] / n, me_idle=g[10] / n, me_busy=(g[11] - g[10] - g[7]) / n,
                     wfill=num(r'the main CPU waited \(ms\): copies as they ended [\d.]+, fills ([\d.]+)', s, 0) / n); continue
    m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl38 bound: turn (-?\d+) kind (\d) \(mask (\d+)\), F1 (\d)', s)
    if m:
        g = m.groups(); a = int(g[1]); tk(a).update(who=g[0], full=int(g[2]) - a == 100, turn=int(g[3]), kind=int(g[4]), f1=int(g[6])); continue
    m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl39 probe: turn (-?\d+) kind (\d) digest (on|off)', s)
    if m: tk(int(m.group(2))).update(pwho=m.group(1), pdig=m.group(6)); continue
    m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl40 plan: turn (-?\d+) XP (\d+) M5 walks (\d+); plans (\d+), us total (\d+) .*?XP check: plans (\d+), differing (\d+)', s)
    if m:
        g = m.groups(); a = int(g[1]); n = max(1, int(g[2]) - a)
        tk(a).update(xp=int(g[4]), m5w=int(g[5]) / n, plans=int(g[6]) / n, pl_ms=int(g[7]) / 1000.0 / n, chk=int(g[8]), dif=int(g[9])); continue
    m = re.match(r'nat turn vb (\d+)-(\d+) (me|main) turn arm (\d+)(.*)', s)
    if m:
        r = m.group(5); a = int(m.group(1))
        prob = {k: num(p, r, 0) for k, p in (('builder', r'builder run instead \(dropped records\) (\d+)'), ('noarr', r'recorded with no array (\d+)'),
                ('early', r'early fills for an array.s reuse (\d+)'), ('drop', r'fed word by word \d+, dropped (\d+)'), ('partly', r'partly \(dropped\) (\d+)'))}
        tk(a).update(groups=num(r'calls recorded \d+, groups (\d+)', r), left=num(r'groups left untaken (\d+)', r, 0), prob={k: v for k, v in prob.items() if v}); continue
    m = re.match(r'np6 turn vb (\d+)-(\d+) (me|main) turn arm (\d+): calls recorded (\d+), groups (\d+) \(most in a picture (\d+)\), pictures (\d+)(.*)', s)
    if m:
        g = m.groups(); a = int(g[0]); r = g[8]
        p6 = {k: num(p, r, 0) for k, p in (('full', r'array full (\d+)'), ('order', r'key order (\d+)'), ('end', r'near the buffer.s end (\d+)'), ('other', r'other (\d+) \('),
              ('noarr', r'recorded with no array (\d+)'), ('left', r'groups left untaken (\d+)'), ('dropped', r'fed word by word \d+, dropped (\d+)'))}
        tk(a).update(p6arm=int(g[3]), p6calls=int(g[4]), p6grp=int(g[5]), p6pics=int(g[7]), p6core=num(r'groups to the core (\d+)', r), p6late=num(r'drawn at a walk.s end (\d+)', r, 0),
                     p6bld=num(r'calls in arm 3 that ran the builder (\d+)', r, 0), p6glate=num(r'\(the growth late (\d+)\)', r, 0), p6prob={k: v for k, v in p6.items() if v}); continue
    m = re.match(r'nat f1 vb (\d+)-\d+ \w+ turn arm \d+ F1 (\d) \([^)]*\): runs planned (\d+) \(groups (\d+), of them not written (\d+), without the fast form (\d+)\), runs not planned (\d+) '
                 r'\(state (\d+), a group without an MSCAL (\d+)\), planned runs with a problem (\d+); check mode: groups compared (\d+), DIFFERING (\d+)', s)
    if m:
        g = [int(x) for x in m.groups()]; tk(g[0]).update(f1prob=g[9], f1diff=g[11]); continue
    m = re.match(r'nge turn vb (\d+)-(\d+):(.*?); arena used (\d+) KB, most (\d+) KB, of (\d+) KB', s)
    if m:
        a = int(m.group(1)); c = {}; last_nge = a
        for q in m.group(3).split(';'):
            q = q.strip(); w = q.rsplit(' ', 1)
            if len(w) == 2 and w[1].isdigit(): c[w[0]] = int(w[1])
        tk(a).update(nge=c, nge_most=int(m.group(5)), nge_cap=int(m.group(6))); continue
    m = re.match(r'nge people turn:(.*)', s)
    if m and last_nge is not None:
        c = {}
        for q in m.group(1).split(';'):
            q = q.strip(); w = q.rsplit(' ', 1)
            if len(w) == 2 and w[1].lstrip('-').isdigit(): c[w[0]] = int(w[1])
        tk(last_nge).update(ppl=c); continue
    m = re.match(r'gl41 levers vb (\d+): opt2 ([0-9a-f]+)', s)
    if m: LV.append((int(m.group(1)), m.group(2))); continue
def f(x, w='%.2f'): return '-' if x is None else (w % x if isinstance(x, float) else str(x))
def av(rows, k):
    v = [r[k] for r in rows if r.get(k) is not None]; return sum(v) / len(v) if v else None
for x in T.values():
    if x.get('main') is not None and x.get('me_busy') is not None: x['P'] = max(x['main'], x['me_busy'])
    elif x.get('main') is not None and x.get('kind') == 7: x['P'] = x['main']
    p = x.get('ppl') or {}
    if p.get('reuse hash checks') is not None:
        x['h_n'] = p['reuse hash checks'] / 100.0; x['h_ms'] = p.get('reuse hash us', 0) / 1000.0 / 100.0; x['h_kb'] = p.get('stream bytes hashed at use', 0) / 1024.0 / 100.0
print('== MAIN-DRAWING STEPS before vb 24,000 (every other 100 vblanks the main CPU makes the picture itself: "main"; in the others the ME does: "me").  C2 is on here too (gl41: off)')
pre = [vb for vb in sorted(T) if vb < 24000 and T[vb].get('mean') is not None]
for lo, hi, nm in ((15700, 16100, 'tanker bow (15,700-16,100)'), (20000, 24000, '20,000-24,000'), (0, 24000, 'all windows before 24,000')):
    for w in ('main', 'me'):
        rs = [T[vb] for vb in pre if lo <= vb < hi and (T[vb].get('who') or T[vb].get('pwho') or ('main' if (vb // 100) & 1 else 'me')) == w]
        if rs: print('  mean of %-28s %-4s n %3d | step %s logic %s pipe %s gsw %s xl %s pic %s gew %s' % ((nm, w, len(rs)) + tuple(f(av(rs, c)) for c in ('mean', 'logic', 'pipe', 'gsw', 'xl', 'pic', 'gew'))))
F1X = {vb: f1 for (t, vb, k, ng, bk, d, xp, sc, st, f1) in SCHED}
S = {vb: (t, k, ng, bk, d, xp, sc, st) for (t, vb, k, ng, bk, d, xp, sc, st, f1) in SCHED}
def dg(vb): return 'none' if vb + 99 < 29490 else ('on' if vb in (32900, 35900) else 'none') if vb >= 31400 else 'off' if S[vb][4] else 'on'   # gl42in: indoors digests only in t89, t119
def ngn(x, k): return (x.get('nge') or {}).get(k)
def pp(x, k): return (x.get('ppl') or {}).get(k)
print('== TURNS  vb | t scene | kind XP dig | who | step main ME P | logic pipe gew | people: arm, pictures, groups native/all, cooked, no room, hash checks a step, hash ms a step | ship: calls native/all, no room')
bad = []; nop = []
for vb in sorted(S):
    t, k, ng, bk, d, xp, sc, st = S[vb]; x = T.get(vb, {}); nxt = S.get(vb + 100, (0, ''))[1]
    if x.get('kind') is not None and (x['kind'] != BK[k] or x.get('f1') != F1X[vb]): bad.append((vb, 'kind/F1'))
    if x.get('pdig') is not None and (x['pdig'] == 'off') != bool(d): bad.append((vb, 'digest'))
    if x.get('xp') is not None and x['xp'] != xp: bad.append((vb, 'XP'))
    cn, ca = ngn(x, 'calls native'), ngn(x, 'calls')
    if k in 'N2R' and cn == 0 and (ca or 0) > 0: bad.append((vb, 'a native turn without a native ship call'))
    if k not in 'N2R' and cn and not (ngn(x, 'pictures') == 1 and nxt in 'N2R'): bad.append((vb, 'native ship calls in a turn of today\'s path'))
    pr = pp(x, 'people groups referenced')
    if k in '2R' and vb >= 24100 and x.get('p6pics') == 0: (nop if vb in EXPNOP else bad).append((vb, 'K-N2 / K-N2R without people records'))
    if k in '2R' and vb >= 24100 and pr == 0: bad.append((vb, 'K-N2 / K-N2R without a native people group'))
    if k not in '2R' and x.get('p6pics') and not (x['p6pics'] == 1 and nxt in '2R'): bad.append((vb, 'people records in a turn of today\'s path'))
    if x.get('m5w') is not None and x['m5w'] == 0 and k != '7': bad.append((vb, 'M5 no walks'))
    print('%d | t%-2d %-22s | %-17s XP%d %-4s | %-4s | %s %s %s %s | %s %s %s | %s, %s, %s/%s, %s, %s, %s, %s | %s/%s, %s' % (vb, t, sc[:22], KN[k], xp, dg(vb), x.get('who', '?'),
          f(x.get('mean')), f(x.get('main')), f(x.get('me_busy')), f(x.get('P')), f(x.get('logic')), f(x.get('pipe')), f(x.get('gew')),
          f(x.get('p6arm')), f(x.get('p6pics')), f(pr), f(pp(x, 'people groups')), f(pp(x, 'people meshes cooked')), f(pp(x, 'people meshes not cooked: no room')),
          f(x.get('h_n'), '%.1f'), f(x.get('h_ms'), '%.3f'), f(cn), f(ca), f(ngn(x, 'no room'))))
print('== SCHEDULE CHECK: turns whose logged kind, F1, digest, XP, M5 or native calls are not the schedule\'s (a turn line counts the next turn\'s first picture: one picture early is accepted): %s' % (bad or 'none'))
print('   as the emulator\'s run of this package (no people in the scene, so no people records; not a fault): %s' % ([v for v, _ in nop] or 'none'))
COLS = ['mean', 'main', 'me_busy', 'P', 'logic', 'pipe', 'gew', 'me_walks', 'me_lists', 'h_ms']
HDR = 'step main ME P | logic pipe gew | ME walks lists | hash'
def rows(stretch, k, d=None):
    return [T[vb] for vb in sorted(S) if S[vb][7] == stretch and S[vb][1] == k and T.get(vb, {}).get('mean') is not None and (d is None or dg(vb) == d)]
def line(tag, rs):
    a = [av(rs, c) for c in COLS]
    print('  %-46s n %2d | ' % (tag, len(rs)) + ' '.join(f(v) for v in a[:4]) + ' | ' + ' '.join(f(v) for v in a[4:7]) + ' | ' + ' '.join(f(v) for v in a[7:9]) + ' | ' + f(a[9], '%.3f'))
def delta(tag, rs, ref, note=''):
    if not rs or not ref: return
    a = [av(rs, c) for c in COLS]; b = [av(ref, c) for c in COLS]
    d = ['-' if x is None or y is None else '%+.2f' % (x - y) for x, y in zip(a, b)]
    print('  %-46s n %d - %d | %s | %s | %s | %s  %s' % (tag, len(rs), len(ref), ' '.join(d[:4]), ' '.join(d[4:7]), ' '.join(d[7:9]), d[9], note))
for stretch in ('deck', 'rain wait', 'walk', 'to the door', 'indoor', 'indoor, past the cycle'):
    print('== %s.  Means, ms a PSP step: %s' % (stretch.upper(), HDR))
    for d in ('none', 'off', 'on'):
        for k in 'AN2R7D':
            rs = rows(stretch, k, d)
            if rs: line('%s, digests %s' % (KN[k], d), rs)
    print('   differences (the first minus the second; digest turns left out): %s' % HDR)
    for d in ('none', 'off'):
        ka = rows(stretch, 'A', d)
        delta('K-N1 minus K-A (the ship native), dig %s' % d, rows(stretch, 'N', d), ka)
        delta('K-N2 minus K-A (ship + people native), dig %s' % d, rows(stretch, '2', d), ka)
        delta('K-N2R minus K-A (native, bound 9), dig %s' % d, rows(stretch, 'R', d), ka)
        delta('K-N2 minus K-N1 (the people\'s share), dig %s' % d, rows(stretch, '2', d), rows(stretch, 'N', d))
        delta('K-A minus K7 (drawing\'s cost on the main CPU), dig %s' % d, ka, rows(stretch, '7', d))
print('== PAIRS: each lever turn minus the mean of its K-A neighbours (digest turns left out).  %s' % HDR)
for vb in sorted(S):
    t, k, ng, bk, d, xp, sc, st = S[vb]
    if k not in 'N2R7' or dg(vb) == 'on' or T.get(vb, {}).get('mean') is None: continue
    nb = [u for u in (vb - 100, vb + 100) if u in S and S[u][1] == 'A' and dg(u) != 'on' and T.get(u, {}).get('mean') is not None]
    delta('t%d %s (%s) vs %s' % (t, KN[k], st, ' '.join('t%d' % S[u][0] for u in nb) or '-'), [T[vb]], [T[u] for u in nb])
print('== THE PEOPLE (step 2) by turn, K-N2 / K-N2R turns: vb t | calls, groups native / all, meshes reused, cooked, re-cooked (changed), no room, not cookable | reuse content hash: checks a step, KB a step, ms a step (the main CPU) | records: pictures, groups, to the core, drawn at a walk\'s end, the builder run in arm 3 (growth late)')
for vb in sorted(S):
    x = T.get(vb, {}); c = x.get('ppl') or {}
    if S[vb][1] not in '2R': continue
    print('  %d t%-2d %-17s | %s, %s / %s, %s, %s, %s, %s, %s | %s, %s, %s | %s, %s, %s, %s, %s (%s)' % (vb, S[vb][0], KN[S[vb][1]], f(c.get('people calls')), f(c.get('people groups referenced')),
          f(c.get('people groups')), f(c.get('people meshes reused')), f(c.get('people meshes cooked')), f(c.get('people meshes re-cooked (changed)')),
          f(c.get('people meshes not cooked: no room')), f(c.get('people meshes not cookable')), f(x.get('h_n'), '%.1f'), f(x.get('h_kb'), '%.1f'), f(x.get('h_ms'), '%.3f'),
          f(x.get('p6pics')), f(x.get('p6grp')), f(x.get('p6core')), f(x.get('p6late')), f(x.get('p6bld')), f(x.get('p6glate'))))
hs = [T[vb] for vb in S if S[vb][1] in '2R' and T.get(vb, {}).get('h_ms') is not None]
for stretch in ('deck', 'rain wait', 'walk', 'to the door', 'indoor', 'indoor, past the cycle'):
    rs = [T[vb] for vb in S if S[vb][1] in '2R' and S[vb][7] == stretch and T.get(vb, {}).get('h_ms') is not None]
    if rs: print('   the reuse content hash on the main CPU, %s (K-N2 and K-N2R turns, n %d): %s checks, %s KB, %s ms a step' % (stretch, len(rs), f(av(rs, 'h_n'), '%.1f'), f(av(rs, 'h_kb'), '%.1f'), f(av(rs, 'h_ms'), '%.3f')))
print('== NATIVE STEP 1 by turn (the ship; K-N1, K-N2, K-N2R turns): vb t | pictures, calls native / calls, reused, cooked, re-cooked after eviction, evicted, no room, groups native / groups | arena most KB of')
for vb in sorted(S):
    x = T.get(vb, {}); c = x.get('nge')
    if not c or S[vb][1] not in 'N2R': continue
    print('  %d t%-2d %-17s | %s, %s / %s, %s, %s, %s, %s, %s, %s / %s | %s of %s' % (vb, S[vb][0], KN[S[vb][1]], c.get('pictures'), c.get('calls native'), c.get('calls'), c.get('reused'),
          c.get('cooked'), c.get('re-cooked (evicted before)'), c.get('evicted'), c.get('no room'), c.get('groups native'), c.get('groups'), x.get('nge_most'), x.get('nge_cap')))
print('== PROBLEM COUNTERS (all should be none / 0):')
pr = [(vb, T[vb]['prob']) for vb in sorted(T) if T[vb].get('prob')]
print('  nat (the ship\'s records: builder, no array, early fills, dropped, partly):', pr or 'none')
print('  np6 (the people\'s records: array full, key order, buffer end, other, no array, left, dropped):', [(vb, T[vb]['p6prob']) for vb in sorted(T) if T[vb].get('p6prob')] or 'none')
print('  the people\'s growth late (the builder ran in arm 3 from the start on):', [(vb, int(T[vb]['p6glate'])) for vb in sorted(T) if T[vb].get('p6glate')] or 'none')
print('  F1: planned runs with a problem %d, check DIFFERING %d' % (sum(T[vb].get('f1prob', 0) for vb in T), sum(T[vb].get('f1diff', 0) for vb in T)))
print('  XP check by turn (plans / DIFFERING):', [(vb, T[vb]['chk'], T[vb]['dif']) for vb in sorted(T) if T[vb].get('chk')] or 'none')
for s in la:
    if s.startswith('gl40 XP totals'): print('  XP check over the run (DIFFERING must be 0):', s[:400])
    if s.startswith('nge converter'):
        for q in ('declined: count', 'native batches whose stub-time WORLD'):
            m = re.search(re.escape(q) + r'[^;]* (\d+)(;|$)', s)
            if m: print('  native step 1, converter: %s ... %s' % (q, m.group(1)))
    if 'NAT PROBLEMS' in s or s.startswith('nat: PROBLEMS') or 'STOPPED' in s or 'TAKEN BACK' in s or 'take-back' in s and 'none' not in s or 'NOT ON' in s: print('  ', s[:300])
b = rd(A, 'battery.txt')
if b: print('== BATTERY: first', b[0][:120], '| last', b[-1][:120])
print('== INDOOR BY TURN (vb, kind, who | step main ME P | logic pipe gew):')
for vb in sorted(S):
    x = T.get(vb, {})
    if vb >= 31400 and x.get('mean') is not None: print('  %d t%-3d %-17s %-4s | %s %s %s %s | %s %s %s' % (vb, S[vb][0], KN[S[vb][1]], x.get('who', '?'), f(x.get('mean')), f(x.get('main')), f(x.get('me_busy')), f(x.get('P')), f(x.get('logic')), f(x.get('pipe')), f(x.get('gew'))))
for s in la:
    if s.startswith(('stage ', 'load ', 'overlay')) or 'w01a' in s[:60]: print('  ', s[:200])
