#!/usr/bin/env python3
"""rd30.py LOG [--bat battery.txt] [--live DIR] [--ref DIR] [--twin LOG] [--hashref FILE] [--from VB] [--to VB]

gl30 (2026-11-20, gl30_plan.md G6): rd29.py reading gl30's new counter: the digest lines' dz text 'sub:n' (X's decals paired with a
subset base, XS) and the window lines' 'subset n' (both optional, so gl29 / gl29x logs still read); per arm the subset pairs next to the
pairs.  Defaults: --ref build_gl30/emu_g3/pic (G3's emulator frames: shot_<frame vb>.bmp), --twin build_gl30/x86/out_g1.log (the twin
with MGS2_GL_DZ=29490:31400:X).  The rest as rd29:

gl29 (2026-11-20, gl29_plan.md G4): rd28.py plus the shadow-depth test's lines: the gl29 switch lines; the frame digests with their
MGS2_GL_DZ arm and counts against the twin's (digest and dz text), and per arm; the 'gl29 dz window' lines; R held by arm and turn and
by window (the log's end table); the shot lines with their arm, 'last list = shot frame', depth files and VRAM read times; the arm on
each eye and live line; per live capture also the pixels where the PSP is lighter than the emulator's same frame (same arm: E-A) by 6
or more in luma (an4.py's rule), outside the HUD boxes.  Defaults: --ref build_gl29/live_ref (E-A's frames: shot_<frame vb>.bmp),
--twin build_gl29/x86/out_e2b.log (the twin with gl29's final MGS2_GL_DZ: B1 in the walk), window 29400..31500.

Reader for gl28's log (2026-11-20 update: MGS2_EYE, MGS2_GL_LIVE, MGS2_GL_FBCHK, MGS2_ME_PIC_GE2_AB, MGS2_GL_SHOT_FLIPWAIT, plus gl28's
MGS2_ME_REST and MGS2_BAT_REC; gl28d2: warns when the display's answers name neither buffer, notes turn-boundary hooks).  Run it with python3 -I (stdlib only).  It prints:
  - the gl28 switch lines, the hashes against the hashref (main/tools/refcmp.py --partial) and the frame digests against the x86 twin
    (main/tools/emucmp.py), split by turn type;
  - the ME rest lines (one per main turn) summed, and the totals line;
  - GE2 per ME turn (the 'gl28 ge2 a/b' lines and the 'GE2 n' field of each 'me turn' line);
  - the display counter (MGS2_GL_FBCHK) per turn from the 'gl27 display' lines, the end line and the detail lines;
  - the eye lines (R down/up) with turn type, GE2 of that turn, how long R was held;
  - the shots that waited for the flip;
  - per live capture: its line, the black clusters outside the HUD boxes (blk.py's rule: a pixel is black when max(R,G,B) <= 6;
    4-connected clusters), and, when the emulator reference of the same frame (the frame's vblank) exists in the reference folder
    (live_ref.txt there maps each reference live_<vb>.bmp to its frame; shot_<frame vb>.bmp are frames too), the pixel differences;
  - with --bat: the battery lines inside the window.
rd28's defaults were: --live = the log's folder, --ref = /home/claude/main/build_gl28/live_ref, --twin = build_gl19/x86/out_g22b.log,
--hashref = /home/claude/main/cfg/hashref_g22b.txt, window 30400..31500.  Game pictures stay in the container."""
import os, re, struct, subprocess, sys, hashlib

M = '/home/claude/main'
HUD = [(351, 16, 466, 80), (379, 215, 466, 256)]   # x0, y0, x1, y1 (top-down rows): the radar box and the lower-right item box [C rd of
                                                   # the reader's emulator shots 30930-31250: black only there]
HUD_M = 3                                          # px of margin around the boxes
BLACK = 6                                          # blk.py: max(R, G, B) <= 6
MIN_CL = 12                                        # clusters smaller than this are listed only in the totals


