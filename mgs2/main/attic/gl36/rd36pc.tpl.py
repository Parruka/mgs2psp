# rd36pc.py: read an MGS2 harness PSP run on the PC (gl36 against gl35). python -I rd36pc.py <LOGS\gl36> <LOGS\gl35>
# Standard library only, one file. Prints: the end and crash lines, the hash and digest fingerprints against the emulator run of the
# same package, the turn table next to gl35's same vblanks, the means by turn kind and reuse arm (gl36's 'nat reuse' lines; gl36's
# reuse 0 turns fall on gl35's keep 0 turns, its reuse 1 turns on gl35's keep 15 turns) next to gl35's same turns, the Media Engine's
# split lines ('nat split', 'me chains') per turn and by reuse arm, the reuse counters, the nat, keep and reuse problem counters, the
# battery, the pictures against gl35's.  The second folder may be another earlier build's LOGS folder.
import sys, os, re, hashlib
A, B = sys.argv[1], sys.argv[2]
EH = '@@EH@@'                      # @@NH@@ hash lines 'vb ee iop' (gl36 emulator run e2 = hashref c9ddb593)
ED, EDZ = '@@ED@@', '@@EDZ@@'  # @@ND@@ digest lines 'vb list frame' / with the dz text
EB = dict(zip(range(@@B0@@, @@B1@@), '@@EB@@'.split()))
NH, ND = @@NH@@, @@ND@@
def rd(d, f):
    p = os.path.join(d, f)
    return open(p, encoding='latin-1').read().splitlines() if os.path.exists(p) else []
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
def num(pat, s, d=None):
    m = re.search(pat, s); return float(m.group(1)) if m else d
la, lb = rd(A, 'log.txt'), rd(B, 'log.txt')
print('== END:', repr(la[-1]) if la else 'NO LOG', '| lines', len(la))
for s in la[-12:]:
    if s.startswith(('memory at the end', 'disc pack', 'stack')): print('  ', s[:240])
for s in la:
    if s.startswith(('picture: ', 'native step 1 totals', 'fast native front end totals', 'static streams in place totals', 'keeps for fast native groups totals', 'reuse totals (gl36)', 'nat reuse (gl36)')) and ('frames made' in s or 'totals' in s or 'the table' in s): print('  ', s[:900])
