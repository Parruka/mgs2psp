#!/usr/bin/env python3
"""rd36.py LOG [--bat battery.txt] [--live DIR] [--ref DIR] [--twin LOG] [--hashref FILE] [--from VB] [--to VB] [--natfrom VB] [--natto VB]

gl36 (2026-11-21, attic/gl36/gl36_plan.md): rd35.py plus gl36's lines:
  - 'nat reuse vb A-B <me|main> turn arm N reuse R': the reuse of a fast native run's VU memory writes and kept position boxes from the
    picture before (MGS2_NAT_REUSE / MGS2_NAT_REUSE_CYCLE): runs replayed (their groups, the groups whose VU writes are skipped, the
    groups that must write), runs recorded (kept or not), the check mode's counts (MGS2_NAT_RCHK: checked, DIFFERING, which must be 0),
    the kept boxes taken / cut / positions then needed, no table room, replays with a group not taken (must be 0);
  - 'nat reuse (gl36): the table ...' (the table's memory) and 'reuse totals (gl36)' (the run's end);
  the turn tables are by picture turn, native arm, in-place arm, fast arm, keep bits and reuse arm; the split ('nat split') and the
  chains ('me chains') take the keep bits and the reuse arm of the same turn's lines, and their means are by them, with the differences.
Defaults: --ref build_gl36/emu_e3/pic, --twin build_gl36/tw/x86/out_rx.log (the twin with the delivered keys: MGS2_NAT_CYCLE=3,3,3,3,
MGS2_NAT_INPL=1, MGS2_NAT_FAST=1, MGS2_NAT_KEEP=1, MGS2_NAT_REUSE_CYCLE=1,1,0,0, DZ X in the whole run), --hashref
build_gl36/deliver/hashref.txt.  The rest as rd35:

rd35.py LOG [--bat battery.txt] [--live DIR] [--ref DIR] [--twin LOG] [--hashref FILE] [--from VB] [--to VB] [--natfrom VB] [--natto VB]

gl35 (2026-11-21, gl35_plan.md): rd34.py plus gl35's line:
  - 'nat keep vb A-B <me|main> turn arm N keep K': the keeps for the fast native groups (MGS2_NAT_KEEP / MGS2_NAT_KEEP_CYCLE; bits 1
    drawn areas, 2 A+D packet, 4 / 8 texture), the groups flagged, the steps kept (drawn areas and textures replayed, CLUT calls not
    made, PRIM writes in line), the check mode's counts (MGS2_NAT_KCHK: checked, DIFFERING, which must be 0);
  the turn tables are by picture turn, native arm, in-place arm, fast arm and keep bits; the split ('nat split') and the chains ('me
  chains') take the keep bits of the 'nat keep' line of the same turn, and their means are by keep bits, with keep - no keep.
Defaults: --ref build_gl35/emu_e3/pic, --twin build_gl35/tw/x86/out_dx.log (the twin with the delivered keys: MGS2_NAT_CYCLE=3,3,3,3,
MGS2_NAT_INPL=1, MGS2_NAT_FAST=1, MGS2_NAT_KEEP_CYCLE=15,15,0,0, DZ X in the whole run), --hashref build_gl35/deliver/hashref.txt.
The rest as rd34:

rd34.py LOG [--bat battery.txt] [--live DIR] [--ref DIR] [--twin LOG] [--hashref FILE] [--from VB] [--to VB] [--natfrom VB] [--natto VB]

gl34 (2026-11-21, gl34_plan.md): rd33.py plus gl34's lines:
  - 'nat fast vb A-B <me|main> turn arm N fast K': the fast native front end's turn arm (MGS2_NAT_FAST / MGS2_NAT_FAST_CYCLE), the groups
    flagged, taken by the fast front end and declined by reason, the transforms kept, the pointers found by their own hints; the turn
    tables are by picture turn, native arm, in-place arm and fast arm;
  - 'nat split vb A-B' (ME turns, MGS2_NAT_SPLIT=1): the ME's time inside the native path by stage (us a timed group; one group in
    about 32 is timed, each stage's figure holds one clock read, whose mean cost the line gives), the turn's native time, the hand-off;
  - 'me chains vb A-B' (ME turns): the ME's walk time by chain of the frame's list (a chain is one transfer), with converter calls;
  per turn in the window, the means by fast arm, and fast - plain.
Defaults: --ref build_gl34/emu_e3/pic, --twin build_gl34/tw/x86/out_dx2.log (the twin with the delivered keys: MGS2_NAT_CYCLE=3,3,3,3,
MGS2_NAT_INPL=1, MGS2_NAT_FAST_CYCLE=0,0,1,1, DZ X in the whole run), --hashref build_gl34/deliver/hashref.txt.  The rest as rd33:

rd33.py LOG [--bat battery.txt] [--live DIR] [--ref DIR] [--twin LOG] [--hashref FILE] [--from VB] [--to VB] [--natfrom VB] [--natto VB]

gl31 (2026-11-20, gl31_plan.md G6): rd30.py plus native step 1's lines (MGS2_NAT_CYCLE; the ship meshes drawn from records):
  - the 'native step 1:' start line (the arrays' memory) and the 'native step 1 totals:' end line;
  - one row per 'nat turn vb A-B <me|main> turn arm N' line, joined with the harness's own times of the same 100 vblanks: the
    'window vb B' line (mean ms a step and its parts) and, in ME turns, the 'me turn vb A-B' line (the ME's records, fills, walks, hooks,
    idle; the main CPU's waits for copies and fills): the nat counters (calls recorded, groups, pictures, bytes a picture), the fallbacks
    (the builder run instead, by reason), recorded with no array, plans undone, early fills, groups dropped, the native (merge) time, the
    holds (the main CPU's waits for fills and copies);
  - per arm and per ME / main turn: the turns, the mean of each time, over all nat turns and over --natfrom..--natto (default the
    rain wait and walk, 29,500..31,400);
  - every turn that breaks 'dropped 0, fallbacks 0, early fills 0, recorded with no array 0, partly 0' is listed (NAT PROBLEM).
The arm of a nat turn is the stub's from the turn's second vblank (the first picture of a turn belongs to the arm before); the ME / main
picture turns are (vb // 100) & 1, one vblank off from the nat turns.  Defaults: --ref build_gl33/emu_e3/pic (the emulator's frames
of the delivered EBOOT with a picture every 2 vblanks in the walk: shot_<frame vb>.bmp; run e3), --twin build_gl33/tw/x86/out_dx.log
(the twin with the delivered keys: MGS2_NAT_CYCLE=3,3,3,3, MGS2_NAT_INPL_CYCLE=0,0,1,1 and
MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X), --hashref build_gl33/deliver/hashref.txt (that twin run's hashes).
gl33 (2026-11-21, gl33_plan.md): rd31.py plus the in-place rule's counter per turn (the 'nat inpl' lines: the arm and the groups
flagged static) next to gl31's "the groups' data in the copy / read from RAM" counts; the turn tables are by picture turn, native
arm and in-place arm (inpl 0: every stream copied; inpl 1: static groups' streams read in RAM by the ME's walk).  The rest as rd30:

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
    o = {'log': None, 'bat': None, 'live': None, 'ref': M + '/build_gl36/emu_e3/pic', 'twin': M + '/build_gl36/tw/x86/out_rx.log',
         'hashref': M + '/build_gl36/deliver/hashref.txt', 'from': 29400, 'to': 31500, 'natfrom': 29500, 'natto': 31400}
    i = 0
    while i < len(a):
        k = a[i]
        if k.startswith('--') and i + 1 < len(a):
            o[k[2:]] = a[i + 1]; i += 2
        else:
            o['log'] = k; i += 1
    if not o['log']:
        print(__doc__); sys.exit(2)
    o['from'] = int(o['from']); o['to'] = int(o['to']); o['natfrom'] = int(o['natfrom']); o['natto'] = int(o['natto'])
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


NAT_ARM = {0: 'builder', 3: 'native', 4: 'not drawn'}


def nat_lines(L, lo, hi):
    """gl31: the 'nat turn' lines with the harness's times of the same 100 vblanks"""
    print('\n== native step 1 (MGS2_NAT / MGS2_NAT_CYCLE) and the static streams in place of gl33 (MGS2_NAT_INPL / MGS2_NAT_INPL_CYCLE)')
    st = [l for l in L if l.startswith('native step 1:')]
    for l in st:
        print('  ' + l)
    if not st:
        print('  no "native step 1:" start line')
    win = {}
    for l in L:
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), GS walk '
                     r'([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
        if m:
            g = m.groups(); win[int(g[0])] = dict(steps=int(g[1]), mean=float(g[2]), logic=float(g[3]), dp=float(g[4]), iop=float(g[5]),
                                                  gsw=float(g[6]), pic=float(g[8]), gew=float(g[9]), max=float(g[10]))
    met = {}
    for l in L:
        m = re.match(r'me turn vb (\d+)-(\d+) \(([\d.]+) ms\): the ME\'s own time \(ms\): records ([\d.]+), thinning ([\d.]+), fills ([\d.]+), '
                     r'walks ([\d.]+), hooks ([\d.]+) .*?idle ([\d.]+), all ([\d.]+); hooks (\d+), lists (\d+).*?the main CPU waited \(ms\): '
                     r'copies as they ended ([\d.]+), fills ([\d.]+), the arena ([\d.]+), the rest ([\d.]+)', l)
        if m:
            g = m.groups(); met[int(g[1])] = dict(ms=float(g[2]), rec=float(g[3]), fills=float(g[5]), walks=float(g[6]), hooks=float(g[7]),
                                                  idle=float(g[8]), lists=int(g[11]), wcopy=float(g[12]), wfill=float(g[13]),
                                                  warena=float(g[14]), wrest=float(g[15]))
    dzw = {}
    R = []
    pat = re.compile(
        r'nat turn vb (\d+)-(\d+) (me|main) turn arm (\d+) \(([^;]*); cycle turn (-?\d+)\): calls recorded (\d+), groups (\d+), pictures with '
        r'records (\d+); snapshot bytes a picture: mean (\d+), most (\d+); stubbed with no record (\d+); calls with no group (\d+); the builder '
        r'run instead \(dropped records\) (\d+): array full (\d+), key order (\d+), near the buffer\'s end (\d+), other (\d+); recorded with no '
        r'array (\d+); groups left untaken (\d+); transfers planned with groups (\d+) \((\d+) groups\), groups taken by walks from RAM (\d+), '
        r'plans undone for the groups\' room (\d+), early fills for an array\'s reuse (\d+); the picture\'s side: records\' groups copied (\d+) '
        r'\((\d+) groups\), merge runs (\d+), groups to the core (\d+), fed word by word (\d+), dropped (\d+), drawn at a walk\'s end (\d+), run '
        r'lanes stopped at a key (\d+), the groups\' data in the copy (\d+), read from RAM (\d+), partly \(dropped\) (\d+); native time ([\d.]+) '
        r'ms \(([^)]*)\); hold: the main CPU waited for fills ([\d.]+) ms, for copies as they ended ([\d.]+) ms(.*)$')
    nl = [l for l in L if l.startswith('nat turn vb ')]
    for l in nl:
        m = pat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        g = m.groups()
        r = dict(a=int(g[0]), b=int(g[1]), kind=g[2], arm=int(g[3]), armtxt=g[4], cyc=int(g[5]), calls=int(g[6]), groups=int(g[7]), pics=int(g[8]),
                 bmean=int(g[9]), bmost=int(g[10]), stub_norec=int(g[11]), nogroup=int(g[12]), fb=int(g[13]), fbr=[int(x) for x in g[14:18]],
                 noarr=int(g[18]), untaken=int(g[19]), plans=int(g[20]), plang=int(g[21]), ramg=int(g[22]), undone=int(g[23]), early=int(g[24]),
                 copied=int(g[25]), copiedg=int(g[26]), merges=int(g[27]), core=int(g[28]), wordfed=int(g[29]), dropped=int(g[30]),
                 atend=int(g[31]), lanes=int(g[32]), incopy=int(g[33]), fromram=int(g[34]), partly=int(g[35]), nat_ms=float(g[36]),
                 nat_note=g[37], hfill=float(g[38]), hcopy=float(g[39]), tail=g[40])
        r['part'] = r['b'] - r['a'] < 100   # the run's last, cut turn: no window of its own
        r['win'] = None if r['part'] else win.get(r['b']); r['me'] = met.get(r['b']) if r['kind'] == 'me' and not r['part'] else None
        R.append(r)
    print('  nat turn lines %d (parsed %d)' % (len(nl), len(R)))
    # gl33: the in-place line of the same turn ('nat inpl vb A-B <me|main> turn arm N inpl K (...): groups flagged static N')
    ipat = re.compile(r'nat inpl vb (\d+)-(\d+) (me|main) turn arm (\d+) inpl (\d+) \([^)]*\): groups flagged static (\d+)')
    IP = {}; il = [l for l in L if l.startswith('nat inpl vb ')]
    for l in il:
        m = ipat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        IP[(int(m.group(1)), int(m.group(2)))] = (int(m.group(5)), int(m.group(6)))
    for r in R:
        r['inpl'], r['static'] = IP.get((r['a'], r['b']), (0, 0))
    print('  nat inpl lines %d (parsed %d)' % (len(il), len(IP)))
    # gl34: the fast front end's line of the same turn
    fpat = re.compile(r'nat fast vb (\d+)-(\d+) (me|main) turn arm (\d+) fast (\d+) \([^)]*\): groups flagged (\d+); the picture\'s side: taken (\d+), '
                      r'declined: state (\d+), entry (\d+), buffers (\d+), stream counts (\d+), material tag (\d+), tables full (\d+), path 1 busy '
                      r'(\d+); transforms taken from the last keyed call (\d+), computed and kept (\d+)(?:; pointers by their own hints (\d+) '
                      r'\(MGS2_NAT_HCHK (-?\d+): checked (\d+), differing (\d+)\))?')
    FP = {}; fl = [l for l in L if l.startswith('nat fast vb ')]
    for l in fl:
        m = fpat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        g = m.groups()
        FP[(int(g[0]), int(g[1]))] = dict(fast=int(g[4]), flagged=int(g[5]), taken=int(g[6]), dec=[int(x) for x in g[7:14]], xk=int(g[14]),
                                          xc=int(g[15]), hint=int(g[16] or 0), hchk=int(g[18] or 0), hbad=int(g[19] or 0))
    for r in R:
        f = FP.get((r['a'], r['b']))
        r['fast'] = f['fast'] if f else 0; r['fp'] = f
    print('  nat fast lines %d (parsed %d)' % (len(fl), len(FP)))
    # gl35: the keeps' line of the same turn
    kpat = re.compile(r'nat keep vb (\d+)-(\d+) (me|main) turn arm (\d+) keep (\d+) \([^)]*\): groups flagged (\d+); the picture\'s side: drawn areas '
                      r'replayed (\d+), checked (\d+), differing (\d+); textures replayed (\d+), checked (\d+), differing (\d+), records (\d+); CLUT '
                      r'calls not made (\d+), PRIM writes in line (\d+) \(MGS2_NAT_KCHK (\d+)\)')
    KP = {}; kl = [l for l in L if l.startswith('nat keep vb ')]
    for l in kl:
        m = kpat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        g = [int(x) if x.isdigit() else x for x in m.groups()]
        KP[(g[0], g[1])] = dict(keep=g[4], flagged=g[5], dr=g[6], dchk=g[7], dbad=g[8], tr=g[9], tchk=g[10], tbad=g[11], trec=g[12],
                                clut=g[13], prim=g[14], kchk=g[15])
    for r in R:
        k = KP.get((r['a'], r['b']))
        r['keep'] = k['keep'] if k else 0; r['kp'] = k
    print('  nat keep lines %d (parsed %d)' % (len(kl), len(KP)))
    KEEP_OF.clear(); KEEP_OF.update({ab: v['keep'] for ab, v in KP.items()})
    # gl36: the reuse's line of the same turn
    upat = re.compile(r'nat reuse vb (\d+)-(\d+) (me|main) turn arm (\d+) reuse (\d+) \([^)]*\): runs replayed (\d+) \(groups (\d+), skipping (\d+), '
                      r'in S (\d+)\), recorded (\d+) \(groups (\d+); kept (\d+), not kept (\d+)\), checked (\d+) \(groups (\d+); S differing (\d+), '
                      r'boxes differing (\d+)\); boxes recorded (\d+), kept boxes taken (\d+), cut (\d+), positions then needed (\d+); no table room '
                      r'(\d+); replays with a group not taken (\d+), with a decline (\d+) \(MGS2_NAT_RCHK (\d+)\)')
    RP = {}; ul = [l for l in L if l.startswith('nat reuse vb ')]
    for l in ul:
        m = upat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        g = [int(x) if x.isdigit() else x for x in m.groups()]
        RP[(g[0], g[1])] = dict(ru=g[4], rr=g[5], rrg=g[6], rskip=g[7], ins=g[8], rec=g[9], recg=g[10], kept=g[11], nkept=g[12], chk=g[13],
                                chkg=g[14], sbad=g[15], bbad=g[16], brec=g[17], btak=g[18], bcut=g[19], bpos=g[20], noroom=g[21], ntaken=g[22],
                                rdec=g[23], rchk=g[24])
    for r in R:
        u = RP.get((r['a'], r['b']))
        r['ru'] = u['ru'] if u else 0; r['up'] = u
    print('  nat reuse lines %d (parsed %d)' % (len(ul), len(RP)))
    REUSE_OF.clear(); REUSE_OF.update({ab: v['ru'] for ab, v in RP.items()})
    if not R:
        for l in L:
            if l.startswith('native step 1 totals'):
                print('  ' + l)
        return
    print('  per turn (vb; ME or main picture turn; arm; the harness window of the same 100 vblanks: mean ms a step = logic + draw pipeline + '
          'GS walk + picture + GE wait + IOP; in ME turns the ME\'s walks / records / fills ms; nat: calls, groups, pictures, bytes a '
          'picture mean/most; fallbacks; early fills; dropped; native ms; holds: fills / copies ms):')
    for r in R:
        w = r['win']; e = r['me']
        ws = 'a cut turn (the run ends): not in the means' if r['part'] else ('step %7.2f (logic %6.2f dp %6.2f gsw %5.2f pic %5.2f gew %6.2f) max %7.2f' % (w['mean'], w['logic'], w['dp'], w['gsw'], w['pic'],
                                                                                         w['gew'], w['max'])) if w else 'no window line'
        es = (' | ME walks %7.1f rec %6.1f fills %5.1f idle %7.1f, waits copy %6.1f fill %7.1f' % (e['walks'], e['rec'], e['fills'], e['idle'],
                                                                                              e['wcopy'], e['wfill'])) if e else ''
        print('  %5d-%-5d %-4s arm %d %-9s inpl %d fast %d keep %2d reuse %d | %s%s | calls %5d groups %6d pics %3d bytes %5d/%5d | fb %d early %d dropped %d noarr %d partly %d '
              '| native %7.2f ms | hold fills %7.1f copies %6.1f | static %6d, data in the copy %6d, read from RAM %6d' % (
                  r['a'], r['b'], r['kind'], r['arm'], NAT_ARM.get(r['arm'], '?'), r['inpl'], r['fast'], r['keep'], r['ru'], ws, es,
                  r['calls'], r['groups'], r['pics'], r['bmean'], r['bmost'], r['fb'],
                  r['early'], r['dropped'], r['noarr'], r['partly'], r['nat_ms'], r['hfill'],
                  r['hcopy'], r['static'], r['incopy'], r['fromram']))
    bad = [r for r in R if r['fb'] or r['early'] or r['dropped'] or r['noarr'] or r['partly'] or r['undone'] or r['fromram'] and 0]
    print('  turns with a fallback, an early fill, a dropped group, a record with no array, a partly read group or an undone plan: %d%s' % (
        len(bad), '' if not bad else '  <- NAT PROBLEM'))
    for r in bad[:20]:
        print('    NAT PROBLEM %d-%d %s arm %d: the builder run instead %d (array full %d, key order %d, near the end %d, other %d), early fills '
              '%d, dropped %d, recorded with no array %d, partly %d, plans undone %d' % (r['a'], r['b'], r['kind'], r['arm'], r['fb'], r['fbr'][0],
              r['fbr'][1], r['fbr'][2], r['fbr'][3], r['early'], r['dropped'], r['noarr'], r['partly'], r['undone']))
    odd = [r for r in R if r['arm'] == 0 and (r['calls'] or r['groups'])]
    if odd:
        print('  note: %d builder-arm turns have records (a turn\'s first step still runs the arm before): %s' % (
            len(odd), ', '.join('%d-%d' % (r['a'], r['b']) for r in odd[:12])))

    def table(rows, title):
        print('  ' + title)
        for kind in ('me', 'main'):
            for arm, ipl, fst, kp, ru in sorted(set((r['arm'], r['inpl'], r['fast'], r['keep'], r['ru']) for r in rows)):
                q = [r for r in rows if r['kind'] == kind and r['arm'] == arm and r['inpl'] == ipl and r['fast'] == fst and r['keep'] == kp and r['ru'] == ru and not r['part']]
                if not q:
                    continue
                n = len(q); ww = [r['win'] for r in q if r['win']]; ee = [r['me'] for r in q if r['me']]

                def av(xs, k):
                    return sum(x[k] for x in xs) / len(xs) if xs else float('nan')
                t = '    %-4s turns, arm %d %-9s inpl %d fast %d keep %2d reuse %d: %2d turns; step mean %7.2f ms (logic %6.2f, draw pipeline %6.2f, GS walk %5.2f, picture %5.2f, GE wait %6.2f)' % (
                    kind, arm, NAT_ARM.get(arm, '?'), ipl, fst, kp, ru, n, av(ww, 'mean'), av(ww, 'logic'), av(ww, 'dp'), av(ww, 'gsw'), av(ww, 'pic'), av(ww, 'gew'))
                if ee:
                    t += '; ME a turn: walks %7.1f, records %6.1f, fills %5.1f, idle %7.1f ms; waits: copies %6.1f, fills %7.1f' % (
                        av(ee, 'walks'), av(ee, 'rec'), av(ee, 'fills'), av(ee, 'idle'), av(ee, 'wcopy'), av(ee, 'wfill'))
                t += '; nat a turn: calls %.0f, groups %.0f, pictures %.1f, native %.2f ms, hold fills %.1f, copies %.1f ms' % (
                    sum(r['calls'] for r in q) / n, sum(r['groups'] for r in q) / n, sum(r['pics'] for r in q) / n, sum(r['nat_ms'] for r in q) / n,
                    sum(r['hfill'] for r in q) / n, sum(r['hcopy'] for r in q) / n)
                t += '; groups flagged static %.0f, their data in the copy %.0f, read from RAM %.0f' % (
                    sum(r['static'] for r in q) / n, sum(r['incopy'] for r in q) / n, sum(r['fromram'] for r in q) / n)
                print(t)
    table(R, 'by picture turn, arm, in-place arm, fast arm, keep bits and reuse arm, all nat turns (vb %d-%d):' % (R[0]['a'], R[-1]['b']))
    Q = [r for r in R if r['a'] >= lo and r['b'] <= hi]
    if Q:
        table(Q, 'by picture turn, arm, in-place arm, fast arm, keep bits and reuse arm, the turns inside vb %d-%d (--natfrom / --natto):' % (lo, hi))
    print('  (the emulator has no ME: its "ME" times are the loop thread\'s on the one CPU; only a PSP log gives a speed)')
    for l in L:
        if l.startswith('native step 1 totals') or l.startswith('static streams in place totals'):
            print('  ' + l)
    gl34_lines(L, R, lo, hi)
    gl35_lines(L, R, lo, hi)
    gl36_lines(L, R, lo, hi)


