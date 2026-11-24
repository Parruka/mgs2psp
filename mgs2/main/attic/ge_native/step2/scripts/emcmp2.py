#!/usr/bin/env python3
"""emcmp2.py A_log B_log [from]: the picture digests of two emulator logs (the PSP harness's "picture: vb N buffer b list L frame F" and
"picture: vb N content H" lines) compared vblank by vblank from vb <from> (default 24,000), split by who made the picture in B's run: the
ME's loop on its own thread ("me turn vb X-Y": the loop apart from the main CPU) or the main CPU itself ("main turn vb X-Y").  Prints
equal / differing counts per kind and owner, and the first differing vblanks."""
import re, sys
def rd(p):
    d = {'list': {}, 'frame': {}, 'content': {}}; turns = []
    for l in open(p, errors='replace'):
        m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+)', l)
        if m: v = int(m.group(1)); d['list'][v] = m.group(2); d['frame'][v] = m.group(3); continue
        m = re.match(r'picture: vb (\d+) content ([0-9a-f]+)', l)
        if m: d['content'][int(m.group(1))] = m.group(2); continue
        m = re.match(r"(me|main) turn vb (\d+)-(\d+) ", l)
        if m and (int(m.group(2)), int(m.group(3)), m.group(1)) not in turns: turns.append((int(m.group(2)), int(m.group(3)), m.group(1)))
    return d, turns
a, _ = rd(sys.argv[1]); b, tb = rd(sys.argv[2]); lo = int(sys.argv[3]) if len(sys.argv) > 3 else 24000
def owner(v):
    for x, y, w in tb:
        if x <= v < y: return w
    return '?'
for k in ('list', 'frame', 'content'):
    res = {}
    for v in sorted(set(a[k]) & set(b[k])):
        if v < lo: continue
        o = owner(v); e = a[k][v] == b[k][v]
        r = res.setdefault(o, [0, 0, []]); r[0 if e else 1] += 1
        if not e and len(r[2]) < 6: r[2].append(v)
    print('%-8s' % k, '; '.join('%s: equal %d differ %d%s' % (o, r[0], r[1], (' first ' + ','.join(map(str, r[2]))) if r[2] else '') for o, r in sorted(res.items())),
          '| only A %d only B %d' % (len([v for v in a[k] if v >= lo and v not in b[k]]), len([v for v in b[k] if v >= lo and v not in a[k]])))
