#!/usr/bin/env python3
"""dh39.py ELF_A DHIST_A ELF_B DHIST_B [TOP] (python3 -I): PspProf PSPPROF_DHIST=64 files (D-misses by 64-byte line of the data address)
of two builds in the same window, by the main ELF's data symbol (static data) or, outside the ELF's image, by 1 MB of address.
D-misses a PSP step (the window's 80 steps); B minus A."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def syms(elf):
    S = []; end = 0
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'dDbBrRsSgG': S.append((int(f[0], 16), int(f[1], 16), f[3]))
        if len(f) >= 3 and f[-1] in ('_end', 'end'): end = max(end, int(f[0], 16))
    S.sort(); return S, [s[0] for s in S], end
def load(elf, dh):
    S, st, end = syms(elf); R = collections.Counter(); sec = collections.Counter()
    for ln in open(dh):
        a, c = ln.split(); a = int(a, 16); c = int(c) / 80.0
        k = bisect.bisect_right(st, a + 63) - 1; n = None
        while k >= 0 and S[k][0] + max(S[k][1], 1) > a - 4096:
            if S[k][0] < a + 64 and S[k][0] + max(S[k][1], 1) > a: n = S[k][2]; break
            k -= 1
        if n is None: n = '(static, no symbol)' if a < end else '(dynamic %05x000)' % (a >> 12 & ~0xff)
        R[n] += c; sec['static' if a < end else 'dynamic (heap, game RAM, stacks)'] += c
    return R, sec, end
A, sa, ea = load(sys.argv[1], sys.argv[2]); B, sb, eb = load(sys.argv[3], sys.argv[4]); top = int(sys.argv[5]) if len(sys.argv) > 5 else 30
print('# D-misses a PSP step: A %s (image end %08x), B %s (image end %08x)' % (sys.argv[2], ea, sys.argv[4], eb))
for k in sorted(set(sa) | set(sb)): print('  %-36s A %9.0f  B %9.0f  B-A %+8.0f  (%+.2f ms a step)' % (k, sa[k], sb[k], sb[k] - sa[k], 86 * (sb[k] - sa[k]) / 333000))
st = lambda R: sum(v for k, v in R.items() if not k.startswith('(dynamic'))
print(' -- static data by symbol, largest changes')
for n in sorted((n for n in set(A) | set(B) if not n.startswith('(dynamic')), key=lambda n: -abs(B[n] - A[n]))[:top]:
    print('   %-44s A %8.0f  B %8.0f  B-A %+8.0f' % (n[:44], A[n], B[n], B[n] - A[n]))
print(' -- dynamic memory by 1 MB, largest changes (the two builds'' addresses can differ)')
for n in sorted((n for n in set(A) | set(B) if n.startswith('(dynamic')), key=lambda n: -abs(B[n] - A[n]))[:12]:
    print('   %-44s A %8.0f  B %8.0f  B-A %+8.0f' % (n, A[n], B[n], B[n] - A[n]))