REUSE_OF = {}  # gl36: (a, b) -> the reuse arm of that turn's 'nat reuse' line
KEEP_OF = {}   # gl35: (a, b) -> the keep bits of that turn's 'nat keep' line (the split and chains lines carry only the fast arm)


def gl35_lines(L, R, lo, hi):
    """gl35: the keeps' counts per turn kind and keep bits, and the check mode"""
    print('\n== gl35: the keeps for the fast native groups (MGS2_NAT_KEEP / MGS2_NAT_KEEP_CYCLE; bits 1 drawn areas, 2 A+D packet, 4 / 8 texture)')
    for l in L:
        if l.startswith('keeps for fast native groups totals') or l.startswith('nat: gl35') or 'gl35 keeps for fast native' in l[:80]:
            print('  ' + l[:700])
    for kind in ('me', 'main'):
        for kp in sorted(set(r['keep'] for r in R)):
            q = [r for r in R if r['kind'] == kind and r['keep'] == kp and r.get('kp') and not r['part']]
            if not q:
                continue
            n = len(q); k = [r['kp'] for r in q]
            def a(f):
                return sum(x[f] for x in k) / n
            print('  %-4s turns keep %2d: %2d turns; a turn: groups flagged %.0f; drawn areas replayed %.0f; textures replayed %.0f, records %.0f; '
                  'CLUT calls not made %.0f; PRIM writes in line %.0f; checked: drawn areas %d, textures %d; DIFFERING %d, %d' % (
                      kind, kp, n, a('flagged'), a('dr'), a('tr'), a('trec'), a('clut'), a('prim'), sum(x['dchk'] for x in k), sum(x['tchk'] for x in k),
                      sum(x['dbad'] for x in k), sum(x['tbad'] for x in k)))
    bad = [r for r in R if r.get('kp') and (r['kp']['dbad'] or r['kp']['tbad'])]
    print('  turns where the keeps\' check differs: %d%s' % (len(bad), '  <- KEEP PROBLEM' if bad else ''))


