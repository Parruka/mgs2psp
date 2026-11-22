sb15 (task #152, Stage C step 4: the effects the PSP leaves out: sprites, quads, streaks; task #153, 4x; stagec_notes.md "Step 4").

Tree /home/claude/sb15 (ephemeral) = /home/claude/sb14 (tools/dev/sb14) + sb15.diff (ge/vu1conv.c, ge/vu1conv.h: vdiv, vc_spr,
vc_quad, vc_line, seg_plane, vc_clip_seg; VC_SPR 0x19bb50, VC_QUAD 0x19be50, VC_LINE 0x19af68).  Apply in sb14's copy: patch -p1 < sb15.diff.
gsref_sb15.diff applies to the project's tools/gsref.c (md5 0a80d503):
- GSREF_SKIPENT=<entry hex>[,...]: those VU1 entries' primitives are not drawn (the census); "entry %06x px" counts per entry (PENT).
- GSREF_DF0CHK=1 also runs spchk (P04+3740 against vc_spr), qdchk (P04+3a38 against vc_quad and vc_clip_tri) and lnchk (P04+2b60
  against vc_line and vc_clip_seg; the clipped ends also by the GE route, world positions through the call's matrix); stderr lines
  "spchk", "qdchk", "lnchk" (counts, then the largest differences with their vblank).
Build (from sb15/tools): gcc -O2 -ffp-contract=off -w -o gsref gsref.c ../ge/texconv.c ../ge/vu1conv.c ../ge/xlate.c ../ge/xconv.c
  ../ge/gsfront.c ../ge/gebatch.c ../ge/gsup2.c ../ge/xldig.c ../ge/c1model.c ../ge/c2model.c ../ge/c3model.c ../ge/texcache.c
  ../ge/fxthin.c -lm
Captures (game data: they stay in the container, /home/claude/c4cap): cap4.sh d dk 24050 8; cap4.sh i ad 30800 8; cap4.sh i in 33410 8
  (sb14/x1's x86 twin, gl16's tree; MGS2_GSCAP).  Then: GSREF_DF0CHK=1 ./gsref <tag>.cap <out dir> > chk.txt 2> chk.err.
Census: skipall.sh <tag> <vb list> (each undrawn entry left out in turn, imdiff.py: pixels whose largest channel moves by more than
  8 levels); pxent.py <gsref stdout> (pixels each entry writes per vblank).  ppm2png.py: gsref's frames to PNG (they stay in the container
  or go to Ffm's PC folder only).
Results 2026-11-03: stagec_notes.md 4.2 (census), 4.4 (checks).

Later the same day (2026-11-03; stagec_notes.md 4.5-4.9):
- sb15.diff now holds the whole tree's change from sb14 (13 files: ge/gelist.c .h, gelive.c .h, vu1conv.c .h, xconv.c .h, xlate.c,
  boot/xlive_chk.c, psp/main_psp.c, psp/xl_psp.c, runtime/host_time.h); the md5s after it: xconv.c 4dde0997, xconv.h 525b266f,
  vu1conv.c 66330345, vu1conv.h 63825614, gelist.c f5cde573, gelive.c a78364a6, host_time.h 8807446a, xl_psp.c 08e748c5, main_psp.c
  0a71ad55, xlive_chk.c 8be7abd0.  gsref_sb15.diff likewise (tools/gsref.c 8da27fd4).
- The live path: g_opt2 OPT2_C4 512 (the effects drawn) and OPT2_C4X 1024 (4x: +25f0 / 2870's strips and polygons from their clip
  positions, the GE's world the identity; an untextured call's stale UV q no longer refused), both off by default: OPT2_DEFAULT
  fffff9df; C4 only fffffbdf (PSP cfg index 32), C4X only fffffddf (33), both ffffffdf (34).  Step 4's quads and streaks use clip
  positions always.  The four effect converters share one work area (ge/xconv.c Fx): step 4 adds no static memory.
- Twin: sh /home/claude/sb15/mk.sh <dir> (x86, GU=lean).  chk4x.sh: the x86 checks (routes d and i, MGS2_GX_SEM=1 drawn digests).
- PSP model build: HT="<title>" sh mkpsp.sh <new dir> (no PGO; sb15m/d before 4x, sb15m2/d after).  c4master.sh <tag> <MGS2_OPT2>:
  the PPSSPP master on the indoor route with gl16's settings and shots (B=<build dir>); masters4x.sh: the three 4x runs.
  models_c4.sh: PspProf windows (sb10/model.sh), then tools/dev/sb6/fitcmp.py <off> <on> 80 <build dir>.
- gsref switches added: GSREF_GEPREC=1 (stderr "geprec": each effect vertex through four GE routes, today's 24-bit matrix, a full-float
  matrix, recentred on the call's first vertex, the clip position; "gpbig": vertices over 1 px), GSREF_FPBAD=1 (stdout "fpbad": each
  refused +25f0 / 2870 call), GSREF_SKIPUNTEX=<entry hex> (that entry's untextured triangles not drawn), GSREF_FPDBG=1 (stdout: a fan
  whose vertex count differs from the PS2's).  fpchk and lnchk compare UVs only for textured calls.

Census and pictures (2026-11-03 12:00; stagec_notes.md 4.6, 4.9):
- MGS2_FX_WHO=1 in an x86 twin (x86 only: ge/fxthin.c fx_who_note under #ifndef __psp__, called from gelive.c's gl_call; the PSP
  builds do not have it): at the end, "fxwho:" lines, per kind (sprites +3740, quads +3a38, streaks +2b60, fans +25f0, strips +2870)
  the calls, then per owner (the actor's file name and where in the actor the block's pointer sits) the calls of each kind.
  whoagg.py <log>...: the owners summed by file name.  sb15.diff now 15 files (+ ge/fxthin.c e83796e1, ge/fxthin.h cc646642; gelive.c
  now 0b97fdc2).
- mcmp.sh <tag> [<ref>, default gl16]: two PPSSPP master runs compared (hash and vuh lines, lists and digests, heap, the largest
  frame, each shot equal or not).  masters_op.sh: the opening's masters (o4d / o4x, to vb 9,000).
- pairshots.py <A dir> <B dir> <out dir> <name A> <name B> <vb>...: two runs' shots side by side for Ffm's PC folder (each picture,
  where they differ, an x4 zoom of the box with the most difference) and each shot's figures.  Game pictures: Ffm's PC folder only.

The census corrected and the rain's streaks at 1/2 (2026-11-03 12:20-13:05; stagec_notes.md 4.9, 4.11; task #155):
- The census reads each actor's own block (to its allocator's next header; 0x400 bytes when the header does not look right), not
  0x1000 bytes from it: the first census could count a block for the actor before its owner (wind_man.c for the streaks).
  MGS2_FX_WHO=3 also counts the actors that point at each block ("fxwhosh:" lines; none has more than one).  whoagg.py -a: every
  owner; "rows not read": calls in rows whose line the log's interleaved output broke.  fx_who_name() gives a call's owner (x86).
- MGS2_GL_STREAKS=<n> (PSP cfg and x86 environment; 1 or left out: whole): with C4 on, rain.c's and rain_slow.c's streak calls draw
  1 strip in n (fxthin kind 2: the actor's table from +0xb0, 0x20-byte entries, the block's pointer at +0x10, as many entries as the
  actor's block holds, at most 64; the blocks joined into one line range an actor; xconv.c xc_line sets the other strips' classes to
  0).  MGS2_THIN_CHK=1 also checks the streaks (xlive_chk.c thin_chk_l; "thin check streaks:" lines, the owners by fx_who_name).
- sb15.diff (15 files) now 5c5f3058; md5s after it: ge/fxthin.c 0cf2e355, fxthin.h f6eed2d1, xconv.c ac793810, xconv.h 0e78b2ac,
  gelive.c 0d752682, boot/xlive_chk.c 9a67e3c9, psp/xl_psp.c 9b814fc6 (the rest as above).  whoagg.py cfc186b1.