print('== CRASH:'); [print('  ', s[:200]) for s in rd(A, 'crash.txt')[:12]]
H, D = [], []
for s in la:
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append((int(m.group(1)), '%s %s %s' % m.groups()[:3], m.group(4).strip()))
print('== HASHES: %d lines, %s (expected %d, %s): %s' % (len(H), md(H), NH, EH, 'EQUAL' if md(H) == EH and len(H) == NH else 'DIFFERENT'))
if md(H) != EH: [print('  H', h) for h in H]
dd, ddz = md([a for _, a, _ in D]), md([a + ' ' + z for _, a, z in D])
print('== DIGESTS: %d lines, %s / with dz %s (expected %d, %s / %s): %s' % (len(D), dd, ddz, ND, ED, EDZ, 'EQUAL' if (dd, ddz) == (ED, EDZ) else 'DIFFERENT'))
if (dd, ddz) != (ED, EDZ):
    bl = {}
    for vb, a, z in D: bl.setdefault(vb // 100, []).append((vb, a + ' ' + z))
    for k in sorted(set(bl) | set(EB)):
        got = md([x for _, x in bl.get(k, [])])
        if got != EB.get(k):
            print('  block %d00: %s expected %s' % (k, got, EB.get(k)))
            for vb, x in bl.get(k, []): print('    D', x[:160])
def turns(L):
    T = {}
    for s in L:
        m = re.match(r'window vb (\d+): \d+ steps, mean ([\d.]+) ms \(logic ([\d.]+),.*GE wait ([\d.]+)\)', s)
        if m:
            T.setdefault(int(m.group(1)) - 100, {}).update(mean=float(m.group(2)), logic=float(m.group(3)), gew=float(m.group(4))); continue
        m = re.match(r'me turn vb (\d+)-\d+ \(([\d.]+) ms\): (.*)', s)
        if m:
            r = m.group(3)
            T.setdefault(int(m.group(1)), {}).update(me=float(m.group(2)), walks=num(r'walks ([\d.]+)', r), fl=num(r'frame ends and lists ([\d.]+)', r),
                idle=num(r'idle ([\d.]+), all', r), wfill=num(r'copies as they ended [\d.]+, fills ([\d.]+)', r), rec=num(r'records ([\d.]+)', r)); continue
        m = re.match(r'(me|main) turn vb (\d+)-\d+ gl27 display: flip wait (\w+);.*drawn into the shown buffer (\d+)', s)
        if m:
            T.setdefault(int(m.group(2)), {}).update(flip=m.group(3), shown=int(m.group(4))); continue
        m = re.match(r'nat turn vb (\d+)-(\d+) (me|main) turn arm (\d+)(.*)', s)
        if m:
            r = m.group(5); a = int(m.group(1))
            prob = {k: num(p, r, 0) for k, p in (('builder', r'builder run instead \(dropped records\) (\d+)'), ('noarr', r'recorded with no array (\d+)'),
                    ('early', r'early fills for an array.s reuse (\d+)'), ('drop', r'fed word by word \d+, dropped (\d+)'), ('partly', r'partly \(dropped\) (\d+)'))}
            T.setdefault(a, {}).update(kind=m.group(3), arm=int(m.group(4)), full=int(m.group(2)) - a == 100, nat=num(r'native time ([\d.]+)', r),
                incopy=num(r'data in the copy (\d+)', r), inram=num(r'read from RAM (\d+)', r), groups=num(r'calls recorded \d+, groups (\d+)', r),
                prob={k: v for k, v in prob.items() if v}); continue
        m = re.match(r'nat inpl vb (\d+)-\d+ \w+ turn arm \d+ inpl (\d+).*groups flagged static (\d+)', s)
        if m: T.setdefault(int(m.group(1)), {}).update(inpl=int(m.group(2)), static=int(m.group(3))); continue
        m = re.match(r'nat fast vb (\d+)-\d+ \w+ turn arm \d+ fast (\d+) \([^)]*\): groups flagged (\d+); the picture.s side: taken (\d+), declined: (.*?); transforms', s)
        if m:
            dec = sum(int(x) for x in re.findall(r' (\d+)(?:,|$)', m.group(5)))
            T.setdefault(int(m.group(1)), {}).update(fast=int(m.group(2)), flagged=int(m.group(3)), taken=int(m.group(4)), declined=dec,
                hbad=num(r'checked \d+, differing (\d+)', s, 0)); continue
        m = re.match(r'nat keep vb (\d+)-\d+ \w+ turn arm \d+ keep (\d+) \([^)]*\): groups flagged (\d+); the picture.s side: drawn areas replayed (\d+), checked '
                     r'(\d+), differing (\d+); textures replayed (\d+), checked (\d+), differing (\d+), records (\d+); CLUT calls not made (\d+), PRIM writes in line (\d+)', s)
        if m:
            g = [int(x) for x in m.groups()]
            T.setdefault(g[0], {}).update(keep=g[1], kflag=g[2], kdr=g[3], kdchk=g[4], kdbad=g[5], ktr=g[6], ktchk=g[7], ktbad=g[8], ktrec=g[9],
                kclut=g[10], kprim=g[11]); continue
        m = re.match(r'nat reuse vb (\d+)-\d+ \w+ turn arm \d+ reuse (\d+) \([^)]*\): runs replayed (\d+) \(groups (\d+), skipping (\d+), in S (\d+)\), '
                     r'recorded (\d+) \(groups (\d+); kept (\d+), not kept (\d+)\), checked (\d+) \(groups (\d+); S differing (\d+), boxes differing (\d+)\); '
                     r'boxes recorded (\d+), kept boxes taken (\d+), cut (\d+), positions then needed (\d+); no table room (\d+); replays with a group not taken (\d+)', s)
        if m:
            g = [int(x) for x in m.groups()]
            T.setdefault(g[0], {}).update(ru=g[1], urr=g[2], urg=g[3], uskip=g[4], uins=g[5], urec=g[6], urecg=g[7], ukept=g[8], unkept=g[9], uchk=g[10],
                uchkg=g[11], usbad=g[12], ubbad=g[13], ubrec=g[14], ubtak=g[15], ubcut=g[16], ubpos=g[17], unoroom=g[18], untaken=g[19]); continue
        m = re.match(r'nat split vb (\d+)-\d+ me turn arm \d+ fast (\d+) \(gl3[0-9]; ticks a ms (\d+)\): groups in the timed merge runs (\d+), of them timed by stage (\d+) '
                     r'\(vertices (\d+), left out by the cut (\d+)\); clock read pairs (\d+), mean ([\d.]+) us a read; us a timed group by stage \(each holds one read\):'
                     r'(.*?); sum ([\d.]+); the turn.s native time ([\d.]+) ms = ([\d.]+) us a group; hand-off: the ME.s records ([\d.]+) ms, the plans. bytes (\d+) KB, '
                     r'of them asked for by the groups. data (\d+) KB, the groups. array (\d+) KB', s)
        if m:
            g = m.groups(); st = []
            for x in g[9].split(','):
                x = x.strip(); k = x.rfind(' '); st.append((x[:k], float(x[k + 1:])))
            T.setdefault(int(g[0]), {}).update(sp=dict(ticks=int(g[2]), seen=int(g[3]), timed=int(g[4]), nv=int(g[5]), ncut=int(g[6]), pairs=int(g[7]),
                read=float(g[8]), st=st, sum=float(g[10]), natg=float(g[12]), rec=float(g[13]), plan=int(g[14]), gdata=int(g[15]), garr=int(g[16]))); continue
        m = re.match(r'me chains vb (\d+)-\d+ me turn arm \d+ fast (\d+) \([^)]*\): lists read (\d+), transfers in a list (\d+), not in one (\d+); chain: transfers, ms, '
                     r'converter calls, native ms \(groups\):(.*); the ME.s walks ([\d.]+) ms', s)
        if m:
            c = {}
            for x in m.group(6).split(';'):
                mm = re.match(r'(.*?): (\d+), ([\d.]+), (\d+), ([\d.]+) \((\d+)\)$', x.strip())
                if mm:
                    k = mm.group(1); k = 'ends' if k.startswith('the transfers') else 'other' if k.startswith('other') else k
                    c[k] = (int(mm.group(2)), float(mm.group(3)), int(mm.group(4)), float(mm.group(5)), int(mm.group(6)))
            T.setdefault(int(m.group(1)), {}).update(ch=dict(lists=int(m.group(3)), inl=int(m.group(4)), unl=int(m.group(5)), c=c, walks=float(m.group(7))))
    return T
ta, tb = turns(la), turns(lb)
def f(x): return '-' if x is None else ('%.1f' % x if isinstance(x, float) else str(x))
print('== TURNS (vb start; per log: mean ms a step, logic, GE wait; ME turn ms, walks, frame ends+lists, idle, main waited for fills; nat arm, inpl, fast, keep, reuse, native ms, in copy/from RAM; reuse: groups in replayed runs / skipping, kept boxes taken / cut; gl35: its keep arm):')
print('vb k | gl36: mean logic gew | me walks fl idle wfill | arm inpl fast keep reuse nat copy/ram taken/declined replayed/skip box/cut || gl35: mean logic gew | me walks fl idle wfill | arm inpl fast keep nat copy/ram')
probs, flipoff, shown, hbad, kbad, ubad = [], 0, 0, 0, 0, 0
for a in sorted(k for k in ta if 24000 <= k < 31400):
    x, y = ta[a], tb.get(a, {})
    if x.get('prob'): probs.append((a, x['prob']))
    flipoff += x.get('flip') == 'off'; shown += x.get('shown', 0); hbad += int(x.get('hbad') or 0); kbad += int(x.get('kdbad') or 0) + int(x.get('ktbad') or 0); ubad += int(x.get('usbad') or 0) + int(x.get('ubbad') or 0) + int(x.get('untaken') or 0)
    print('%d %s | %s %s %s | %s %s %s %s %s | %s %s %s %s %s %s %s/%s %s/%s %s/%s %s/%s || %s %s %s | %s %s %s %s %s | %s %s %s %s %s %s/%s' % (a, (x.get('kind') or '?')[:2],
          f(x.get('mean')), f(x.get('logic')), f(x.get('gew')), f(x.get('me')), f(x.get('walks')), f(x.get('fl')), f(x.get('idle')), f(x.get('wfill')),
          f(x.get('arm')), f(x.get('inpl')), f(x.get('fast')), f(x.get('keep')), f(x.get('ru')), f(x.get('nat')), f(x.get('incopy')), f(x.get('inram')), f(x.get('taken')), f(x.get('declined')),
          f(x.get('urg')), f(x.get('uskip')), f(x.get('ubtak')), f(x.get('ubcut')),
          f(y.get('mean')), f(y.get('logic')), f(y.get('gew')), f(y.get('me')), f(y.get('walks')), f(y.get('fl')), f(y.get('idle')), f(y.get('wfill')),
          f(y.get('arm')), f(y.get('inpl')), f(y.get('fast')), f(y.get('keep')), f(y.get('nat')), f(y.get('incopy')), f(y.get('inram'))))
def av(rows, k):
    v = [r[k] for r in rows if r.get(k) is not None]; return sum(v) / len(v) if v else None
def means(lo, hi, title):
    print('== MEANS, turns starting in vb %d..%d (%s); ms a step = the window line\'s mean; ME figures are ms a turn of 100 vblanks (50 steps):' % (lo, hi - 100, title))
    MM = {}
    for kind in ('me', 'main'):
        for kp in sorted(set(ta[a].get('ru', 0) for a in ta)):
            ks = [a for a in sorted(ta) if lo <= a < hi and ta[a].get('kind') == kind and ta[a].get('ru', 0) == kp and ta[a].get('full') and ta[a].get('arm') == 3]
            if not ks: continue
            q = [ta[a] for a in ks]; p = [tb[a] for a in ks if a in tb and tb[a].get('mean') is not None]
            MM[(kind, kp)] = (av(q, 'mean'), av(p, 'mean') if p else None, sorted(set(r.get('keep') for r in p), key=str))
            t = '  gl36 %-4s turns reuse %d: %2d turns; step %s ms (logic %s, GE wait %s)' % (kind, kp, len(q), f(av(q, 'mean')), f(av(q, 'logic')), f(av(q, 'gew')))
            if kind == 'me':
                t += '; ME turn %s: walks %s, frame ends+lists %s, idle %s, records %s; main waited for fills %s; native %s' % (
                    f(av(q, 'me')), f(av(q, 'walks')), f(av(q, 'fl')), f(av(q, 'idle')), f(av(q, 'rec')), f(av(q, 'wfill')), f(av(q, 'nat')))
            print(t)
            if p:
                ip = sorted(set(r.get('keep') for r in p), key=str)
                t = '       gl35, the same %2d turns (its keep arm %s): step %s ms (logic %s, GE wait %s)' % (len(p), '/'.join(f(i) for i in ip), f(av(p, 'mean')), f(av(p, 'logic')), f(av(p, 'gew')))
                if kind == 'me':
                    t += '; ME turn %s: walks %s, frame ends+lists %s, idle %s, records %s; main waited for fills %s; native %s' % (
                        f(av(p, 'me')), f(av(p, 'walks')), f(av(p, 'fl')), f(av(p, 'idle')), f(av(p, 'rec')), f(av(p, 'wfill')), f(av(p, 'nat')))
                print(t)
    for kind in ('me', 'main'):
        ks = sorted(k for k in MM if k[0] == kind)
        if len(ks) < 2 or ks[0][1] != 0: continue
        a0 = MM[ks[0]]
        for k in ks[1:]:
            a1 = MM[k]
            t = '  %-4s:' % kind
            if a0[0] is not None and a1[0] is not None:
                t += ' reuse %d - reuse 0 (gl36, the lever) %+.2f ms a step;' % (k[1], a1[0] - a0[0])
            if a0[0] is not None and a0[1] is not None:
                t += ' gl36 reuse 0 - gl35 same turns (its keep %s: the base change = keep 1 + reset + retrain) %+.2f;' % ('/'.join(f(i) for i in a0[2]), a0[0] - a0[1])
            if a1[0] is not None and a1[1] is not None:
                t += ' gl36 reuse %d - gl35 same turns (its keep %s) %+.2f' % (k[1], '/'.join(f(i) for i in a1[2]), a1[0] - a1[1])
            print(t)
means(29200, 30500, 'the rain wait'); means(30500, 31400, 'the rain walk'); means(26200, 28800, 'the steady deck, d01t'); means(24000, 31400, 'all nat turns')
print('   (gl36 reuse 1 against gl36 reuse 0 = the lever; gl36 reuse 0 against gl35 keep 0 on the same vblanks = the base change: keep 1 + the reset + the new profile and order)')
print('== ME SPLIT per ME turn (us a timed group by stage; one group in about 32 is timed; each stage figure holds one clock read):')
names = []
for a in sorted(ta):
    if ta[a].get('sp'): names = [k for k, _ in ta[a]['sp']['st']]; break
print('   stages: ' + ', '.join('%d %s' % (i, k) for i, k in enumerate(names)))
for a in sorted(k for k in ta if 24000 <= k < 31400 and ta[k].get('sp')):
    s = ta[a]['sp']
    print('%d f%s k%s r%s | groups %d timed %d vtx %d cut %d | read %.3f us, %d ticks a ms | %s | sum %.2f | native %s ms = %.2f us a group | records %.1f ms, plans %d KB, groups\' data %d KB' % (
        a, f(ta[a].get('fast')), f(ta[a].get('keep')), f(ta[a].get('ru')), s['seen'], s['timed'], s['nv'], s['ncut'], s['read'], s['ticks'], ' '.join('%.2f' % v for _, v in s['st']), s['sum'],
        f(ta[a].get('nat')), s['natg'], s['rec'], s['plan'], s['gdata']))
def spmeans(lo, hi, title):
    M = {}
    for fst in sorted(set(ta[a].get('ru', 0) for a in ta)):
        q = [ta[a] for a in sorted(ta) if lo <= a < hi and ta[a].get('sp') and ta[a].get('ru', 0) == fst and ta[a].get('full') and ta[a].get('arm') == 3]
        if not q: continue
        tm = sum(x['sp']['timed'] for x in q) or 1; seen = sum(x['sp']['seen'] for x in q) or 1
        st = [sum(x['sp']['st'][i][1] * x['sp']['timed'] for x in q) / tm for i in range(len(names))]
        natg = sum((x.get('nat') or 0.0) for x in q) * 1000.0 / seen
        M[fst] = (st, natg)
        print('  %s, reuse %d, %d ME turns: %s | sum %.2f | native %.2f us a group (%.1f ms a turn) | clock read %.3f us | records %.1f ms a turn' % (
            title, fst, len(q), ' '.join('%.2f' % v for v in st), sum(st), natg, sum((x.get('nat') or 0.0) for x in q) / len(q),
            sum(x['sp']['read'] * x['sp']['pairs'] for x in q) / (sum(x['sp']['pairs'] for x in q) or 1), sum(x['sp']['rec'] for x in q) / len(q)))
    for k in sorted(M):
        if k == 0 or 0 not in M: continue
        print('  %s, reuse %d - reuse 0: %s | sum %+.2f | native us a group %+.2f (%+.1f%%)' % (title, k, ' '.join('%+.2f' % (b - a) for a, b in zip(M[0][0], M[k][0])),
              sum(M[k][0]) - sum(M[0][0]), M[k][1] - M[0][1], 100.0 * (M[k][1] - M[0][1]) / M[0][1] if M[0][1] else 0.0))
spmeans(29200, 30500, 'rain wait'); spmeans(30500, 31400, 'rain walk'); spmeans(26200, 28800, 'steady deck d01t'); spmeans(24000, 31400, 'all')
print('== ME CHAINS per ME turn (ms a turn : converter calls, by chain of the frame\'s list; ends = held calls converted at the transfers\' ends; other = not in a list):')
for a in sorted(k for k in ta if 24000 <= k < 31400 and ta[k].get('ch')):
    c = ta[a]['ch']; ks = sorted(c['c'], key=lambda k: (0, int(k)) if k.isdigit() else (1, 0 if k == 'ends' else 1))
    print('%d f%s k%s r%s | lists %d in %d not %d | %s | walks %.1f' % (a, f(ta[a].get('fast')), f(ta[a].get('keep')), f(ta[a].get('ru')), c['lists'], c['inl'], c['unl'],
          ' '.join('%s %.1f:%d' % (k, c['c'][k][1], c['c'][k][2]) for k in ks if c['c'][k][0] or c['c'][k][1]), c['walks']))
def chmeans(lo, hi, title):
    M = {}
    for fst in sorted(set(ta[a].get('ru', 0) for a in ta)):
        q = [ta[a]['ch'] for a in sorted(ta) if lo <= a < hi and ta[a].get('ch') and ta[a].get('ru', 0) == fst and ta[a].get('full') and ta[a].get('arm') == 3]
        if not q: continue
        ks = sorted(set(k for x in q for k in x['c']), key=lambda k: (0, int(k)) if k.isdigit() else (1, 0 if k == 'ends' else 1))
        M[fst] = {k: sum(x['c'].get(k, (0, 0.0, 0, 0.0, 0))[1] for x in q) / len(q) for k in ks}
        print('  %s, reuse %d, %d ME turns (ms a turn): %s | walks %.1f' % (title, fst, len(q), ' '.join('%s %.1f' % (k, M[fst][k]) for k in ks if M[fst][k] >= 0.05),
              sum(x['walks'] for x in q) / len(q)))
    for kk in sorted(M):
        if kk == 0 or 0 not in M: continue
        print('  %s, reuse %d - reuse 0: %s' % (title, kk, ' '.join('%s %+.1f' % (k, M[kk].get(k, 0.0) - M[0].get(k, 0.0)) for k in M[0] if M[0][k] >= 0.05 or M[kk].get(k, 0.0) >= 0.05)))
chmeans(29200, 30500, 'rain wait'); chmeans(30500, 31400, 'rain walk'); chmeans(26200, 28800, 'steady deck d01t')
def rumeans(lo, hi, title):
    for kind in ('me', 'main'):
        for kp in sorted(set(ta[a].get('ru', 0) for a in ta if 'ru' in ta[a])):
            q = [ta[a] for a in sorted(ta) if lo <= a < hi and ta[a].get('kind') == kind and ta[a].get('ru') == kp and ta[a].get('full') and ta[a].get('arm') == 3]
            if not q: continue
            print('  %s, %-4s turns reuse %d, %d turns; a turn: runs replayed %s (groups %s, skipping %s, in S %s), recorded %s (groups %s; kept %s, not kept %s); kept boxes '
                  'taken %s, cut %s, positions then needed %s; no table room %s; checked runs %s (groups %s)' % (title, kind, kp, len(q), f(av(q, 'urr')), f(av(q, 'urg')),
                  f(av(q, 'uskip')), f(av(q, 'uins')), f(av(q, 'urec')), f(av(q, 'urecg')), f(av(q, 'ukept')), f(av(q, 'unkept')), f(av(q, 'ubtak')), f(av(q, 'ubcut')),
                  f(av(q, 'ubpos')), f(av(q, 'unoroom')), f(av(q, 'uchk')), f(av(q, 'uchkg'))))
print('== REUSE COUNTERS (gl36, by turn kind and reuse arm; means a turn):')
rumeans(29200, 30500, 'rain wait'); rumeans(30500, 31400, 'rain walk'); rumeans(26200, 28800, 'steady deck d01t')
print('== NAT PROBLEMS (gl36):', probs or 'none', '| hinted pointers differing', hbad, '| keeps\' check differing', kbad, '| reuse: check differing or replays with a group not taken', ubad, '| display: turns with flip wait off', flipoff, ', lists drawn into the shown buffer', shown)
print('== LINES:'); [print('  ', s[:300]) for s in la if s.startswith(('nat:', 'native step 1:', 'picture heap', 'eye:', 'nat split (gl3'))]
bt = rd(A, 'battery.txt')
print('== BATTERY: %d lines' % len(bt))
for s in bt:
    if s.startswith(('start ', 'end ', 'HOME ')): print('  ', s[:170])
pc = [int(m.group(1)) for m in (re.search(r'vb -?\d+: (\d+)%', s) for s in bt) if m]
print('   levels: first %s, min %s, last %s; lines with external power 1: %d' % (pc[0] if pc else '-', min(pc) if pc else '-', pc[-1] if pc else '-',
      sum('external power 1' in s for s in bt)))
print('== PICTURES (gl36 against gl35, same names; pixel data only; luma = (2R+5G+B)/8; count of pixels, lighter/darker by 6+):')
na = sorted(n for n in os.listdir(A) if n.lower().endswith('.bmp')); nb = set(n for n in os.listdir(B) if n.lower().endswith('.bmp'))
same = 0
for n in na:
    if n not in nb: print('  %s: only in gl36' % n); continue
    pa, pb = open(os.path.join(A, n), 'rb').read(), open(os.path.join(B, n), 'rb').read()
    o = int.from_bytes(pa[10:14], 'little')
    xa, xb = pa[o:], pb[int.from_bytes(pb[10:14], 'little'):]
    if xa == xb: same += 1; continue
    if len(xa) != len(xb): print('  %s: sizes %d / %d' % (n, len(xa), len(xb))); continue
    dif = lt = dk = 0
    for i in range(0, len(xa) - 2, 3):
        if xa[i:i + 3] != xb[i:i + 3]:
            dif += 1
            d = ((2 * xa[i + 2] + 5 * xa[i + 1] + xa[i]) - (2 * xb[i + 2] + 5 * xb[i + 1] + xb[i])) / 8
            lt += d >= 6; dk += d <= -6
    print('  %s: %d px differ, lighter 6+ %d, darker 6+ %d' % (n, dif, lt, dk))
print('   %d of %d gl36 pictures byte-equal gl35\'s; only in gl35: %s' % (same, len(na), sorted(nb - set(na))))