def gl36_lines(L, R, lo, hi):
    """gl36: the reuse's counts per turn kind and reuse arm, the check mode and the problem counters"""
    print('\n== gl36: the reuse of a fast native run\'s VU memory writes and kept boxes from the last picture (MGS2_NAT_REUSE / MGS2_NAT_REUSE_CYCLE)')
    for l in L:
        if l.startswith('nat reuse (gl36)') or l.startswith('reuse totals (gl36)') or l.startswith('nat: gl36'):
            print('  ' + l[:700])
    for kind in ('me', 'main'):
        for ru in sorted(set(r['ru'] for r in R)):
            for inw in (0, 1):
                q = [r for r in R if r['kind'] == kind and r['ru'] == ru and r.get('up') and not r['part'] and (not inw or (r['a'] >= lo and r['b'] <= hi))]
                if not q:
                    continue
                n = len(q); k = [r['up'] for r in q]
                def a(f):
                    return sum(x[f] for x in k) / n
                print('  %-4s turns reuse %d%s: %2d turns; a turn: runs replayed %.0f (groups %.0f, skipping %.0f, in S %.0f), recorded %.0f (groups %.0f; '
                      'kept %.0f, not kept %.0f); kept boxes taken %.0f, cut %.0f, positions then needed %.0f; no table room %.0f; checked runs %d '
                      '(groups %d): S DIFFERING %d, boxes DIFFERING %d; replays with a group not taken %d' % (
                          kind, ru, ' (in vb %d-%d)' % (lo, hi) if inw else ' (all turns)', n, a('rr'), a('rrg'), a('rskip'), a('ins'), a('rec'), a('recg'),
                          a('kept'), a('nkept'), a('btak'), a('bcut'), a('bpos'), a('noroom'), sum(x['chk'] for x in k), sum(x['chkg'] for x in k),
                          sum(x['sbad'] for x in k), sum(x['bbad'] for x in k), sum(x['ntaken'] for x in k)))
    bad = [r for r in R if r.get('up') and (r['up']['sbad'] or r['up']['bbad'] or r['up']['ntaken'])]
    print('  turns where the reuse\'s check differs or a replayed run has a group not taken: %d%s' % (len(bad), '  <- REUSE PROBLEM' if bad else ''))
    for r in bad[:10]:
        print('    REUSE PROBLEM %d-%d %s: S differing %d, boxes differing %d, replays with a group not taken %d' % (
            r['a'], r['b'], r['kind'], r['up']['sbad'], r['up']['bbad'], r['up']['ntaken']))


