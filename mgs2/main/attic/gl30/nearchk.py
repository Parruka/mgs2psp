import sys, os
# nearchk.py GERDIR... (gl30 G4 follow-up): for each X target, the nearest candidate that is an exact or a subset base, against the nearest
# exact base: would "stop at the first of either" change any pairing on this route? (subana.py's rules)
exec(open(os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), 'subana.py')).read().split('tot = collections.Counter()')[0])
import collections
T = collections.Counter(); walk = collections.Counter()
fs = [os.path.join(d, f) for d in sys.argv[1:] for f in sorted(os.listdir(d)) if f.endswith('.ger')]
for fn in fs:
    vb, B = load(fn)
    for d in B:
        if d['kind'] != 1 or not (d['flags'] & 16) or not (d['flags'] & 4) or d['sig'] != 0x42: continue
        cands = [c for c in B[max(0, d['i'] - 64):d['i']] if c['kind'] == 1 and (c['flags'] & 32) and not ((c['flags'] & 4) and c['sig'] == 0x42)
                 and c['xw'][9:12] == d['xw'][9:12]]
        cands.reverse(); first = None; exact = None; n = 0
        for c in cands:
            n += 1
            if c['xw'] != d['xw']: continue
            dmin = min(d['S']); bs = sorted(c['S']); isx = iss = False
            if len(c['S']) == len(d['S']):
                o = sh(dmin, bs[0])
                isx = max(map(abs, o)) <= 64 and set(sh(p, o) for p in d['S']) == c['S'] and set(tuple(sorted(sh(p, o) for p in t)) for t in d['T']) == c['T']
            if not isx:
                for q in bs:
                    o = sh(dmin, q)
                    if max(map(abs, o)) <= 64 and set(sh(p, o) for p in d['S']) <= c['S'] and set(tuple(sorted(sh(p, o) for p in t)) for t in d['T']) <= c['T']:
                        iss = True; break
            if first is None and (isx or iss): first = ('exact' if isx else 'subset', c['i'], n)
            if isx: exact = (c['i'], n); break
        T['targets'] += 1
        if exact and first[0] == 'subset': T['a subset base nearer than the exact base'] += 1
        T['walk to the end (no exact)' if not exact else 'walk stops at the exact base'] += 1
        walk['candidates walked, as built'] += len(cands) if not exact else exact[1]
        walk['candidates walked, first of either'] += first[2] if first else len(cands)
print(dict(T)); print(dict(walk))
