#!/usr/bin/env python3
"""mvse34.py LOG (gl34; run with python3 -I): the 'nat split' lines of the rain wait's ME turns in an emulator (or PSP) log, by part,
next to the model's parts (model_split.out's figures, typed in below from split34.py's output for the rain wait's ME turn).
The emulator's times are the host's: only the shares mean anything, and each stage holds one clock read (the line's mean read is
taken off below: 'net')."""
import sys, re
ST = ['walk and pointers', 'register', 'A+D packet', "call's rest", 'host call and GS state', 'transform', 'positions', 'cut test', 'attributes',
      'texture', 'strips', 'GE transform and batch data', 'depth range', 'into the frame', 'way back', 'drawn areas', "walk's end"]
PARTS = [('walk (merge, record, pointers)', [0, 16], 9.5, 8.6),
         ('front end (register, A+D packet, call, way back; with the CLUT tracking)', [1, 2, 3, 14], 25.0 + 7.5, 13.7 + 5.4 + 8.2),
         ('converter per group (host call, GS state, texture, batch, into the frame)', [4, 9, 11, 13], 17.1, 16.8),
         ('transform', [5], 3.7, 0.9), ('cut test', [7], 3.4, 3.5), ('depth range', [12], 2.2, 2.1),
         ('per vertex (positions, attributes, strips)', [6, 8, 10], 6.7, 7.8), ('drawn areas (in the model: inside the front end line)', [15], 0.0, 0.0)]
T = {0: [], 1: []}
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'nat split vb (\d+)-\d+ me turn arm \d fast (\d)', l)
    if not m or not (29500 <= int(m[1]) < 30500): continue
    rd = float(re.search(r'mean ([\d.]+) us a read', l)[1]); v = [float(re.search(re.escape(s) + r' ([\d.]+)', l)[1]) for s in ST]
    nat = float(re.search(r'= ([\d.]+) us a group', l)[1]); T[int(m[2])].append((int(m[1]), rd, v, nat))
for f in (0, 1):
    print('fast %d: ME turns %s; clock read %s us; native us a group %s' % (f, [t[0] for t in T[f]], ['%.3f' % t[1] for t in T[f]], ['%.2f' % t[3] for t in T[f]]))
def mean(f, idx, net):
    r = []
    for _, rd, v, _ in T[f]: r.append(sum(v[i] - (rd if net else 0) for i in idx))
    return sum(r) / len(r) if r else 0.0
tot = {(f, n): sum(mean(f, p[1], n) for p in PARTS) for f in (0, 1) for n in (0, 1)}
mt = (sum(p[2] for p in PARTS), sum(p[3] for p in PARTS))
print('%-78s | %-33s | %-33s | %s' % ('part', 'log, plain: us (net of reads) share', 'log, fast: us (net of reads) share', 'model us a group plain / fast (share)'))
for name, idx, mp, mf in PARTS:
    a, an, b, bn = mean(0, idx, 0), mean(0, idx, 1), mean(1, idx, 0), mean(1, idx, 1)
    print('%-78s | %6.2f (%6.2f) %5.1f%%          | %6.2f (%6.2f) %5.1f%%          | %5.1f (%4.1f%%) / %5.1f (%4.1f%%)' % (name, a, an, 100 * an / tot[0, 1], b, bn, 100 * bn / tot[1, 1], mp, 100 * mp / mt[0], mf, 100 * mf / mt[1]))
print('%-78s | %6.2f (%6.2f)                 | %6.2f (%6.2f)                 | %5.1f / %5.1f' % ('sum of the timed stages', tot[0, 0], tot[0, 1], tot[1, 0], tot[1, 1], mt[0], mt[1]))
