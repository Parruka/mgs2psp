# livecmp.py runA runB: the live shots (gl28 live lines: 'live vb V ... frame vb F ... saved as .../live_V.bmp') of two emulator runs,
# paired by the frame they show (F), compared by pixel data; the fixed shots (shot_V.bmp) by name
import sys, os, re, struct
def frames(run):
    d = {}
    for l in open(os.path.join(run, 'log.txt'), errors='replace'):
        m = re.match(r'picture: gl28 live vb (\d+) .*? frame vb (\d+) .*?saved as \S*/(live_\d+\.bmp)', l)
        if m: d.setdefault(int(m.group(2)), m.group(3))
    return d
def px(p):
    b = open(p, 'rb').read(); return b[struct.unpack_from('<I', b, 10)[0]:]
a, b = sys.argv[1], sys.argv[2]
fa, fb = frames(a), frames(b)
both = sorted(set(fa) & set(fb)); eq = 0
for f in both:
    x, y = px(os.path.join(a, 'pic', fa[f])), px(os.path.join(b, 'pic', fb[f]))
    n = sum(1 for i in range(0, min(len(x), len(y)), 4) if x[i:i + 3] != y[i:i + 3])
    if n == 0: eq += 1
    else: print('  frame vb %d (%s / %s): %d pixels differ' % (f, fa[f], fb[f], n))
print('live shots: A %d, B %d; showing the same frame: %d; of those pixel data equal: %d' % (len(fa), len(fb), len(both), eq))
