"""elfcmp.py <v24 MGS2T.elf> <new MGS2T.elf>: where v24's symbols went.  Per output section of v24: how many symbols kept their address,
how many moved and by how much (mod 8 KB: the PSP's caches index 8 KB per way); unique names only (name, type, size)."""
import subprocess, sys, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; OD = '/home/claude/pspsdk/pspdev/bin/psp-objdump'
def secs(f):
    S = []
    for l in subprocess.run([OD, '-h', f], capture_output=True, text=True).stdout.split('\n'):
        p = l.split()
        if len(p) >= 7 and p[0].isdigit(): S.append((p[1], int(p[3], 16), int(p[2], 16)))
    return S
def syms(f):
    c = collections.Counter(); d = {}
    for l in subprocess.run([NM, '-S', '--defined-only', f], capture_output=True, text=True).stdout.split('\n'):
        p = l.split()
        if len(p) == 4: k = (p[3], p[2], int(p[1], 16)); a = int(p[0], 16)
        elif len(p) == 3: k = (p[2], p[1], -1); a = int(p[0], 16)
        else: continue
        c[k] += 1; d[k] = a
    return {k: a for k, a in d.items() if c[k] == 1}
a, b = sys.argv[1:3]; SA = secs(a); A = syms(a); B = syms(b)
def sec_of(x):
    for n, v, s in SA:
        if v and v <= x < v + max(s, 1): return n
    return '?'
R = collections.defaultdict(collections.Counter); miss = collections.Counter(); ex = {}
for k, x in A.items():
    s = sec_of(x)
    if k not in B: miss[s] += 1; continue
    dl = B[k] - x; R[s][dl] += 1; ex.setdefault((s, dl), k[0])
for n, v, sz in SA:
    if n not in R and not miss[n]: continue
    tot = sum(R[n].values())
    print('%-22s %6d symbols:' % (n, tot), ', '.join('%+#x (mod 8K %#x) x%d e.g. %s' % (dl, dl % 8192, c, ex[(n, dl)]) for dl, c in R[n].most_common(6)),
          ('; %d not found' % miss[n]) if miss[n] else '')
