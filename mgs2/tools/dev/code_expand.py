#!/usr/bin/env python3
"""code_expand.py <generated header> <object> [<object> ...]: PSP bytes per translated function (psp-nm -S) against the guest bytes it
translates (its lowest to highest /*addr*/ line + 8), summed and as percentiles (speed_notes §48)."""
import re, sys, subprocess, collections
hdr, *objs = sys.argv[1:]
G = {}; cur = None
for ln in open(hdr, errors='replace'):
    m = re.match(r'^static void (P[0-9a-f]+|S_\w+_P[0-9a-f]+)\(void\)$', ln.strip())
    if m: cur = m.group(1); G[cur] = [1 << 40, 0]; continue
    m = re.match(r'^/\*([0-9a-f]{5,7})\*/', ln)
    if m and cur:
        a = int(m.group(1), 16); g = G[cur]; g[0] = min(g[0], a); g[1] = max(g[1], a)
H = {}
for o in objs:
    out = subprocess.run(['/home/claude/pspsdk/pspdev/bin/psp-nm', '-S', o], capture_output=True, text=True).stdout
    for l in out.splitlines():
        p = l.split()
        if len(p) == 4 and p[2] in 'tT': H[p[3]] = H.get(p[3], 0) + int(p[1], 16)
gb = hb = n = 0; R = []
for f, (lo, hi) in G.items():
    if f in H and hi >= lo:
        g = hi - lo + 8; gb += g; hb += H[f]; n += 1; R.append((H[f] / g, f, g, H[f]))
print('functions %d: guest %d bytes, host %d bytes, ratio %.2f' % (n, gb, hb, hb / gb))
R.sort()
for q in (0.1, 0.5, 0.9): print('  ratio p%d %.2f' % (int(q * 100), R[int(q * len(R))][0]))
