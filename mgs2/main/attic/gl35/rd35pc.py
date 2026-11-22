# rd35pc.py: read an MGS2 harness PSP run on the PC (gl35 against gl34). python -I rd35pc.py <LOGS\gl35> <LOGS\gl34>
# Standard library only, one file. Prints: the end and crash lines, the hash and digest fingerprints against the emulator run of the
# same package, the turn table next to gl34's same vblanks, the means by turn kind and keep arm (gl35's 'nat keep' lines) next to
# gl34's same turns, the Media Engine's split lines ('nat split', 'me chains') per turn and by keep arm, the nat and keep problem
# counters, the battery, the pictures against gl34's.  The second folder may be another earlier build's LOGS folder.
import sys, os, re, hashlib
A, B = sys.argv[1], sys.argv[2]
EH = '66499cdb584a'                      # 62 hash lines 'vb ee iop' (gl35 emulator run e2 = hashref c9ddb593)
ED, EDZ = 'b4bf989fb1d6', '7d2f93ab5f56'  # 916 digest lines 'vb list frame' / with the dz text
EB = dict(zip(range(294, 314), '0138e21f8c33 fc3116aea28f 697347b6ee5d 6b9ccff1ae97 5e5329e4d835 8c8303691acf 57c9ce01f7c1 9079a20e4284 6a3906ca1707 29854c27117d 6d8173c7a278 9b9ac3c4a76e 295708a5cb51 33fb7b2aeb2a fb61643000fe 67df445ed73f 3559082ade57 3ef2bda8e0f5 9848929c60f7 94f522d0e8b8'.split()))
NH, ND = 62, 916
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
    if s.startswith(('picture: ', 'native step 1 totals', 'fast native front end totals', 'static streams in place totals', 'keeps for fast native groups totals')) and ('frames made' in s or 'totals' in s): print('  ', s[:900])
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
print('== TURNS (vb start; per log: mean ms a step, logic, GE wait; ME turn ms, walks, frame ends+lists, idle, main waited for fills; nat arm, inpl, fast, keep, native ms, in copy/from RAM; kept: drawn areas / textures replayed):')
print('vb k | gl35: mean logic gew | me walks fl idle wfill | arm inpl fast keep nat copy/ram taken/declined kept dr/tex || gl34: mean logic gew | me walks fl idle wfill | arm inpl fast nat copy/ram')
probs, flipoff, shown, hbad, kbad = [], 0, 0, 0, 0
for a in sorted(k for k in ta if 24000 <= k < 31400):
    x, y = ta[a], tb.get(a, {})
    if x.get('prob'): probs.append((a, x['prob']))
    flipoff += x.get('flip') == 'off'; shown += x.get('shown', 0); hbad += int(x.get('hbad') or 0); kbad += int(x.get('kdbad') or 0) + int(x.get('ktbad') or 0)
    print('%d %s | %s %s %s | %s %s %s %s %s | %s %s %s %s %s %s/%s %s/%s %s/%s || %s %s %s | %s %s %s %s %s | %s %s %s %s %s/%s' % (a, (x.get('kind') or '?')[:2],
          f(x.get('mean')), f(x.get('logic')), f(x.get('gew')), f(x.get('me')), f(x.get('walks')), f(x.get('fl')), f(x.get('idle')), f(x.get('wfill')),
          f(x.get('arm')), f(x.get('inpl')), f(x.get('fast')), f(x.get('keep')), f(x.get('nat')), f(x.get('incopy')), f(x.get('inram')), f(x.get('taken')), f(x.get('declined')),
          f(x.get('kdr')), f(x.get('ktr')),
          f(y.get('mean')), f(y.get('logic')), f(y.get('gew')), f(y.get('me')), f(y.get('walks')), f(y.get('fl')), f(y.get('idle')), f(y.get('wfill')),
          f(y.get('arm')), f(y.get('inpl')), f(y.get('fast')), f(y.get('nat')), f(y.get('incopy')), f(y.get('inram'))))
def av(rows, k):
    v = [r[k] for r in rows if r.get(k) is not None]; return sum(v) / len(v) if v else None
