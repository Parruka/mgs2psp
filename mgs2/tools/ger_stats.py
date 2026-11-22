# per-entry GE cost factors of a GE replay file (ge/replay_fmt.h)
import struct, sys, collections
f = open(sys.argv[1], 'rb').read()
(magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h) = struct.unpack_from('<12I', f, 0)
BS = struct.calcsize('<HBB4B6B2B4h4I'); VS = 24; TS = struct.calcsize('<4B4H4I')
B = [struct.unpack_from('<HBB4B6B2B4h4I', f, ob + i * BS) for i in range(nb)]
V = [struct.unpack_from('<2fI3f', f, ov + i * VS) for i in range(nv)]
T = [struct.unpack_from('<4B4H4I', f, ot + i * TS) for i in range(nt)]
def tris(prim, vs):
    if prim == 3: return [vs[i:i+3] for i in range(0, len(vs) - 2, 3)]
    if prim == 4: return [vs[i:i+3] for i in range(len(vs) - 2)]
    if prim == 5: return [(vs[0], vs[i], vs[i+1]) for i in range(1, len(vs) - 1)]
    return []
def tarea(a, b, c, iu, iv):
    return abs((b[iu]-a[iu])*(c[iv]-a[iv]) - (c[iu]-a[iu])*(b[iv]-a[iv])) / 2
E = collections.defaultdict(lambda: collections.Counter())
TX = collections.defaultdict(collections.Counter)
last_tex = None
for b in B:
    tex, prim, flags = b[0], b[1], b[2]
    entry, first, count, px = b[-4:]
    vs = V[first:first+count]
    sa = ta = 0.0
    if prim == 6:
        for i in range(0, len(vs) - 1, 2):
            p, q = vs[i], vs[i+1]
            sa += abs(q[3]-p[3]) * abs(q[4]-p[4]); ta += abs(q[0]-p[0]) * abs(q[1]-p[1])
    else:
        for t in tris(prim, vs):
            sa += tarea(*t, 3, 4); ta += tarea(*t, 0, 1)
    e = E[entry]
    e['batches'] += 1; e['ps2px'] += px; e['area'] += sa
    if flags & 2:
        k = T[tex]; kind = k[0]; key = 'fb' if kind == 2 else 'T%d' % k[1]
        e['area_' + key] += sa; e['texarea_' + key] += ta
        if tex != last_tex: e['texchg'] += 1; e['clut_bytes'] += 4 * k[11] if kind == 1 else 0
        last_tex = tex
        TX[entry][tex] += sa
        if b[11] == 1: e['decal'] += 1
    else:
        e['area_untex'] += sa
    if flags & 4: e['area_blend'] += sa
    if flags & 64: e['fog'] += 1
    if b[14]: e['twopass_area'] += sa
tot = collections.Counter()
for en, e in sorted(E.items(), key=lambda kv: -kv[1]['area']):
    tot.update(e)
    if e['area'] < 2000: continue
    print('entry %06x: %d batches, PS2 px %d, GE area %.0f (%.2f screens)' % (en, e['batches'], e['ps2px'], e['area'], e['area'] / (480 * 272)))
    for k in ('fb', 'T4', 'T8'):
        if e['area_' + k]: print('   %-3s area %8.0f  texel/pixel %.2f' % (k, e['area_' + k], e['texarea_' + k] / e['area_' + k]))
    print('   untextured %.0f, blended %.0f, two-pass %.0f, texture changes %d, CLUT bytes %d, distinct textures %d' % (e['area_untex'], e['area_blend'], e['twopass_area'], e['texchg'], e['clut_bytes'], len(TX[en])))
    big = sorted(TX[en].items(), key=lambda kv: -kv[1])[:6]
    print('   top textures by area: ' + '; '.join('#%d %s %dx%d %.0f px' % (t, ('fb%d' % T[t][1]) if T[t][0] == 2 else 'T%d' % T[t][1], T[t][4], T[t][5], a) for t, a in big))
print('total GE area %.0f (%.2f screens), texture changes %d, CLUT bytes %d' % (tot['area'], tot['area'] / (480 * 272), tot['texchg'], tot['clut_bytes']))
