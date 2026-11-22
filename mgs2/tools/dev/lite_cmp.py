#!/usr/bin/env python3
"""lite_cmp.py <full run log> <reduced run log>: checks the reduced draw pipeline (boot.c rt_dg_call with OPT_LITE, or the dev
MGS2_DG_LITE) against the full one, on x86 logs with MGS2_PKT_HASH (and MGS2_GS_MASK for gs).  The reduced run's "dgc: vb N L" lines
say which calls were reduced (L = 1).  The packets sent during vblank v were built by the last call before v (2026-10-21: a reduced call
changes only the next vblank's packets), and a hash line at vblank v shows the state after the last call before v.
 - pkt: every vblank whose packets come from a full call must hash the same in both runs (the shown steps look the same);
 - gs (game state without renderer memory): equal at the hash lines that follow a full call; after a reduced call the scratchpad and
   packet-stream words it leaves differ until the next full call rewrites them (counted for information);
 - iop, vfb: equal at every hash line."""
import re, sys, bisect
def load(fn):
    h, p, calls = {}, {}, []
    for l in open(fn, errors='replace'):
        if 'hash: vb' in l:
            l = l[l.index('hash: vb'):]; m = dict(re.findall(r'\b(vb|ee|iop|gs|vfb) (\S+)', l)); h[int(m['vb'])] = m
        elif l.startswith('pkt: vb'):
            q = l.split(); p[int(q[2])] = q[3]
        elif l.startswith('dgc: vb'):
            q = l.split(); calls.append((int(q[2]), int(q[3])))
    return h, p, calls
a, pa, _ = load(sys.argv[1]); b, pb, calls = load(sys.argv[2])
cv = [c[0] for c in calls]
def last_call(v):                       # the reduced run's last call before vblank v: (vb, reduced) or None
    i = bisect.bisect_left(cv, v) - 1
    return calls[i] if i >= 0 else None
nl = sum(c[1] for c in calls)
print('reduced run: %d pipeline calls in the packet-hash window, %d reduced' % (len(calls), nl))
ps = sorted(set(pa) & set(pb)); full = []; red = []
for v in ps:
    c = last_call(v)
    if c is None: continue
    (red if c[1] else full).append(v)
d = [v for v in full if pa[v] != pb[v]]
print('pkt  %5d of %5d vblanks sending a full call\'s packets differ%s' % (len(d), len(full), (': first ' + ' '.join(map(str, d[:8]))) if d else ''))
dr = [v for v in red if pa[v] != pb[v]]
print('     (vblanks sending a reduced call\'s packets: %d of %d differ)' % (len(dr), len(red)))
vs = sorted(set(a) & set(b)); af = []; ar = []
for v in vs:
    c = last_call(v)
    (ar if c and c[1] else af).append(v)
for k in ('gs', 'iop', 'vfb', 'ee'):
    if k == 'gs' and not any('gs' in a[v] for v in vs): continue
    if k in ('gs', 'ee'):
        d = [v for v in af if a[v].get(k) != b[v].get(k)]; dr = [v for v in ar if a[v].get(k) != b[v].get(k)]
        print('%-4s %5d of %5d hash lines after a full call differ%s; after a reduced call %d of %d' % (k, len(d), len(af),
              (': first ' + ' '.join(map(str, d[:8]))) if d else '', len(dr), len(ar)))
    else:
        d = [v for v in vs if a[v].get(k) != b[v].get(k)]
        print('%-4s %5d of %5d hash lines differ%s' % (k, len(d), len(vs), (': first ' + ' '.join(map(str, d[:8]))) if d else ''))
