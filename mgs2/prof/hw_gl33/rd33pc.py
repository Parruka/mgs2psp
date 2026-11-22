# rd33pc.py: read an MGS2 harness PSP run on the PC (gl33 against gl31). python -I rd33pc.py <LOGS\gl33> <LOGS\gl31>
import sys, os, re, hashlib
A, B = sys.argv[1], sys.argv[2]
EH = '66499cdb584a'                      # 62 hash lines 'vb ee iop' (gl33 emulator run e4 = hashref c9ddb593)
ED, EDZ = 'b4bf989fb1d6', '7d2f93ab5f56'  # 916 digest lines 'vb list frame' / with the dz text
EB = dict(zip(range(294, 314), '0138e21f8c33 fc3116aea28f 697347b6ee5d 6b9ccff1ae97 5e5329e4d835 8c8303691acf 57c9ce01f7c1 9079a20e4284 6a3906ca1707 29854c27117d 6d8173c7a278 9b9ac3c4a76e 295708a5cb51 33fb7b2aeb2a fb61643000fe 67df445ed73f 3559082ade57 3ef2bda8e0f5 9848929c60f7 94f522d0e8b8'.split()))
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
    if s.startswith(('picture: ', 'native step 1 totals')) and ('frames made' in s or 'totals' in s): print('  ', s[:700])
print('== CRASH:'); [print('  ', s[:200]) for s in rd(A, 'crash.txt')[:12]]
H, D = [], []
for s in la:
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', s)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', s)
    if m: D.append((int(m.group(1)), '%s %s %s' % m.groups()[:3], m.group(4).strip()))
print('== HASHES: %d lines, %s (expected 62, %s): %s' % (len(H), md(H), EH, 'EQUAL' if md(H) == EH and len(H) == 62 else 'DIFFERENT'))
if md(H) != EH: [print('  H', h) for h in H]
dd, ddz = md([a for _, a, _ in D]), md([a + ' ' + z for _, a, z in D])
print('== DIGESTS: %d lines, %s / with dz %s (expected 916, %s / %s): %s' % (len(D), dd, ddz, ED, EDZ, 'EQUAL' if (dd, ddz) == (ED, EDZ) else 'DIFFERENT'))
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
        m = re.match(r'nat turn vb (\d+)-\d+ (me|main) turn arm (\d+)(.*)', s)
        if m:
            r = m.group(4); a = int(m.group(1))
            prob = {k: num(p, r, 0) for k, p in (('builder', r'builder run instead \(dropped records\) (\d+)'), ('noarr', r'recorded with no array (\d+)'),
                    ('early', r'early fills for an array.s reuse (\d+)'), ('drop', r'fed word by word \d+, dropped (\d+)'), ('partly', r'partly \(dropped\) (\d+)'))}
            T.setdefault(a, {}).update(kind=m.group(2), arm=int(m.group(3)), nat=num(r'native time ([\d.]+)', r), incopy=num(r'data in the copy (\d+)', r),
                inram=num(r'read from RAM (\d+)', r), prob={k: v for k, v in prob.items() if v}); continue
        m = re.match(r'nat inpl vb (\d+)-\d+ \w+ turn arm \d+ inpl (\d+).*groups flagged static (\d+)', s)
        if m: T.setdefault(int(m.group(1)), {}).update(inpl=int(m.group(2)), static=int(m.group(3)))
    return T
ta, tb = turns(la), turns(lb)
def f(x): return '-' if x is None else ('%.1f' % x if isinstance(x, float) else str(x))
print('== TURNS (vb start; per log: mean ms a step, logic, GE wait; ME turn ms, walks, frame ends+lists, idle, main waited for fills; nat arm, inpl, native ms, in copy/from RAM):')
print('vb k | gl33: mean logic gew | me walks fl idle wfill | arm inpl nat copy/ram || gl31: mean logic gew | me walks fl idle wfill | arm nat copy/ram')
probs, flipoff, shown = [], 0, 0
for a in sorted(k for k in ta if 24000 <= k < 31400):
    x, y = ta[a], tb.get(a, {})
    if x.get('prob'): probs.append((a, x['prob']))
    flipoff += x.get('flip') == 'off'; shown += x.get('shown', 0)
    print('%d %s | %s %s %s | %s %s %s %s %s | %s %s %s %s/%s || %s %s %s | %s %s %s %s %s | %s %s %s/%s' % (a, (x.get('kind') or '?')[:2],
          f(x.get('mean')), f(x.get('logic')), f(x.get('gew')), f(x.get('me')), f(x.get('walks')), f(x.get('fl')), f(x.get('idle')), f(x.get('wfill')),
          f(x.get('arm')), f(x.get('inpl')), f(x.get('nat')), f(x.get('incopy')), f(x.get('inram')),
          f(y.get('mean')), f(y.get('logic')), f(y.get('gew')), f(y.get('me')), f(y.get('walks')), f(y.get('fl')), f(y.get('idle')), f(y.get('wfill')),
          f(y.get('arm')), f(y.get('nat')), f(y.get('incopy')), f(y.get('inram'))))
print('== NAT PROBLEMS (gl33):', probs or 'none', '| display: turns with flip wait off', flipoff, ', lists drawn into the shown buffer', shown)
print('== LINES:'); [print('  ', s[:300]) for s in la if s.startswith(('nat:', 'native step 1:', 'picture heap', 'eye:'))]
bt = rd(A, 'battery.txt')
print('== BATTERY: %d lines' % len(bt))
for s in bt:
    if s.startswith(('start ', 'end ', 'HOME ')): print('  ', s[:170])
pc = [int(m.group(1)) for m in (re.search(r'vb -?\d+: (\d+)%', s) for s in bt) if m]
print('   levels: first %s, min %s, last %s; lines with external power 1: %d' % (pc[0] if pc else '-', min(pc) if pc else '-', pc[-1] if pc else '-',
      sum('external power 1' in s for s in bt)))
print('== PICTURES (gl33 against gl31, same names; pixel data only; luma = (2R+5G+B)/8; count of pixels, lighter/darker by 6+):')
na = sorted(n for n in os.listdir(A) if n.lower().endswith('.bmp')); nb = set(n for n in os.listdir(B) if n.lower().endswith('.bmp'))
same = 0
for n in na:
    if n not in nb: print('  %s: only in gl33' % n); continue
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
print('   %d of %d gl33 pictures byte-equal gl31\'s; only in gl31: %s' % (same, len(na), sorted(nb - set(na))))