def args(a):
    o = {'log': None, 'bat': None, 'live': None, 'ref': M + '/build_gl30/emu_g3/pic', 'twin': M + '/build_gl30/x86/out_g1.log',
         'hashref': M + '/cfg/hashref_g22b.txt', 'from': 29400, 'to': 31500}
    i = 0
    while i < len(a):
        k = a[i]
        if k.startswith('--') and i + 1 < len(a):
            o[k[2:]] = a[i + 1]; i += 2
        else:
            o['log'] = k; i += 1
    if not o['log']:
        print(__doc__); sys.exit(2)
    o['from'] = int(o['from']); o['to'] = int(o['to'])
    if o['live'] is None:
        o['live'] = os.path.dirname(os.path.abspath(o['log']))
    return o


def md5(p):
    return hashlib.md5(open(p, 'rb').read()).hexdigest()[:8]


def load_bmp(p):
    """24-bit BMP -> (w, h, rows top-down of bytes R,G,B...)"""
    b = open(p, 'rb').read()
    off = struct.unpack_from('<I', b, 10)[0]; w, h = struct.unpack_from('<ii', b, 18); bpp = struct.unpack_from('<H', b, 28)[0]
    if bpp != 24:
        raise ValueError('not 24-bit')
    rs = (w * 3 + 3) & ~3; H = abs(h); rows = []
    for y in range(H):
        yy = H - 1 - y if h > 0 else y
        row = b[off + rs * yy: off + rs * yy + w * 3]
        rgb = bytearray(w * 3); rgb[0::3] = row[2::3]; rgb[1::3] = row[1::3]; rgb[2::3] = row[0::3]
        rows.append(bytes(rgb))
    return w, H, rows


def in_hud(x0, y0, x1, y1):
    for a, b, c, d in HUD:
        if x0 >= a - HUD_M and y0 >= b - HUD_M and x1 <= c + HUD_M and y1 <= d + HUD_M:
            return True
    return False


def px_hud(x, y):
    for a, b, c, d in HUD:
        if a - HUD_M <= x <= c + HUD_M and b - HUD_M <= y <= d + HUD_M:
            return True
    return False


def black_clusters(img):
    """blk.py's logic: black pixels, 4-connected clusters; those not inside a HUD box"""
    w, h, rows = img
    blk = [bytearray(1 if max(r[3 * x], r[3 * x + 1], r[3 * x + 2]) <= BLACK else 0 for x in range(w)) for r in rows]
    seen = [bytearray(w) for _ in range(h)]; comps = []
    for y in range(h):
        by = blk[y]; sy = seen[y]
        for x in range(w):
            if by[x] and not sy[x]:
                st = [(x, y)]; sy[x] = 1; c = 0; x0 = x1 = x; y0 = y1 = y
                while st:
                    a, q = st.pop(); c += 1
                    if a < x0: x0 = a
                    if a > x1: x1 = a
                    if q < y0: y0 = q
                    if q > y1: y1 = q
                    for u, v in ((a + 1, q), (a - 1, q), (a, q + 1), (a, q - 1)):
                        if 0 <= u < w and 0 <= v < h and blk[v][u] and not seen[v][u]:
                            seen[v][u] = 1; st.append((u, v))
                comps.append((c, x0, y0, x1, y1))
    comps.sort(reverse=True)
    out = [k for k in comps if not in_hud(*k[1:])]
    nb = sum(map(sum, blk)); nout = sum(k[0] for k in out)
    mean = sum(sum(r) for r in rows) / float(w * h * 3)
    return nb, nout, out, mean


