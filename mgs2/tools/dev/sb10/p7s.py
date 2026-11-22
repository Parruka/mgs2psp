# p7s.py <log> [a b]: task #133 sizing: the P07 input repeats (xconv.c P07_STAT) over the frames whose vb is in a..b (each p7s line is printed
# at a frame's gl_end, before that frame's "xlive gb: vb N:" line), and over the whole run.  Both line forms (x2's first, x3's by group)
import sys, re
L = sys.argv[1]; a = int(sys.argv[2]) if len(sys.argv) > 2 else 0; b = int(sys.argv[3]) if len(sys.argv) > 3 else 1 << 30
NUM = re.compile(r'-?\d+')
rows = []; pend = None
for l in open(L, errors='replace'):
    if l.startswith('p7s: '): pend = [int(x) for x in NUM.findall(l[5:].replace('T0-3', 'T').replace('abs2-11', 'a').replace('abs16-19', 'b').replace('abs24-31', 'c').replace('Tfe-ff', 'd').replace('last4', 'last'))]
    elif l.startswith('xlive gb: vb ') and pend is not None:
        vb = int(l.split()[3].rstrip(':')); rows.append((vb, pend)); pend = None
def show(t, lo, hi):
    prev = None; first = last = None; lastvb = None
    for vb, v in rows:
        if vb < lo: prev = v
        elif vb <= hi: last = v; first = first or vb; lastvb = vb
    if last is None: print(t, 'no frames'); return
    d = [x - y for x, y in zip(last, prev or [0] * len(last))]
    print('%s (frames of vb %d..%d):' % (t, first, lastvb))
    if len(d) == 12:   # x2: calls verts new last_c last_b v_b last_cb v_last_cb 4 any_cb v_any full
        c, v = d[0], d[1]
        print('  calls %d, vertices %d (%.1f a call); parts new %d' % (c, v, v / max(c, 1), d[2]))
        if c: print('  as the part\'s last call: constants %.2f%%, bones %.2f%%, both %.2f%%; both in its last 4: %.2f%%' % (100 * d[3] / c, 100 * d[4] / c, 100 * d[6] / c, 100 * d[9] / c))
    else:   # x3: calls verts new T a b c d bones all v_all any v_any full nb1..4
        c, v = d[0], d[1]
        print('  calls %d, vertices %d (%.1f a call); parts new %d; bones a vertex 1..4: %s' % (c, v, v / max(c, 1), d[2], d[14:18]))
        if c:
            print('  the same as the part\'s last call: T+0..3 %.2f%%, abs 2..11 %.2f%%, abs 16..19 %.2f%%, abs 24..31 %.2f%%, T+0xfe..ff %.2f%%, '
                  'the bones named %.2f%%, everything %.2f%% (%.2f%% of vertices)' % tuple([100 * d[k] / c for k in range(3, 10)] + [100 * d[10] / max(v, 1)]))
            print('  everything as one of the part\'s last 4 calls: %.2f%% (%.2f%% of vertices); table full %d' % (100 * d[11] / c, 100 * d[12] / max(v, 1), d[13]))
show('window', a, b); show('whole run', 0, 1 << 30)
