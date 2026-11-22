import sys, re, importlib.util
spec = importlib.util.spec_from_file_location('rd30', '/home/claude/main/build_gl30/rd30.py'); rd = importlib.util.module_from_spec(spec); spec.loader.exec_module(rd)
rdout, live, ref = sys.argv[1], sys.argv[2], sys.argv[3]
T = int(sys.argv[4]) if len(sys.argv) > 4 else 6
pairs = re.findall(r'live (\d+) \(periodic[^)]*\): buffer \d frame vb (\d+)', open(rdout).read())
def lum(r, p): return (r[p]*299 + r[p+1]*587 + r[p+2]*114)//1000
for lv, fv in pairs:
    try:
        a = rd.load_bmp('%s/live_%s.bmp' % (live, lv)); b = rd.load_bmp('%s/shot_%s.bmp' % (ref, fv))
    except Exception as e:
        print(lv, fv, 'no ref', e); continue
    w, h, ra = a; _, _, rb = b
    m = set(); dsum = 0
    for y in range(h):
        x1, x2 = ra[y], rb[y]
        if x1 == x2: continue
        for x in range(w):
            p = 3*x; d = lum(x1, p) - lum(x2, p)
            if d >= T and not rd.px_hud(x, y): m.add((x, y)); dsum += d
    seen = set(); comps = []
    for s in m:
        if s in seen: continue
        st = [s]; seen.add(s); n = 0; xs = []; ys = []
        while st:
            x, y = st.pop(); n += 1; xs.append(x); ys.append(y)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (x+dx, y+dy)
                    if q in m and q not in seen: seen.add(q); st.append(q)
        comps.append((n, min(xs), min(ys), max(xs), max(ys)))
    comps.sort(reverse=True)
    big = [c for c in comps if c[0] >= 100]
    print('live %s frame %s: lighter %d px (mean %.1f), clusters %d, >=100 px: %d, largest %s' % (lv, fv, len(m), dsum/max(1,len(m)), len(comps), len(big), comps[:3]))
