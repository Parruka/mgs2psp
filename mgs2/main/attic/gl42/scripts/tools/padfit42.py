#!/usr/bin/env python3
"""padfit42.py TRIAL_ELF OBJDIR RO ME_PRE ME XL END [TARGET_ELF] (python3 -I; gl42): the data pads that put a trial link's data where the
target build has it against the data cache's 8 KB ways (address mod 0x2000), as gl40 chose its pads (attic/gl40/state.md B6-B8):
.rodata (RO_PAD), the hot block .me_bss.h39 (ME_PRE), xl_psp.o's and ge_xlive.o's .me_bss (ME_PAD), the rest of .me_bss (XL_PAD), and
the image end, so the heap block (END_PAD).  The trial's own pads are given (RO >= 1; ME_PRE 0 = me_sec_pad3.ld with no pad).  Each
group's shift is the most common (trial - target) mod 0x2000 of the symbols both ELFs define in it.  TARGET: gl40's final build
(build_gl40/psp: rodata, data and .me_bss at gl39's offsets, the heap block at gl36's address), which gl41 keeps too."""
import sys, subprocess, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'; OD = '/home/claude/pspsdk/pspdev/bin/psp-objdump'
T, OBJ = sys.argv[1], sys.argv[2]; RO, PRE, MEP, XLP, END = (int(x, 0) for x in sys.argv[3:8])
G = sys.argv[8] if len(sys.argv) > 8 else '/home/claude/main/build_gl40/psp/MGS2T.elf'
W = 0x2000
def syms(e):
    S = {}; d = set()
    for l in subprocess.run([NM, '-S', '--defined-only', e], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'bBdDrR':
            if f[3] in S: d.add(f[3])
            S[f[3]] = int(f[0], 16)
        elif len(f) == 3 and f[1] in 'bBdD': S.setdefault(f[2], int(f[0], 16))
    for x in d: S.pop(x, None)
    return S
def secs(e):
    R = {}
    for l in subprocess.run([OD, '-h', e], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) >= 7 and f[0].isdigit(): R[f[1]] = (int(f[3], 16), int(f[2], 16))
    return R
def objnames(o):
    N = set()
    for l in subprocess.run([OD, '-t', OBJ + '/' + o], capture_output=True, text=True).stdout.split('\n'):
        if '.me_bss' in l and '.me_bss.h39' not in l:
            f = l.split()
            if f: N.add(f[-1])
    return N
A, B = syms(T), syms(G); SA, SB = secs(T), secs(G)
XLN = objnames('xl_psp.o') | objnames('ge_xlive.o')
def sg(x): x %= W; return x - W if x >= W // 2 else x
def grp(lo, hi, pick):
    c = collections.Counter(); n = 0
    for k, a in A.items():
        if k in B and lo <= a < hi and pick(k): c[sg(a - B[k])] += 1; n += 1
    return c.most_common(3), n
ms, me, h39 = A['__me_bss_start'], A['__me_bss_end'], A['__me_h39_end']
g_h39 = grp(ms, h39, lambda k: True); g_xl = grp(h39, me, lambda k: k in XLN); g_rest = grp(h39, me, lambda k: k not in XLN and not k.startswith('__'))
s_ro = sg(SA['.rodata'][0] - SB['.rodata'][0]); s_da = sg(SA['.data'][0] - SB['.data'][0]); s_bss = sg(SA['.bss'][0] - SB['.bss'][0])
print('trial %s (pads RO %#x ME_PRE %#x ME %#x XL %#x END %#x) against %s, shifts (trial - target) mod 0x2000:' % (T, RO, PRE, MEP, XLP, END, G))
print('  .rodata %+#x  .data %+#x  .bss head %+#x' % (s_ro, s_da, s_bss))
print('  .me_bss h39 block %s (of %d names)  xl_psp/ge_xlive %s (of %d)  the rest %s (of %d)' % (g_h39[0], g_h39[1], g_xl[0], g_xl[1], g_rest[0], g_rest[1]))
sh, sx, sr = g_h39[0][0][0], g_xl[0][0][0], g_rest[0][0][0]
def pm(p, s): v = (p - s) % W; return v
ro_r = pm(RO, s_ro) or W; ro_d = pm(RO, s_da) or W
pre = pm(PRE, sh); mep = pm(MEP, sx - sh); xlp = pm(XLP, sr - sx)
endt = A['_end'] + (pre - PRE) + (mep - MEP) + (xlp - XLP); s_end = sg(endt - B['_end']); end = pm(END, s_end)
print('PADS: RO_PAD %#x (.rodata at the target\'s offset; .data then %+#x) or %#x (.data at the target\'s; .rodata then %+#x)' % (ro_r, sg(s_da - s_ro), ro_d, sg(s_ro - s_da)))
print('      ME_PRE %#x  ME_PAD %#x  XL_PAD %#x  END_PAD %#x  (image end then %#x, the target\'s %#x; bytes of pads in the heap\'s way: %d)' % (
      pre, mep, xlp, end, endt + end - END, B['_end'], pre + mep + xlp + end))
