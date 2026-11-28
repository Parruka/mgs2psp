#!/usr/bin/env python3
# piccmp38.py A_DIR B_DIR (gl38): the emulator's pictures of two runs (shot_*.bmp, live_*.bmp), image data only (the BMP header's
# pixel offset on), by the kind of the turn the picture's vblank falls in (tools/sched38.py).  A K0 turn's pictures must equal the
# reference's (gl36's emulator pictures); the other kinds' pictures differ by design and are only counted.
import os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
SCH = eval(subprocess.run([sys.executable, "-I", os.path.join(here, os.environ.get("SCHED38", "sched38.py")), "py"], capture_output=True, text=True, check=True).stdout)
BOUND = {r[0]: r[2] for r in SCH}   # (turn, vb, kind, re-use, F1, scene)
def kind(vb):
    if vb < 24000: return 0
    t = (vb - 24000) // 100
    return BOUND.get(t, 0)
def pix(p):
    d = open(p, "rb").read()
    if d[:2] != b"BM": return None
    off, = struct.unpack_from("<I", d, 10); w, h, _, bpp = struct.unpack_from("<iiHH", d, 18)
    return w, h, bpp, d[off:]
a, b = sys.argv[1], sys.argv[2]
fa = sorted(f for f in os.listdir(a) if f.endswith(".bmp")); fb = set(f for f in os.listdir(b) if f.endswith(".bmp"))
tot = {}; bad = []
print("pictures of %s against %s: %d / %d files; only in the first: %s; only in the second: %s" % (a, b, len(fa), len(fb), sorted(set(fa) - fb), sorted(fb - set(fa))))
for f in fa:
    if f not in fb: continue
    vb = int(f.split("_")[1].split(".")[0]); k = kind(vb); kb = kind(vb - 4)
    x, y = pix(os.path.join(a, f)), pix(os.path.join(b, f))
    if x is None or y is None or x[:3] != y[:3]: res = "NOT COMPARABLE"; same = False
    elif x[3] == y[3]: res = "equal"; same = True
    else:
        bpp = x[2] // 8; n = 0; mx = 0; da, db = x[3], y[3]
        for i in range(0, min(len(da), len(db)), bpp):
            if da[i:i + bpp] != db[i:i + bpp]:
                n += 1; mx = max(mx, max(abs(da[i + j] - db[i + j]) for j in range(bpp)))
        res = "%d px differ (of %d), largest step %d a channel" % (n, x[0] * abs(x[1]), mx); same = False
    key = "K%d" % k + ("" if kb == k else " (turn edge: the picture may be the K%d turn's)" % kb)
    t = tot.setdefault(key, [0, 0]); t[0] += 1; t[1] += same
    if not same: bad.append((f, key, res))
    print("  %-16s vb %5d turn %s kind %s: %s" % (f, vb, (vb - 24000) // 100 if vb >= 24000 else "-", key, res))
print("by kind of the picture's turn: " + "; ".join("%s: %d of %d equal" % (k, v[1], v[0]) for k, v in sorted(tot.items())))
k0 = [x for x in bad if x[1] == "K0"]
print("K0 turns' pictures differing: %d%s" % (len(k0), "" if not k0 else " (" + ", ".join(x[0] for x in k0) + ")"))