def means(lo, hi, title):
    print('== MEANS, turns starting in vb %d..%d (%s); ms a step = the window line\'s mean; ME figures are ms a turn of 100 vblanks (50 steps):' % (lo, hi - 100, title))
    MM = {}
    for kind in ('me', 'main'):
        for kp in sorted(set(ta[a].get('keep', 0) for a in ta)):
            ks = [a for a in sorted(ta) if lo <= a < hi and ta[a].get('kind') == kind and ta[a].get('keep', 0) == kp and ta[a].get('full') and ta[a].get('arm') == 3]
            if not ks: continue
            q = [ta[a] for a in ks]; p = [tb[a] for a in ks if a in tb and tb[a].get('mean') is not None]
            MM[(kind, kp)] = (av(q, 'mean'), av(p, 'mean') if p else None, sorted(set(r.get('fast') for r in p), key=str))
            t = '  gl35 %-4s turns keep %2d: %2d turns; step %s ms (logic %s, GE wait %s)' % (kind, kp, len(q), f(av(q, 'mean')), f(av(q, 'logic')), f(av(q, 'gew')))
            if kind == 'me':
                t += '; ME turn %s: walks %s, frame ends+lists %s, idle %s, records %s; main waited for fills %s; native %s' % (
                    f(av(q, 'me')), f(av(q, 'walks')), f(av(q, 'fl')), f(av(q, 'idle')), f(av(q, 'rec')), f(av(q, 'wfill')), f(av(q, 'nat')))
            print(t)
            if p:
                ip = sorted(set(r.get('fast') for r in p), key=str)
                t = '       gl34, the same %2d turns (its fast arm %s): step %s ms (logic %s, GE wait %s)' % (len(p), '/'.join(f(i) for i in ip), f(av(p, 'mean')), f(av(p, 'logic')), f(av(p, 'gew')))
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
                t += ' keep %d - keep 0 (gl35, the lever) %+.2f ms a step;' % (k[1], a1[0] - a0[0])
            if a0[0] is not None and a0[1] is not None:
                t += ' gl35 keep 0 - gl34 same turns (its fast %s: the retrain) %+.2f;' % ('/'.join(f(i) for i in a0[2]), a0[0] - a0[1])
            if a1[0] is not None and a1[1] is not None:
                t += ' gl35 keep %d - gl34 same turns (its fast %s) %+.2f' % (k[1], '/'.join(f(i) for i in a1[2]), a1[0] - a1[1])
            print(t)
means(29200, 30500, 'the rain wait'); means(30500, 31400, 'the rain walk'); means(26200, 28800, 'the steady deck, d01t'); means(24000, 31400, 'all nat turns')
print('   (gl35 keep against gl35 keep 0 = the lever; gl35 keep 0 against gl34 on turns where gl34 was fast = the new profile and order)')
print('== ME SPLIT per ME turn (us a timed group by stage; one group in about 32 is timed; each stage figure holds one clock read):')
names = []
for a in sorted(ta):
    if ta[a].get('sp'): names = [k for k, _ in ta[a]['sp']['st']]; break
print('   stages: ' + ', '.join('%d %s' % (i, k) for i, k in enumerate(names)))
for a in sorted(k for k in ta if 24000 <= k < 31400 and ta[k].get('sp')):
    s = ta[a]['sp']
    print('%d f%s k%s | groups %d timed %d vtx %d cut %d | read %.3f us, %d ticks a ms | %s | sum %.2f | native %s ms = %.2f us a group | records %.1f ms, plans %d KB, groups\' data %d KB' % (
        a, f(ta[a].get('fast')), f(ta[a].get('keep')), s['seen'], s['timed'], s['nv'], s['ncut'], s['read'], s['ticks'], ' '.join('%.2f' % v for _, v in s['st']), s['sum'],
        f(ta[a].get('nat')), s['natg'], s['rec'], s['plan'], s['gdata']))
def spmeans(lo, hi, title):
    M = {}
    for fst in sorted(set(ta[a].get('keep', 0) for a in ta)):
        q = [ta[a] for a in sorted(ta) if lo <= a < hi and ta[a].get('sp') and ta[a].get('keep', 0) == fst and ta[a].get('full') and ta[a].get('arm') == 3]
        if not q: continue
        tm = sum(x['sp']['timed'] for x in q) or 1; seen = sum(x['sp']['seen'] for x in q) or 1
        st = [sum(x['sp']['st'][i][1] * x['sp']['timed'] for x in q) / tm for i in range(len(names))]
        natg = sum((x.get('nat') or 0.0) for x in q) * 1000.0 / seen
        M[fst] = (st, natg)
        print('  %s, keep %d, %d ME turns: %s | sum %.2f | native %.2f us a group (%.1f ms a turn) | clock read %.3f us | records %.1f ms a turn' % (
            title, fst, len(q), ' '.join('%.2f' % v for v in st), sum(st), natg, sum((x.get('nat') or 0.0) for x in q) / len(q),
            sum(x['sp']['read'] * x['sp']['pairs'] for x in q) / (sum(x['sp']['pairs'] for x in q) or 1), sum(x['sp']['rec'] for x in q) / len(q)))
    for k in sorted(M):
        if k == 0 or 0 not in M: continue
        print('  %s, keep %d - keep 0: %s | sum %+.2f | native us a group %+.2f (%+.1f%%)' % (title, k, ' '.join('%+.2f' % (b - a) for a, b in zip(M[0][0], M[k][0])),
              sum(M[k][0]) - sum(M[0][0]), M[k][1] - M[0][1], 100.0 * (M[k][1] - M[0][1]) / M[0][1] if M[0][1] else 0.0))
