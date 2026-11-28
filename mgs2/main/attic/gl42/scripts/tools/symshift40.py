#!/usr/bin/env python3
"""symshift40.py ELF_A ELF_B [DHIST_A ...] (python3 -I; gl40): where the main ELF's data symbols moved against the data cache's 8 KB
ways (D-cache 16 KB, two ways: an address's set is address mod 0x2000).  For each data symbol both ELFs define (by name; names
defined more than once are skipped), the shift (B - A) mod 0x2000 as a signed number; printed by section of B and by shift, with the
symbols' D-misses a PSP step in A's DHIST files (PSPPROF_DHIST=64 of A's model windows; 80 steps a window), so the shifts that matter
come first.  Also the image end (_end) of both: the heap block starts after it."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; OD = '/home/claude/pspsdk/pspdev/bin/psp-objdump'
def syms(elf):
    S = {}; dup = set(); end = None
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'dDbBrRsSgG':
            if f[3] in S: dup.add(f[3])
            S[f[3]] = (int(f[0], 16), int(f[1], 16))
        if len(f) >= 3 and f[-1] == '_end': end = int(f[0], 16)
    for d in dup: S.pop(d, None)
    return S, end
def secs(elf):
    R = []
    for l in subprocess.run([OD, '-h', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) >= 7 and f[0].isdigit(): R.append((int(f[3], 16), int(f[2], 16), f[1]))
    return sorted(R)
A, ea = syms(sys.argv[1]); B, eb = syms(sys.argv[2]); SB = secs(sys.argv[2])
W = collections.Counter()
if len(sys.argv) > 3:
    lst = sorted((a, s, n) for n, (a, s) in A.items()); st = [x[0] for x in lst]
    for dh in sys.argv[3:]:
        for ln in open(dh):
            a, c = ln.split(); a = int(a, 16); c = int(c) / 80.0
            k = bisect.bisect_right(st, a + 63) - 1
            while k >= 0 and lst[k][0] + max(lst[k][1], 1) > a - 65536:
                if lst[k][0] < a + 64 and lst[k][0] + max(lst[k][1], 1) > a: W[lst[k][2]] += c; break
                k -= 1
def sec(a):
    for s, z, n in SB:
        if s <= a < s + max(z, 1): return n
    return '?'
G = collections.defaultdict(lambda: [0, 0.0, []])
for n, (b, z) in B.items():
    if n not in A: continue
    d = (b - A[n][0]) % 0x2000; d = d - 0x2000 if d >= 0x1000 else d
    g = G[(sec(b), d)]; g[0] += 1; g[1] += W.get(n, 0.0); g[2].append((W.get(n, 0.0), n))
print('image end (_end): A %08x B %08x, B - A %+d (mod 0x2000 %+#x)' % (ea or 0, eb or 0, (eb or 0) - (ea or 0), (((eb or 0) - (ea or 0)) % 0x2000)))
print('section of B | shift B - A mod 0x2000 | symbols | D-misses a step in A (DHIST) | the heaviest')
for (s, d), (c, w, L) in sorted(G.items(), key=lambda kv: (-kv[1][1], kv[0])):
    if c < 3 and w < 1: continue
    L.sort(reverse=True); print('  %-14s %+7d (%+#06x) | %5d | %9.1f | %s' % (s, d, d, c, w, ', '.join('%s %.0f' % (n, x) for x, n in L[:5])))
