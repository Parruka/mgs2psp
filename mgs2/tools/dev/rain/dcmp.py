# dcmp.py <dump a> <dump b> [watch hex addr ...]: EE RAM dumps of two runs at the same vblank (task #164): ranges of differing words
# (merged across gaps under 256 bytes), by size, with region labels; the watched words' values in both
import sys, numpy as np
a = np.fromfile(sys.argv[1], dtype='<u4'); b = np.fromfile(sys.argv[2], dtype='<u4')
d = np.nonzero(a != b)[0]
print('words that differ: %d of %d' % (len(d), len(a)))
def lab(x):
    if x < 0x100000: return 'below the ELF (kernel / system)'
    if x < 0x17cf78: return 'ELF code'
    if x < 0x1f1c00: return 'ELF data / bss'
    if x < 0x1f1c00 + 0x21d000: return 'stage image (w01a)'
    return 'above the stage image'
rng = []
if len(d):
    s = p = d[0]
    for w in d[1:]:
        if (w - p) * 4 > 256: rng.append((s, p)); s = w
        p = w
    rng.append((s, p))
by = {}
for s, e in rng:
    k = lab(s * 4); n = int(np.count_nonzero(a[s:e + 1] != b[s:e + 1])); by[k] = by.get(k, 0) + n
for k, v in sorted(by.items(), key=lambda x: -x[1]): print('  %-34s %8d words' % (k, v))
print('ranges: %d; the first 60 by address:' % len(rng))
for s, e in rng[:60]:
    n = int(np.count_nonzero(a[s:e + 1] != b[s:e + 1]))
    print('  %07x-%07x %6d B, %5d words differ  %s' % (s * 4, e * 4 + 3, (e - s + 1) * 4, n, lab(s * 4)))
for x in sys.argv[3:]:
    i = int(x, 16) // 4; print('word %07x: %08x / %08x' % (i * 4, a[i], b[i]))
