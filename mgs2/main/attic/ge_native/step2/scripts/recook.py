# recook.py LOG: the people's re-cooks (the 'nge6 re' lines, MGS2_NGE6_DBG=1): by reason, meshes, and whether the new bases were seen before for that mesh
import gzip, re, sys, collections
rx = re.compile(r'^nge6 re vb (\d+) why (\d+) mesh (\w+) ng (\d+) desc (\w+)/(\w+) a (\w+)/(\w+) b (\w+)/(\w+) c (\w+)/(\w+)')
why = collections.Counter(); mesh = collections.Counter(); seen = collections.defaultdict(set); back = 0; n = 0; ngs = collections.Counter()
deltas = collections.Counter(); byvb = collections.Counter()
for l in gzip.open(sys.argv[1], 'rt', errors='replace'):
    m = rx.match(l)
    if not m: continue
    vb, w, ms, ng, d0, d1, a0, a1, b0, b1, c0, c1 = m.groups(); n += 1
    why[w] += 1; mesh[ms] += 1; ngs[int(ng)] += 1; byvb[int(vb) // 2000 * 2000] += 1
    st = seen[ms]
    if not st: st.add((a0, b0, c0))
    if (a1, b1, c1) in st: back += 1
    st.add((a1, b1, c1))
    deltas[(int(a1, 16) - int(a0, 16))] += 1
print('re-cooks', n, 'by reason', dict(why), 'meshes', len(mesh), 'top', mesh.most_common(5))
print('new bases seen before for the mesh', back, '; states a mesh had: max', max(len(s) for s in seen.values()) if seen else 0,
      'mean %.1f' % (sum(len(s) for s in seen.values()) / max(1, len(seen))))
print('stream A base deltas (top)', deltas.most_common(8))
print('by 2000 vb', sorted(byvb.items()))
print('groups of the re-cooked meshes (top)', ngs.most_common(6))