def diff(a, b):
    """pixel differences: any channel; more than 32 in a channel; black here but not (max > 30) in the reference, outside the HUD"""
    w, h, ra = a; _, _, rb = b; n_any = n_big = n_newblk = 0
    for y in range(h):
        x1 = ra[y]; x2 = rb[y]
        if x1 == x2:
            continue
        for x in range(w):
            p = 3 * x
            r1, g1, b1 = x1[p], x1[p + 1], x1[p + 2]; r2, g2, b2 = x2[p], x2[p + 1], x2[p + 2]
            if r1 != r2 or g1 != g2 or b1 != b2:
                n_any += 1
                if max(abs(r1 - r2), abs(g1 - g2), abs(b1 - b2)) > 32:
                    n_big += 1
                if max(r1, g1, b1) <= BLACK and max(r2, g2, b2) > 30 and not px_hud(x, y):
                    n_newblk += 1
    return n_any, n_big, n_newblk


def turn_of(vb, turn=100):
    """ME turns: vb // TURN even (cfg MGS2_ME_PIC_TURN=100: 0-99 the ME's)"""
    return 'me' if (vb // turn) % 2 == 0 else 'main'


def bnote(vb, turn):
    """gl28d2: the hook at a turn's first vblank runs before the turn changes (xl_psp_gl_vblank comes before xl_psp_frame's
    mq_turn_hook in host_vb_end): its 'turn' label is the turn before's"""
    return (' (boundary hook: the label is the turn before\'s; the %s turn of vb %d begins after this hook)' % (turn_of(vb, turn), vb)
            if vb % turn == 0 else '')


def run(cmd):
    try:
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
        return r.stdout.rstrip('\n')
    except Exception as e:  # noqa
        return 'could not run %s: %s' % (cmd[2] if len(cmd) > 2 else cmd, e)


ARMS = ('O', 'X', 'Y', 'W')


def lighter(a, b, t=6):
    """an4.py's rule: pixels where a's luma (R*299+G*587+B*114)//1000 is at least t above b's, outside the HUD boxes"""
    w, h, ra = a; _, _, rb = b; n = 0
    for y in range(h):
        x1 = ra[y]; x2 = rb[y]
        if x1 == x2:
            continue
        for x in range(w):
            p = 3 * x
            l1 = (x1[p] * 299 + x1[p + 1] * 587 + x1[p + 2] * 114) // 1000; l2 = (x2[p] * 299 + x2[p + 1] * 587 + x2[p + 2] * 114) // 1000
            if l1 - l2 >= t and not px_hud(x, y):
                n += 1
    return n


def dz_parse(t):
    """'; dz X 42:n oth:n pair:n [sub:n ]fb:a,b,c,d,e,f bias:n w:n aux:n' -> dict, or None (gl30: sub, 0 when absent)"""
    m = re.match(r'; dz (\S+) 42:(\d+) oth:(\d+) pair:(\d+) (?:sub:(\d+) )?fb:(\d+),(\d+),(\d+),(\d+),(\d+),(\d+) bias:(\d+) w:(\d+) aux:(\d+)',
                 t or '')
    if not m:
        return None
    g = m.groups()
    return dict(arm=g[0], n42=int(g[1]), oth=int(g[2]), pair=int(g[3]), sub=int(g[4] or 0), fb=[int(x) for x in g[5:11]], bias=int(g[11]),
                w=int(g[12]), aux=int(g[13]))


def dz_summary(mine, tw, turn):
    """per arm: frames with digests, 0x42 batches, paired, fallbacks by reason, biased, W, the largest copy, digests equal to the twin"""
    S = {}
    for k in sorted(mine):
        for e in mine[k]:
            d = dz_parse(e[2])
            if not d:
                continue
            s = S.setdefault(d['arm'], dict(n=0, eq=0, me=0, n42=0, oth=0, pair=0, sub=0, fb=[0] * 6, bias=0, w=0, aux=0, nopair=[]))
            s['n'] += 1; s['eq'] += tw.get(k) == mine[k]; s['me'] += turn_of(k, turn) == 'me'
            for f in ('n42', 'oth', 'pair', 'sub', 'bias', 'w'):
                s[f] += d[f]
            s['fb'] = [a + b for a, b in zip(s['fb'], d['fb'])]; s['aux'] = max(s['aux'], d['aux'])
            if d['arm'] in ('X', 'Y') and d['pair'] + d['sub'] < d['n42'] and len(s['nopair']) < 12:
                s['nopair'].append((k, d['n42'] - d['pair'] - d['sub']))
    if not S:
        print('  no dz text on the digest lines (MGS2_GL_DZ off)')
        return
    print('  by arm (frames with digests; ME turns; equal to the twin; 0x42 batches; other kinds; paired; gl30 subset pairs; fallbacks base/matrix/vertex set/'
          'triangles/uv-colour/room; biased; W; the largest copy in bytes):')
    for a in sorted(S, key=lambda x: (x[0] != 'O', x)):
        s = S[a]
        print('    %-4s frames %4d (ME %4d), = twin %4d; 0x42 %6d, other %d; paired %6d; subset %6d; fallbacks %s; biased %d; W %d; copy at most %d%s' % (
            a, s['n'], s['me'], s['eq'], s['n42'], s['oth'], s['pair'], s['sub'], '/'.join(map(str, s['fb'])), s['bias'], s['w'], s['aux'],
            ('; frames with unpaired decals (vb, n): %s' % s['nopair']) if s['nopair'] else ''))


def gl29_lines(L, lo, hi):
    print('\n== gl29 dz windows (frames by turn, counts, list build time, R held)')
    n = 0
    for l in L:
        m = re.match(r'picture: gl29 dz window vb (\d+)-(\S+) arm (\S+): frames (\d+) \(ME turns (\d+), main (\d+)\); 0x42 decals (\d+) \(of '
                     r'other kinds (\d+)\), paired (\d+), (?:subset (\d+), )?fallbacks base (\d+) matrix (\d+) vertex set (\d+) triangles (\d+) uv/colour (\d+) '
                     r'room (\d+), biased (\d+), W (\d+), the largest copy (\d+) bytes; list build: ME turns ([\d.]+) ms \(the largest '
                     r'([\d.]+)\), main ([\d.]+) ms \(the largest ([\d.]+)\); R held (\d+) vblank hooks \(ME turns (\d+), main (\d+)\), (\d+) '
                     r'from the window', l)
        if not m:
            continue
        g = list(m.groups()); n += 1
        g[9] = g[9] or '0'   # gl30: the subset pairs (absent in gl29 logs)
        fr = int(g[3]); me, mn = int(g[4]), int(g[5])
        print('  %6s-%-5s %-3s frames %3s (ME %3s, main %3s); 0x42 %5s paired %5s subset %5s fb %s; bias %s W %s; copy %6s B; list ms ME %7s '
              '(avg %.2f, max %s), main %7s (avg %.2f, max %s); R held %3s (ME %s, main %s), from the 10th vb %s' % (
                  g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[8], g[9], ','.join(g[10:16]), g[16], g[17], g[18], g[19],
                  float(g[19]) / me if me else 0, g[20], g[21], float(g[21]) / mn if mn else 0, g[22], g[23], g[24], g[25], g[26]))
    if not n:
        print('  none')
    for l in L:
        if l.startswith('gl29 dz: R held'):
            print('  ' + l)
    print('\n== gl29 shots (arm, last list = shot frame, depth files, VRAM read)')
    n = bad = 0
    for l in L:
        m = re.match(r'picture: the frame of vb (\d+) (saved as|NOT saved:) (\S+); gl29 shot vb (\d+) \((\S+), (\w+) turn\): dz (\S+); last '
                     r'list = shot frame: (\w+); colour VRAM read ([\d.]+) ms; (.*)$', l)
        if not m:
            continue
        g = m.groups(); n += 1
        if g[7] != 'yes' or g[1] != 'saved as' or 'NOT saved' in g[9]:
            bad += 1
        if int(g[3]) < lo - 1000 or int(g[3]) > hi:
            continue
        z = re.sub(r'\S*/(zb\w+_\d+\.raw)', r'\1', g[9])
        print('  shot vb %s (%s, %s turn): frame %s arm %s; last list = shot frame %s; colour %s ms; %s%s' % (
            g[3], g[4].replace('MGS2_GL_', ''), g[5], g[0], g[6], g[7], g[8], z, '' if g[1] == 'saved as' else '  <- NOT SAVED'))
    gv = [l for l in L if 'GAVE UP' in l]
    print('  gl29 shot lines %d; with a problem (not saved, last list not the shot frame, depth not saved) %d; GAVE UP lines %d' % (n, bad, len(gv)))
    for l in gv[:5]:
        print('    ' + l)


def main():
    o = args(sys.argv[1:])
    L = open(o['log'], errors='replace').read().split('\n')
    lo, hi = o['from'], o['to']
    print('== rd29: %s (md5 %s, %d lines); window vb %d..%d' % (o['log'], md5(o['log']), len(L), lo, hi))
    turn = 100
    for l in L:
        m = re.match(r'me picture loop: turns of (\d+) vblanks', l)
        if m:
            turn = int(m.group(1)); break
    for l in L:
        if (l.startswith('gl28 ') or l.startswith('gl29 ')) and '(MGS2_' in l[:60]:
            print('  ' + l[:220] + (' ...' if len(l) > 220 else ''))
        if l.startswith('end:'):
            print('  ' + l)

    print('\n== hashes and digests')
    if os.path.exists(o['hashref']):
        print('  ' + run([sys.executable, '-I', M + '/tools/refcmp.py', '--partial', o['hashref'], o['log']]).replace('\n', '\n  '))
    if os.path.exists(o['twin']):
        print('  ' + run([sys.executable, '-I', M + '/tools/emucmp.py', o['log'], o['twin']]).replace('\n', '\n  '))
        # by turn: the frame digests against the twin's
        tw = {}
        for l in open(o['twin'], errors='replace'):
            m = re.match(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+).*?(; dz .*)?$', l)
            if m:
                tw.setdefault(int(m.group(1)), []).append((m.group(2), m.group(3), m.group(4) or ''))
        mine = {}
        for l in L:
            m = re.match(r'picture: vb (\d+) buffer \d+ list (\w+) frame (\w+).*?(; dz .*)?$', l)
            if m:
                mine.setdefault(int(m.group(1)), []).append((m.group(2), m.group(3), m.group(4) or ''))
        by = {'me': [0, 0, []], 'main': [0, 0, []]}
        for k in sorted(mine):
            t = turn_of(k, turn); by[t][0] += 1
            if tw.get(k) == mine[k]:
                by[t][1] += 1
            else:
                by[t][2].append(k)
        for t in ('me', 'main'):
            n, e, d = by[t]
            print('  digests in %s turns (by the frame\'s vblank): %d, equal to the twin %d (list, frame and the dz text)%s' % (
                t, n, e, (', differing: %s' % d[:20]) if d else ''))
        dz_summary(mine, tw, turn)
    gl29_lines(L, lo, hi)

    print('\n== gl28 me rest')
    rl = [l for l in L if ' gl28 me rest: rests ' in l]
    s = {'rests': 0, 'halt': 0, 'kicks': 0, 'asleep': 0.0}
    for l in rl:
        m = re.search(r'rests (\d+) \(woken in HALT (\d+).*?kicks (\d+) \+.*?ME asleep ([\d.]+) ms', l)
        if m:
            s['rests'] += int(m.group(1)); s['halt'] += int(m.group(2)); s['kicks'] += int(m.group(3)); s['asleep'] += float(m.group(4))
    print('  main-turn rest lines %d: rests %d, woken in HALT %d, kicks %d, ME asleep %.1f s' % (len(rl), s['rests'], s['halt'], s['kicks'],
                                                                                          s['asleep'] / 1000))
    nt = [l for l in L if re.match(r'me turn vb \d+-\d+ gl28 me rest:', l)]
    print('  ME-turn notes (a rest held over the switch) %d' % len(nt))
    for l in L:
        if l.startswith('gl28 me rest totals'):
            print('  ' + l[:400] + (' ...' if len(l) > 400 else ''))

    print('\n== GE2 per ME turn (window)')
    ab = {}
    for l in L:
        m = re.match(r'me turn vb (\d+) gl28 ge2 a/b: GE2 (on|off)', l)
        if m:
            ab[int(m.group(1))] = m.group(2)
    ge2 = {}
    for l in L:
        m = re.match(r'me turn vb (\d+)-(\d+) \(([\d.]+) ms\).*?GE2 (\d+): lists (\d+), drawing ([\d.]+) ms.*?begun while the GE drew the frame '
                     r'before (\d+); gl24 TWO (\d+)', l)
        if m:
            a = int(m.group(1))
            ge2[a] = m.groups()
            if lo <= a <= hi:
                print('  me turn %s-%s: %s ms; GE2 %s%s, lists %s, GE drawing %s ms, frames begun while the GE drew %s; TWO %s' % (
                    m.group(1), m.group(2), m.group(3), m.group(4), (' (a/b line: %s)' % ab[a]) if a in ab else '', m.group(5), m.group(6),
                    m.group(7), m.group(8)))
    print('  ge2 a/b lines: %s' % (', '.join('%d %s' % (k, ab[k]) for k in sorted(ab)) or 'none'))

    print('\n== display (gl27 lines with the gl28 fbchk counts), turns in the window')
    for l in L:
        m = re.match(r'(me|main) turn vb (\d+)-(\d+) gl27 display: flip wait (\w+); lists held for the screen (\d+) \(([\d.]+) ms\); lists drawn '
                     r'into the shown buffer (\d+)(; gl28 fbchk: lists (\d+), into the buffer the display shows now \(IMMEDIATE\) (\d+), into the '
                     r'buffer it shows next \(NEXTFRAME\) (\d+), in the display vblank of the show (\d+), display vblanks since the show at most '
                     r'(\d+))?', l)
        if m and int(m.group(3)) > lo and int(m.group(2)) <= hi:
            g = m.groups()
            print('  %-4s %s-%s flip %s: held %s (%s ms), drawn into the shown buffer %s%s' % (g[0], g[1], g[2], g[3], g[4], g[5], g[6],
                  ('; fbchk lists %s, IMMEDIATE %s, NEXTFRAME %s, in the show\'s vblank %s, at most %s vblanks after the show' % g[8:13])
                  if g[7] else ''))
    for l in L:
        if l.startswith('picture display (gl27): flip wait'):
            print('  end: ' + l[l.find('MGS2_GL_FLIPWAIT'):])
    fd = [l for l in L if l.startswith('picture: gl28 fbchk ')]
    print('  fbchk detail lines %d (the first check, then lists into the buffer on the screen or shown next, and the first answer that '
          'names neither buffer; 50 at most)' % len(fd))
    for l in fd[:12]:
        print('    ' + l[len('picture: gl28 fbchk '):])
    # gl28d2: the counts mean something only if the display's answers name buffer 0 or 1 (fb_off knows the 0x04/0x44 VRAM views)
    if fd:
        m = re.search(r'IMMEDIATE ([0-9a-f]+) \(buffer (\S+), rc (-?\d+)\), NEXTFRAME ([0-9a-f]+) \(buffer (\S+), rc (-?\d+)\)', fd[0])
        if m:
            bad = [n for n, b, rc in (('IMMEDIATE', m.group(2), m.group(3)), ('NEXTFRAME', m.group(5), m.group(6)))
                   if b not in ('0', '1') or int(rc) < 0]
            if bad:
                print('  WARNING: the first fbchk line\'s %s answer names neither buffer 0 nor buffer 1 (or was refused): the IMMEDIATE/'
                      'NEXTFRAME counts cannot match and read 0 whatever the display does; look at the raw address on that line' %
                      ' and '.join(bad))
            else:
                print('  the first fbchk line\'s answers name buffer %s (IMMEDIATE) and buffer %s (NEXTFRAME): the counts can match' %
                      (m.group(2), m.group(5)))
        nn = [l for l in fd if 'AN ANSWER NAMES NEITHER BUFFER' in l]
        if nn:
            print('  WARNING: %d fbchk line(s) with an answer naming neither buffer; the first: %s' % (len(nn), nn[0][len('picture: gl28 fbchk '):]))
    for l in L:
        m = re.search(r'gl28 fbchk from vb \d+: .*?answers refused (\d+)(?:; answers naming neither buffer (\d+))?', l)
        if m:
            print('  fbchk end: answers refused %s, answers naming neither buffer %s%s' % (m.group(1), m.group(2) if m.group(2) is not None
                  else '(not in this log)', '  <- WARNING: those answers cannot match a buffer' if m.group(2) and int(m.group(2)) else ''))

    print('\n== eye (R trigger)')
    ev = []
    for l in L:
        m = re.match(r'eye: R (down|up) vb (\d+) t (\d+) turn (me|main) two (\d+) ge2 (\d+) flip (on|off) shown frame (-?\d+) buffer (-?\d+)'
                     r'.*?(?:; dz (\S+))?$', l)
        if m:
            ev.append(m.groups())
        elif l.startswith('eye: t 0'):
            print('  ' + l)
    down = None; held = []
    for g in ev:
        d, vb, t = g[0], int(g[1]), int(g[2])
        gab = ab.get(((vb - 1 if vb % turn == 0 else vb) // turn) * turn) if g[3] == 'me' else None   # (a boundary hook: the turn before)
        extra = ''
        if d == 'down':
            down = (vb, t)
        elif down:
            extra = '; held %.1f s (vb %d-%d)' % ((t - down[1]) / 1000.0, down[0], vb); held.append((down[0], vb)); down = None
        print('  R %-4s vb %d t %.1f s: %s turn (TWO %s, GE2 %s%s), flip %s, shown frame %s in buffer %s%s%s%s' % (
            d, vb, t / 1000.0, g[3], g[4], g[5], (', a/b ' + gab) if gab else '', g[6], g[7], g[8], (', arm ' + g[9]) if g[9] else '',
            extra, bnote(vb, turn)))
    if not ev:
        print('  no R presses')
    for l in L:
        if l.startswith('gl28 eye:') or l.startswith('gl28 live:') or l.startswith('gl29 dz: R held'):
            print('  ' + l)

    sw = [l for l in L if l.startswith('picture: gl28 shot ')]
    print('\n== shots that kept the flip wait: %d' % len(sw))
    for l in sw:
        print('  ' + l[len('picture: gl28 shot '):])

    print('\n== live captures')
    refs = {}
    if os.path.isdir(o['ref']):
        idx = os.path.join(o['ref'], 'live_ref.txt')
        if os.path.exists(idx):
            for l in open(idx, errors='replace'):
                m = re.match(r'picture: gl28 live vb (\d+) \(.*?\) turn \w+: buffer \d frame vb (-?\d+)', l)
                if m and os.path.exists(os.path.join(o['ref'], 'live_%s.bmp' % m.group(1))):
                    refs.setdefault(int(m.group(2)), os.path.join(o['ref'], 'live_%s.bmp' % m.group(1)))
        for f in os.listdir(o['ref']):
            m = re.match(r'shot_(\d+)\.bmp$', f)
            if m:
                refs.setdefault(int(m.group(1)), os.path.join(o['ref'], f))
    print('  references (emulator frames by their vblank) in %s: %s' % (o['ref'], ', '.join(str(k) for k in sorted(refs)) or 'none'))
    lv = []
    for l in L:
        m = re.match(r'picture: gl28 live vb (\d+) \(([^)]*)\) turn (\w+): buffer (\d) frame vb (-?\d+) \(shown #(\d+), (\d+) display vblanks '
                     r'ago\); the display scans buffer (\S+); the other buffer frame vb (-?\d+); the GE (.*?); lists waiting (\d+); VRAM read '
                     r'([\d.]+) ms, all ([\d.]+) ms: (.*)$', l)
        if m:
            lv.append(m.groups())
    for g in lv:
        vb, why, tt, buf, fvb, sn, dvc, disp, other, ge, lw, rd, al, res = g
        am = re.search(r'; dz (\S+)$', res); arm = am.group(1) if am else None
        print('  live %s (%s, %s turn): buffer %s frame vb %s%s (%s vblanks after its show; display scans %s; other buffer frame %s; GE %s; '
              'lists waiting %s); VRAM %s ms, all %s ms%s' % (vb, why, tt, buf, fvb, (' arm ' + arm) if arm else '', dvc, disp, other,
                                                              ge.replace('draws frame vb ', 'draws '), lw, rd, al, bnote(int(vb), turn)))
        p = os.path.join(o['live'], 'live_%s.bmp' % vb)
        if not os.path.exists(p):
            print('    no picture %s' % p); continue
        try:
            img = load_bmp(p)
        except Exception as e:  # noqa
            print('    %s: cannot read (%s)' % (p, e)); continue
        nb, nout, out, mean = black_clusters(img)
        w, h = img[0], img[1]
        edge = [k for k in out if (k[1] == k[3] and k[1] in (0, w - 1)) or (k[2] == k[4] and k[2] in (0, h - 1))]   # 1-px lines on the
        terr = [k for k in out if k not in edge]                                                                   # screen's edge
        big = [k for k in terr if k[0] >= MIN_CL]
        print('    black px %d; outside the HUD boxes %d: %d clusters (%d of %d px or more)%s%s; mean level %.1f%s' % (
            nb, nout, len(terr), len(big), MIN_CL, (': largest (n, x0, y0, x1, y1) %s' % big[:4]) if big else '',
            ('; and %d one-pixel lines on the screen\'s edge (%d px)' % (len(edge), sum(k[0] for k in edge))) if edge else '', mean,
            ' (a fade: dark picture)' if mean < 30 else ''))
        r = refs.get(int(fvb))
        if r:
            try:
                rimg = load_bmp(r); d = diff(img, rimg)
                print('    against the emulator\'s frame %s (%s): pixels differing %d, by more than 32 %d, black here and not there (outside '
                      'the HUD) %d; PSP lighter by 6 or more in luma (outside the HUD) %d' % (fvb, os.path.basename(r), d[0], d[1], d[2],
                                                                                          lighter(img, rimg)))
            except Exception as e:  # noqa
                print('    reference %s: cannot compare (%s)' % (r, e))
        else:
            near = min(refs, key=lambda k: abs(k - int(fvb))) if refs else None
            print('    no emulator reference of frame %s%s' % (fvb, (' (nearest %d)' % near) if near is not None else ''))
    if not lv:
        print('  no live captures in the log')

    if o['bat'] and os.path.exists(o['bat']):
        print('\n== battery.txt in the window (%s, md5 %s)' % (o['bat'], md5(o['bat'])))
        for l in open(o['bat'], errors='replace'):
            m = re.match(r'(\S.*?) t (\d+) s, vb (-?\d+): (\d+)%, (-?\d+) mV, (-?\d+) C.*?external power (\d), charging (\d)', l)
            if m and lo <= int(m.group(3)) <= hi:
                pic = re.search(r'picture: ([^,;]*?turn[^,;]*?, queue head \d+ tail \d+)', l)
                print('  %s t %s s vb %s: %s%%, %s mV, %s C, ext %s, chg %s%s' % (m.group(1), m.group(2), m.group(3), m.group(4), m.group(5),
                      m.group(6), m.group(7), m.group(8), ('; ' + pic.group(1)) if pic else ''))


if __name__ == '__main__':
    main()
