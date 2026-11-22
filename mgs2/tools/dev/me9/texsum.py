# texsum.py <log>: me9 (task #130) the texture check's windows (MGS2_XLIVE_TEX=1): converted texels and palette entries that differ
# from what the uploads sent (GU_SENT), per window with any, and totals.  Counts only.
import re, sys
R = re.compile(r'xlive tex: vb (\d+)\.\.(\d+):.*?not convertible \d+: texels (\d+), unknown (\d+), known one way (\d+), differing (\d+); '
               r'palettes \d+: entries (\d+), unknown (\d+), known one way (\d+), differing (\d+)')
tot = [0] * 8; rows = []
for l in open(sys.argv[1], errors='replace'):
    m = R.search(l)
    if not m: continue
    v = [int(x) for x in m.groups()]
    for i in range(8): tot[i] += v[2 + i]
    if v[5] or v[9]: rows.append(v)
print('windows with differing texels or palette entries: %d' % len(rows))
for v in rows: print('  vb %d-%d: texels %d, differing %d (%.4f%%); palette entries %d, differing %d' % (v[0], v[1], v[2], v[5], 100.0 * v[5] / max(v[2], 1), v[6], v[9]))
print('totals: texels %d (unknown %d, one way %d), differing %d (%.5f%%); palette entries %d, differing %d' % (tot[0], tot[1], tot[2], tot[3], 100.0 * tot[3] / max(tot[0], 1), tot[4], tot[7]))
