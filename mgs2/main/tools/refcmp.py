#!/usr/bin/env python3
"""refcmp.py <reference hashref> <log> [<out hashref>]: the hash lines of an x86 twin log or a PSP / emulator log.txt against a hashref
(lines "<vb> <ee> <vfb> [<vuh>]", prof/hashref_x86_*.txt).  Log lines: "hash: vb N ee X iop Y [gs Z gsd D] vfb V" (x86: on stderr; the
PSP harness writes the same fields) and "vuh: vb N H" (MGS2_HASH_DENSE).  Prints how many reference lines the log has, how many differ in
ee, vfb or vuh (with the first ten), and the reference lines the log lacks.  With <out hashref> it writes the log's own hashref in the
reference's layout: one line per reference vblank the log has, "<vb> <ee> <vfb>" plus " <vuh>" where the reference line has a fourth field.
Exit status 0 when every reference line the log has is equal and none is missing (--partial: missing lines allowed).  Run it with python3 -I (mainline tree, 2026-11-18)."""
import sys, re
part = '--partial' in sys.argv   # a log that hashes only some of the reference's vblanks (the PSP harness: MGS2_HASH_EVERY=500,31800:100)
sys.argv = [a for a in sys.argv if a != '--partial']
ref = {}; order = []
for ln in open(sys.argv[1]):
    p = ln.split()
    if len(p) >= 2:
        ref[int(p[0])] = p[1:]; order.append(int(p[0]))
H = {}; V = {}; L = []
for ln in open(sys.argv[2], errors='replace'):   # a hash line cut by another message (x86 at vb 100: "gs mask: ..." from gs_load) goes on
    if ln.startswith(' gs ') and L and L[-1].startswith('hash: vb') and ' vfb ' not in L[-1]:   # on the next line (as cmp_ref.py joins it)
        L[-1] = re.sub(r'gs mask:.*$', '', L[-1].rstrip('\n')) + ln
    else: L.append(ln)
for ln in L:
    m = re.search(r'hash: vb (\d+) ee (\w+) iop (\w+).*? vfb (\d+)', ln)
    if m: H[int(m.group(1))] = (m.group(2), m.group(4)); continue
    m = re.search(r'vuh: vb (\d+) (\w+)', ln)
    if m: V[int(m.group(1))] = m.group(2)
n = 0; bad = []; miss = []; nv = 0
for vb in order:
    r = ref[vb]
    if vb not in H: miss.append(vb); continue
    n += 1; ee, vfb = H[vb]; why = []
    if ee != r[0]: why.append('ee %s (ref %s)' % (ee, r[0]))
    if len(r) > 1 and vfb != r[1]: why.append('vfb %s (ref %s)' % (vfb, r[1]))
    if len(r) > 2:
        if vb in V:
            nv += 1
            if V[vb] != r[2]: why.append('vuh %s (ref %s)' % (V[vb], r[2]))
        else: why.append('no vuh line')
    if why: bad.append((vb, why))
print('%s: %d hash lines, %d vuh lines; reference %s: %d lines' % (sys.argv[2], len(H), len(V), sys.argv[1], len(order)))
print('compared %d reference lines (%d with vuh): %d differ; %d reference lines not in the log%s' %
      (n, nv, len(bad), len(miss), (' (first %d, last %d)' % (miss[0], miss[-1])) if miss else ''))
for vb, why in bad[:10]: print('  vb %d: %s' % (vb, '; '.join(why)))
if len(sys.argv) > 3:
    with open(sys.argv[3], 'w') as f:
        for vb in order:
            if vb in H:
                f.write('%d %s %s%s\n' % (vb, H[vb][0], H[vb][1], (' ' + V.get(vb, '-')) if len(ref[vb]) > 2 else ''))
    print('wrote', sys.argv[3])
sys.exit(0 if not bad and (part or not miss) and n else 1)