spmeans(29200, 30500, 'rain wait'); spmeans(30500, 31400, 'rain walk'); spmeans(26200, 28800, 'steady deck d01t'); spmeans(24000, 31400, 'all')
print('== ME CHAINS per ME turn (ms a turn : converter calls, by chain of the frame\'s list; ends = held calls converted at the transfers\' ends; other = not in a list):')
for a in sorted(k for k in ta if 24000 <= k < 31400 and ta[k].get('ch')):
    c = ta[a]['ch']; ks = sorted(c['c'], key=lambda k: (0, int(k)) if k.isdigit() else (1, 0 if k == 'ends' else 1))
    print('%d f%s k%s | lists %d in %d not %d | %s | walks %.1f' % (a, f(ta[a].get('fast')), f(ta[a].get('keep')), c['lists'], c['inl'], c['unl'],
          ' '.join('%s %.1f:%d' % (k, c['c'][k][1], c['c'][k][2]) for k in ks if c['c'][k][0] or c['c'][k][1]), c['walks']))
def chmeans(lo, hi, title):
    M = {}
    for fst in sorted(set(ta[a].get('keep', 0) for a in ta)):
        q = [ta[a]['ch'] for a in sorted(ta) if lo <= a < hi and ta[a].get('ch') and ta[a].get('keep', 0) == fst and ta[a].get('full') and ta[a].get('arm') == 3]
        if not q: continue
        ks = sorted(set(k for x in q for k in x['c']), key=lambda k: (0, int(k)) if k.isdigit() else (1, 0 if k == 'ends' else 1))
        M[fst] = {k: sum(x['c'].get(k, (0, 0.0, 0, 0.0, 0))[1] for x in q) / len(q) for k in ks}
        print('  %s, keep %d, %d ME turns (ms a turn): %s | walks %.1f' % (title, fst, len(q), ' '.join('%s %.1f' % (k, M[fst][k]) for k in ks if M[fst][k] >= 0.05),
              sum(x['walks'] for x in q) / len(q)))
    for kk in sorted(M):
        if kk == 0 or 0 not in M: continue
        print('  %s, keep %d - keep 0: %s' % (title, kk, ' '.join('%s %+.1f' % (k, M[kk].get(k, 0.0) - M[0].get(k, 0.0)) for k in M[0] if M[0][k] >= 0.05 or M[kk].get(k, 0.0) >= 0.05)))
chmeans(29200, 30500, 'rain wait'); chmeans(30500, 31400, 'rain walk'); chmeans(26200, 28800, 'steady deck d01t')
print('== NAT PROBLEMS (gl35):', probs or 'none', '| hinted pointers differing', hbad, '| keeps\' check differing', kbad, '| display: turns with flip wait off', flipoff, ', lists drawn into the shown buffer', shown)
print('== LINES:'); [print('  ', s[:300]) for s in la if s.startswith(('nat:', 'native step 1:', 'picture heap', 'eye:', 'nat split (gl3'))]
bt = rd(A, 'battery.txt')
print('== BATTERY: %d lines' % len(bt))
for s in bt:
    if s.startswith(('start ', 'end ', 'HOME ')): print('  ', s[:170])
pc = [int(m.group(1)) for m in (re.search(r'vb -?\d+: (\d+)%', s) for s in bt) if m]
print('   levels: first %s, min %s, last %s; lines with external power 1: %d' % (pc[0] if pc else '-', min(pc) if pc else '-', pc[-1] if pc else '-',
      sum('external power 1' in s for s in bt)))
print('== PICTURES (gl35 against gl34, same names; pixel data only; luma = (2R+5G+B)/8; count of pixels, lighter/darker by 6+):')
na = sorted(n for n in os.listdir(A) if n.lower().endswith('.bmp')); nb = set(n for n in os.listdir(B) if n.lower().endswith('.bmp'))
same = 0
for n in na:
    if n not in nb: print('  %s: only in gl35' % n); continue
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
print('   %d of %d gl35 pictures byte-equal gl34\'s; only in gl34: %s' % (same, len(na), sorted(nb - set(na))))
