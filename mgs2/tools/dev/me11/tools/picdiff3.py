#!/usr/bin/env python3
# picdiff3.py <ref log> <log> [...]: me11 (task #142): frames whose GE list, frame or content digest differs from the reference run's, by
# vblank ("picture: vb N buffer B list L frame F: ..." and "picture: vb N content C" lines), with the first few and the vb stretches.
import sys, re
rl = re.compile(r'^picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+)')
rc = re.compile(r'^picture: vb (\d+) content ([0-9a-f]+)')
def load(fn):
    d = {}
    for line in open(fn, errors='replace'):
        m = rl.match(line)
        if m:
            d.setdefault(int(m.group(1)), {})['lf'] = (m.group(2), m.group(3)); continue
        m = rc.match(line)
        if m:
            d.setdefault(int(m.group(1)), {})['c'] = m.group(2)
    return d
ref = load(sys.argv[1])
for fn in sys.argv[2:]:
    d = load(fn)
    vbs = sorted(set(ref) | set(d))
    miss = [v for v in vbs if v not in d or v not in ref]
    dl = [v for v in vbs if v in d and v in ref and d[v].get('lf', ('',''))[0] != ref[v].get('lf', ('',''))[0]]
    dfr = [v for v in vbs if v in d and v in ref and d[v].get('lf') != ref[v].get('lf')]
    dc = [v for v in vbs if v in d and v in ref and d[v].get('c') != ref[v].get('c')]
    anyd = sorted(set(dfr) | set(dc))
    st = []
    for v in anyd:
        if st and v - st[-1][1] <= 200: st[-1][1] = v; st[-1][2] += 1
        else: st.append([v, v, 1])
    print('%s: %d frames (ref %d), %d missing on one side; differing: list %d, list or frame %d, content %d, any %d' % (fn, len(d), len(ref), len(miss), len(dl), len(dfr), len(dc), len(anyd)))
    print('  first: %s' % ' '.join(str(v) for v in anyd[:12]))
    print('  stretches (gap <= 200 vb): %s' % '; '.join('%d-%d (%d)' % (a, b, n) for a, b, n in st[:30]))