def gl34_lines(L, R, lo, hi):
    """gl34: the fast front end's counts, the native path's split and the walks' time by chain"""
    print('\n== gl34: the fast native front end (MGS2_NAT_FAST / MGS2_NAT_FAST_CYCLE), the ME split (MGS2_NAT_SPLIT)')
    for l in L:
        if l.startswith('nat split (gl34') or l.startswith('fast native front end totals') or l.startswith('nat: gl34') or 'gl34 fast native front end' in l[:80]:
            print('  ' + l[:700])
    # the fast front end's counts per turn kind and arm
    for kind in ('me', 'main'):
        for fst in (0, 1):
            q = [r for r in R if r['kind'] == kind and r['fast'] == fst and r.get('fp') and not r['part']]
            if not q:
                continue
            n = len(q); f = [r['fp'] for r in q]; tk = sum(x['taken'] for x in f); fg = sum(x['flagged'] for x in f)
            dn = ['state', 'entry', 'buffers', 'stream counts', 'material tag', 'tables full', 'path 1 busy']
            print('  %-4s turns fast %d: %2d turns; a turn: groups flagged %.0f, taken %.0f (declined: %s), transforms kept from the last keyed call '
                  '%.0f, computed %.0f, pointers by hints %.0f (checked %d, DIFFERING %d)' % (
                      kind, fst, n, fg / n, tk / n, ', '.join('%s %.0f' % (dn[i], sum(x['dec'][i] for x in f) / n) for i in range(7)),
                      sum(x['xk'] for x in f) / n, sum(x['xc'] for x in f) / n, sum(x['hint'] for x in f) / n, sum(x['hchk'] for x in f),
                      sum(x['hbad'] for x in f)))
    bad = [r for r in R if r.get('fp') and r['fp']['hbad']]
    if bad:
        print('  HINT CHECK DIFFERS in %d turns  <- NAT PROBLEM' % len(bad))
    # the split
    spat = re.compile(r'nat split vb (\d+)-(\d+) me turn arm (\d+) fast (\d+) \(gl34; ticks a ms (\d+)\): groups in the timed merge runs (\d+), of '
                      r'them timed by stage (\d+) \(vertices (\d+), left out by the cut (\d+)\); clock read pairs (\d+), mean ([\d.]+) us a read; us '
                      r'a timed group by stage \(each holds one read\):(.*?); sum ([\d.]+); the turn\'s native time ([\d.]+) ms = ([\d.]+) us a '
                      r'group; hand-off: the ME\'s records ([\d.]+) ms, the plans\' bytes (\d+) KB, of them asked for by the groups\' data (\d+) KB, '
                      r'the groups\' array (\d+) KB')
    SP = []; names = []
    sl = [l for l in L if l.startswith('nat split vb ')]
    for l in sl:
        m = spat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        g = m.groups(); st = []
        for x in g[11].split(','):
            x = x.strip(); k = x.rfind(' '); st.append((x[:k], float(x[k + 1:])))
        if not names:
            names = [k for k, _ in st]
        SP.append(dict(a=int(g[0]), b=int(g[1]), arm=int(g[2]), fast=int(g[3]), keep=KEEP_OF.get((int(g[0]), int(g[1])), 0), ru=REUSE_OF.get((int(g[0]), int(g[1])), 0), ticks=int(g[4]), seen=int(g[5]), timed=int(g[6]), nv=int(g[7]),
                       ncut=int(g[8]), pairs=int(g[9]), read=float(g[10]), st=dict(st), sum=float(g[12]), nat=float(g[13]), natg=float(g[14]),
                       rec=float(g[15]), plan=int(g[16]), gdata=int(g[17]), garr=int(g[18])))
    cpat = re.compile(r'me chains vb (\d+)-(\d+) me turn arm (\d+) fast (\d+) \([^)]*\): lists read (\d+), transfers in a list (\d+), not in one '
                      r'(\d+); chain: transfers, ms, converter calls, native ms \(groups\):(.*); the ME\'s walks ([\d.]+) ms')
    CH = []
    cl = [l for l in L if l.startswith('me chains vb ')]
    for l in cl:
        m = cpat.match(l)
        if not m:
            print('  NOT PARSED: ' + l[:200]); continue
        g = m.groups(); c = {}
        for x in g[7].split(';'):
            x = x.strip()
            mm = re.match(r'(.*?): (\d+), ([\d.]+), (\d+), ([\d.]+) \((\d+)\)$', x)
            if mm:
                k = mm.group(1); k = 'ends' if k.startswith('the transfers') else 'other' if k.startswith('other') else k
                c[k] = (int(mm.group(2)), float(mm.group(3)), int(mm.group(4)), float(mm.group(5)), int(mm.group(6)))
        CH.append(dict(a=int(g[0]), b=int(g[1]), arm=int(g[2]), fast=int(g[3]), keep=KEEP_OF.get((int(g[0]), int(g[1])), 0), ru=REUSE_OF.get((int(g[0]), int(g[1])), 0), lists=int(g[4]), inl=int(g[5]), unl=int(g[6]), c=c, walks=float(g[8])))
    print('  nat split lines %d (parsed %d), me chains lines %d (parsed %d)' % (len(sl), len(SP), len(cl), len(CH)))
    if not SP and not CH:
        return
    W = [x for x in SP if x['a'] >= lo and x['b'] <= hi and x['b'] - x['a'] == 100]
    print('  the split per ME turn inside vb %d-%d (us a timed group; the clock: %s ticks a ms):' % (lo, hi, SP[0]['ticks'] if SP else '?'))
    print('    stages: ' + ', '.join('%d %s' % (i, k) for i, k in enumerate(names)))
    for x in W:
        print('    %5d-%-5d arm %d fast %d keep %2d reuse %d: groups %6d timed %4d (vertices %5d, cut %4d) read %.3f us | %s | sum %6.2f | native %8.2f ms = %6.2f us a '
              'group | records %6.1f ms, plans %6d KB, groups\' data %5d KB, array %4d KB' % (
                  x['a'], x['b'], x['arm'], x['fast'], x['keep'], x['ru'], x['seen'], x['timed'], x['nv'], x['ncut'], x['read'],
                  ' '.join('%5.2f' % x['st'].get(k, 0.0) for k in names), x['sum'], x['nat'], x['natg'], x['rec'], x['plan'], x['gdata'], x['garr']))
    M2 = {}
    for fst in sorted(set((x['fast'], x['keep'], x['ru']) for x in W)):
        q = [x for x in W if (x['fast'], x['keep'], x['ru']) == fst and x['arm'] == 3]
        if not q:
            continue
        n = len(q); tm = sum(x['timed'] for x in q)
        def wav(k):
            return sum(x['st'].get(k, 0.0) * x['timed'] for x in q) / tm if tm else 0.0
        d = dict(n=n, st={k: wav(k) for k in names}, nat=sum(x['nat'] for x in q) / n, natg=sum(x['nat'] for x in q) * 1000.0 / max(1, sum(x['seen'] for x in q)),
                 seen=sum(x['seen'] for x in q) / n, read=sum(x['read'] * x['pairs'] for x in q) / max(1, sum(x['pairs'] for x in q)),
                 rec=sum(x['rec'] for x in q) / n, plan=sum(x['plan'] for x in q) / n, gdata=sum(x['gdata'] for x in q) / n,
                 nvt=sum(x['nv'] for x in q) / max(1, tm))
        d['sum'] = sum(d['st'].values()); M2[fst] = d
        print('    mean, fast %d keep %d reuse %d, %2d ME turns: groups a turn %.0f; native %.2f ms a turn = %.2f us a group; timed groups: %.1f vertices each; clock read '
              '%.3f us; by stage (us a timed group): %s; sum %.2f; hand-off a turn: records %.1f ms, plans %.0f KB, the groups\' data %.0f KB' % (
                  fst[0], fst[1], fst[2], n, d['seen'], d['nat'], d['natg'], d['nvt'], d['read'], ', '.join('%s %.2f' % (k, d['st'][k]) for k in names), d['sum'],
                  d['rec'], d['plan'], d['gdata']))
    ks = sorted(M2)
    for kb in ks[1:]:
        a, b = M2[ks[0]], M2[kb]
        print('    (fast %d keep %d reuse %d) - (fast %d keep %d reuse %d) (us a timed group): %s; sum %+.2f; native us a group %+.2f (%+.1f %%); native ms a turn %+.2f' % (
            kb[0], kb[1], kb[2], ks[0][0], ks[0][1], ks[0][2], ', '.join('%s %+.2f' % (k, b['st'][k] - a['st'][k]) for k in names), b['sum'] - a['sum'], b['natg'] - a['natg'],
            100.0 * (b['natg'] - a['natg']) / a['natg'] if a['natg'] else 0.0, b['nat'] - a['nat']))
    if len(ks) > 1:
        print('    (the stage figures are sampled: one group in about 32, at changing places in the runs; each holds one clock read.  The native '
              'time is the whole merge runs\', timed at their two ends.  On the emulator the times are the host\'s: only a PSP log gives a speed.)')
    V = [x for x in CH if x['a'] >= lo and x['b'] <= hi and x['b'] - x['a'] == 100]
    keys = []
    for x in V:
        for k in x['c']:
            if k not in keys:
                keys.append(k)
    keys.sort(key=lambda k: (0, int(k)) if k.isdigit() else (1, 0 if k == 'ends' else 1))
    print('  the ME\'s walks by chain per ME turn inside vb %d-%d (ms a turn: converter calls; a chain = one transfer of the frame\'s list; ends = the '
          'transfers\' ends, where held calls are converted; other = transfers not in a list):' % (lo, hi))
    for x in V:
        print('    %5d-%-5d arm %d fast %d keep %2d reuse %d: lists %3d, transfers in a list %4d, not %4d | %s | walks %8.1f ms' % (
            x['a'], x['b'], x['arm'], x['fast'], x['keep'], x['ru'], x['lists'], x['inl'], x['unl'],
            ' '.join('%s %.1f:%d' % (k, x['c'][k][1], x['c'][k][2]) for k in keys if k in x['c'] and (x['c'][k][0] or x['c'][k][1])), x['walks']))
    C2 = {}
    for fst in sorted(set((x['fast'], x['keep'], x['ru']) for x in V)):
        q = [x for x in V if (x['fast'], x['keep'], x['ru']) == fst and x['arm'] == 3]
        if not q:
            continue
        n = len(q); d = {k: (sum(x['c'].get(k, (0, 0, 0, 0, 0))[1] for x in q) / n, sum(x['c'].get(k, (0, 0, 0, 0, 0))[2] for x in q) / n,
                             sum(x['c'].get(k, (0, 0, 0, 0, 0))[3] for x in q) / n, sum(x['c'].get(k, (0, 0, 0, 0, 0))[4] for x in q) / n) for k in keys}
        C2[fst] = d; wk = sum(x['walks'] for x in q) / n
        print('    mean, fast %d keep %d reuse %d, %2d ME turns (ms a turn, calls; native ms and groups where any): %s; walks %.1f ms' % (
            fst[0], fst[1], fst[2], n, '; '.join('%s: %.1f, %.0f%s' % (k, d[k][0], d[k][1], (' (native %.1f, %.0f)' % (d[k][2], d[k][3])) if d[k][3] else '')
                             for k in keys if d[k][0] or d[k][1]), wk))
    cs = sorted(C2)
    for kb in cs[1:]:
        a, b = C2[cs[0]], C2[kb]
        print('    (fast %d keep %d reuse %d) - (fast %d keep %d reuse %d) (ms a turn): %s' % (kb[0], kb[1], kb[2], cs[0][0], cs[0][1], cs[0][2],
              '; '.join('%s %+.1f' % (k, b[k][0] - a[k][0]) for k in keys if a[k][0] or b[k][0])))


def main():
    o = args(sys.argv[1:])
    L = open(o['log'], errors='replace').read().split('\n')
    lo, hi = o['from'], o['to']
    print('== rd36: %s (md5 %s, %d lines); window vb %d..%d' % (o['log'], md5(o['log']), len(L), lo, hi))
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
        if l.startswith('nat: cull study') or l.startswith('natcull: totals'):   # gl36 retrain: the cull's key echo and totals
            print('  ' + l[:900])

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
    nat_lines(L, o['natfrom'], o['natto'])

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
