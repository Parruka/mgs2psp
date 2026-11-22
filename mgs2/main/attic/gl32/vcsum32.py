#!/usr/bin/env python3
"""vcsum32.py TWINLOG [REFLOG]   (gl32; run with python3 -I)
The x86 twin's vertex cache lines of one run (boot/nat_x86.c "natvc:", "natst:"): the check mode's totals, the cache's counts by
window, and, with REFLOG (a run of the same route without the cache), whether the two runs' hash lines and picture lines are equal
(the frame's list and frame digests "picture: vb", the content digests, natb / natq / natz, the "xlive: vb" lines)."""
import sys, re, hashlib
log = sys.argv[1]; ref = sys.argv[2] if len(sys.argv) > 2 else None
KEYS = ['hash: vb', 'picture: vb', 'xlive: vb', 'natb:', 'natq:', 'natz:']
def digs(p):
    h = {k: hashlib.md5() for k in KEYS}; n = {k: 0 for k in KEYS}; tail = []
    for ln in open(p, errors='replace'):
        for k in KEYS:
            if ln.startswith(k): h[k].update(ln.encode()); n[k] += 1; break
        if ln.startswith('picture: ') and ('frames made' in ln or 'content digests' in ln): tail.append(ln.strip()[:60] + ' ... ' + ln.strip()[-40:])
    return {k: (n[k], h[k].hexdigest()[:12]) for k in KEYS}, tail
rows = []; st = []; setup = []
for ln in open(log, errors='replace'):
    if ln.startswith('natst:'): st.append(ln.strip())
    if ln.startswith('natvc: the') or ln.startswith('nat: gl32'): setup.append(ln.strip())
    if not ln.startswith('natvc: vb'): continue
    m = re.match(r'natvc: vb (\d+)-(\d+)', ln); v0 = int(m.group(1))
    g = lambda pat: [int(x) for x in re.search(pat, ln).groups()]
    r = dict(v0=v0, vc=g(r' vc (\d+) ')[0], arm=g(r' arm (\d+) ')[0])
    r['fb'], r['fv'], r['fn'] = g(r'found box (\d+), vertices (\d+), nothing-drawn (\d+)'); r['nf'], = g(r'not found (\d+)'); r['st'], = g(r'stored (\d+)')
    r['dg'], r['ds'], r['dd'] = g(r'dropped: generation (\d+), slots (\d+), data (\d+)'); r['ck'], r['bad'] = g(r'check: (\d+) compared, (\d+) DIFFER')
    r['ns'], r['kb'], r['mns'], r['mkb'] = g(r'now (\d+) slots, (\d+) KB data \(most (\d+), (\d+) KB\)'); r['byp'], = g(r'not looked up (\d+)'); r['al'], = g(r'alpha rewritten (\d+)')
    r['nos'], = g(r'not stored (\d+)'); r['ev'] = g(r'events: made (\d+), freed (\d+), lit (\d+), stage (\d+)'); r['stat'], r['statd'] = g(r'flagged static (\d+) \(drawn (\d+)\)')
    rows.append(r)
for s in setup: print(s)
S = lambda k, R=rows: sum(r[k] for r in R)
print('%s: natvc lines %d; static groups flagged %d (drawn %d); check mode: %d found entries converted again, %d DIFFER; not looked up %d; alpha rewritten %d; '
      'not stored %d; dropped: generation %d, slots %d, data %d; most in use %d slots, %d KB' % (log, len(rows), S('stat'), S('statd'), S('ck'), S('bad'), S('byp'), S('al'), S('nos'),
      S('dg'), S('ds'), S('dd'), max([r['mns'] for r in rows] or [0]), max([r['mkb'] for r in rows] or [0])))
for (a, b) in [(24000, 29000), (29000, 29400), (29400, 31400), (29600, 30500), (30900, 31300), (31400, 37800)]:
    for vc in sorted(set(r['vc'] for r in rows)):
        R = [r for r in rows if a <= r['v0'] < b and r['vc'] == vc]
        if not R: continue
        look = S('fb', R) + S('fv', R) + S('fn', R) + S('nf', R)
        print('  vb %5d-%5d vc %d: %2d lines; static drawn %7d; looked up %7d: found box %6d, vertices %6d, nothing-drawn %d, not found %5d (%.2f%%); stored %5d; drops gen %d slots %d data %d; '
              'most now %d slots %d KB; events made %d freed %d lit %d stage %d' % (a, b, vc, len(R), S('statd', R), look, S('fb', R), S('fv', R), S('fn', R), S('nf', R),
              100.0 * S('nf', R) / max(1, look), S('st', R), S('dg', R), S('ds', R), S('dd', R), max(r['ns'] for r in R), max(r['kb'] for r in R),
              sum(r['ev'][0] for r in R), sum(r['ev'][1] for r in R), sum(r['ev'][2] for r in R), sum(r['ev'][3] for r in R)))
for s in st[-45:]: print(s[:400])
d, t = digs(log); print('lines (count, md5): ' + ', '.join('%s %d %s' % (k, d[k][0], d[k][1]) for k in KEYS))
for x in t: print('  ' + x)
if ref:
    e, t2 = digs(ref); print('reference %s: ' % ref + ', '.join('%s %d %s' % (k, e[k][0], e[k][1]) for k in KEYS))
    print('EQUAL to the reference: ' + ', '.join('%s %s' % (k, 'yes' if d[k] == e[k] else 'NO') for k in KEYS))
