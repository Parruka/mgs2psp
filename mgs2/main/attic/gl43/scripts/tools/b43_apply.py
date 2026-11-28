"""b43_apply.py <gen dir> <gen43 dir> (gl43; attic/gl43): writes gen43/{wg,wpl}/gen_s_w00a.h from gen/{wg,wpl}/gen_s_w00a.h (md5-checked:
wg eac6413f, wpl 923514f4) with option B's hooks (boot/native_b43.h, boot/native_b43.c):
  - '#include "native_b43.h"' first;
  - the four shadowed functions (floor spawn 0x317e88, floor update 0x318418, fall update 0x319600, fall dead 0x319078) renamed <name>_t,
    each with a wrapper of its old name before it (B43_WRAP: the shadow or the translation);
  - the floor and fall acts (0x318798, 0x319938): in GEN_DG_TRACE builds only, renamed _t behind a wrapper that leaves their loads out of
    the read hash (B43_ACT_WRAP); other builds keep them as they are;
  - B43_HWR at the acts' show stores of their prims' +164: floor 0x318808, fall 0x319aec, ripple 0x3321f4 0x332214 0x332494 0x3324a4
    0x3324b4; B43_RD around ripple's loads of +164 (0x3321bc 0x3321ec 0x332208 0x332448 0x332458 0x332468 0x332484 0x33249c 0x3324ac);
  - '#include "native_b43.c"' last.
python3 -I; prints the md5s of what it wrote."""
import sys, hashlib, re, os
G, O = sys.argv[1], sys.argv[2]
WANT = {'wg': 'eac6413f', 'wpl': '923514f4'}
SHADOW = {'317e88': 'B43_FLSP', '318418': 'B43_FLUP', '319600': 'B43_FAUP', '319078': 'B43_FADE'}
ACTS = ('318798', '319938')
HWR = {'318808': 'r3', '319aec': 'r4', '3321f4': 'r3', '332214': 'r4', '332494': 'r4', '3324a4': 'r3', '3324b4': 'r4'}
RD = ('3321bc', '3321ec', '332208', '332448', '332458', '332468', '332484', '33249c', '3324ac')
for v in ('wg', 'wpl'):
    src = os.path.join(G, v, 'gen_s_w00a.h'); b = open(src, 'rb').read()
    m = hashlib.md5(b).hexdigest()[:8]
    if m != WANT[v]: sys.exit('b43_apply: %s is %s, not %s' % (src, m, WANT[v]))
    L = b.decode('latin-1').split('\n')
    n_hwr = n_rd = 0
    for i, l in enumerate(L):
        mm = re.match(r'^/\*([0-9a-f]{6})\*/ ', l)
        if not mm: continue
        a = mm.group(1)
        if a in HWR:
            r = HWR[a]; old = 'WR32B(%s, 164, r2);' % r
            if l.count(old) != 1: sys.exit('b43_apply: %s: %s not once at %s' % (v, old, a))
            L[i] = l.replace(old, 'B43_HWR(%s, r2);' % r); n_hwr += 1
        if a in RD:
            body = l[len(mm.group(0)):]
            if not re.match(r'^r2 = \(int32_t\)RD32B\(r[34], 164\);$', body): sys.exit('b43_apply: %s: load at %s is %r' % (v, a, body))
            L[i] = mm.group(0) + 'B43_RD(' + body[:-1] + ');'; n_rd += 1
    if n_hwr != len(HWR) or n_rd != len(RD): sys.exit('b43_apply: %s: %d stores, %d loads' % (v, n_hwr, n_rd))
    out = []
    for l in L:
        mm = re.match(r'^static void S_w00a_P([0-9a-f]{6})\(void\)$', l)
        if mm and mm.group(1) in SHADOW:
            f = 'S_w00a_P' + mm.group(1)
            out += ['static void %s_t(void);' % f,
                    'static void %s(void) { B43_WRAP(%s, %s_t); }   /* gl43 B (boot/native_b43.h) */' % (f, SHADOW[mm.group(1)], f),
                    'static void %s_t(void)' % f]
            continue
        if mm and mm.group(1) in ACTS:
            f = 'S_w00a_P' + mm.group(1)
            out += ['#ifdef GEN_DG_TRACE   /* gl43 B: the act\'s loads out of the read hash in both modes (boot/native_b43.h) */',
                    'static void %s_t(void);' % f,
                    'static void %s(void) { B43_ACT_WRAP(%s_t); }' % (f, f),
                    '#define B43_A_%s %s_t' % (mm.group(1), f), '#else', '#define B43_A_%s %s' % (mm.group(1), f), '#endif',
                    'static void B43_A_%s(void)' % mm.group(1)]
            continue
        out.append(l)
    if sum(1 for l in out if l.startswith('static void S_w00a_P') and l.endswith('_t(void)')) != len(SHADOW) or \
       sum(1 for l in out if l.startswith('static void B43_A_')) != len(ACTS): sys.exit('b43_apply: %s: definitions not found' % v)
    while out and out[-1] == '': out.pop()
    out = ['#include "native_b43.h"   /* gl43 B: tools/b43_apply.py */'] + out + ['#include "native_b43.c"   /* gl43 B: tools/b43_apply.py, after native_w00a.c */', '']
    dst = os.path.join(O, v, 'gen_s_w00a.h'); os.makedirs(os.path.dirname(dst), exist_ok=True)
    ob = '\n'.join(out).encode('latin-1'); open(dst, 'wb').write(ob)
    print('%s: %s -> %s %s (stores %d, loads %d, wrappers %d, acts %d)' % (v, m, dst, hashlib.md5(ob).hexdigest()[:8], n_hwr, n_rd, len(SHADOW), len(ACTS)))
