# chars.py: the w00a stage image's character table (hash -> new function) and, for each rain actor's name string, the function that
# names it (GV_SetNamedActor's 4th argument), the enclosing function's start, and the table entries that reach it (directly or by one call)
import struct, re, sys
B = 0x1f1c00
d = open('/home/claude/mgs2/stage/w00a/nocache/w00a.bin', 'rb').read()
def w(a): return struct.unpack_from('<I', d, a - B)[0]
tab = {}; o = 0x10
while True:
    h, a = struct.unpack_from('<2I', d, o)
    if h == 0 and a == 0: break
    tab[a] = tab.get(a, []) + [h]; o += 8
end_text = w(B + 4)
# function starts: after each jr ra + delay slot
starts = set()
for a in range(B + 0x10 + 8 * (len(sum(tab.values(), [])) + 1), end_text, 4):
    if w(a) == 0x03e00008: starts.add(a + 8)
st = sorted(starts)
import bisect
def fstart(a):
    i = bisect.bisect_right(st, a) - 1
    return st[i] if i >= 0 else None
# jal targets -> callers
calls = {}
for a in range(B, end_text, 4):
    x = w(a)
    if x >> 26 == 3:
        t = ((a + 4) & 0xf0000000) | ((x & 0x3ffffff) << 2); calls.setdefault(t, []).append(a)
names = {}
for s in [b'rain.c', b'line_sph.c', b'body_sph.c', b'charaspread.c', b'd_splash_fall.c', b'd_splash_camera.c', b'd_splash_floor.c',
          b'rain_gas_pers.c', b'wave2.c', b'ripple.c', b'chim_smo.c', b'fade_lgt.c', b'bonbori_red.c', b'bonbori_yw.c', b'c_light.c', b'wind_man.c', b'windnois.c', b'sky_prev.c', b'item_box.c']:
    i = 0
    while True:
        i = d.find(s + b'\0', i)
        if i < 0: break
        names[B + i] = s.decode()
        i += 1
for sa, n in sorted(names.items(), key=lambda x: x[1]):
    hi = (sa + 0x8000) >> 16; lo = sa - (hi << 16)
    sites = []
    for a in range(B, end_text, 4):
        x = w(a)
        if x >> 26 == 9 and (x & 0xffff) == (lo & 0xffff):   # addiu with the low half
            rs = (x >> 21) & 31
            for b in range(a - 4, a - 64, -4):                 # a lui of the high half into rs nearby
                y = w(b)
                if y >> 26 == 15 and (y >> 16) & 31 == rs and (y & 0xffff) == hi: sites.append(a); break
    out = []
    for s_ in sites:
        f = fstart(s_); ent = tab.get(f)
        if ent: out.append('%06x in %06x = table %s' % (s_, f, ','.join('%x' % h for h in ent)))
        else:
            cs = calls.get(f, []); via = []
            for c in cs:
                g = fstart(c)
                if g in tab: via.append('%06x (table %s)' % (g, ','.join('%x' % h for h in tab[g])))
                else: via.append('%06x' % g)
            out.append('%06x in %06x, called from %s' % (s_, f, ' '.join(via) or 'nothing by jal'))
    print('%-18s %06x: %s' % (n, sa, '; '.join(out) or 'no addiu site'))
