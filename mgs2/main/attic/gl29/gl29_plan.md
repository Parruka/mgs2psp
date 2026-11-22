# gl29 plan: the black flickering marks on the aft deck (2026-11-20)

Marks: [C] means read in code or a log, or measured by a script, with the place named. [C model] means computed from the GER float model. [I] means inferred, with the basis named. UNKNOWN means no basis yet, with the way to find out.

## 0. In short

- **What the marks are.** On the real PSP the GE drops parts of the deck's shadow decals. These are blend 0x42 (Cd - Cs*As), colour-only second pass, depth test GEQUAL. Which parts are dropped changes from frame to frame during the walk, so the shadows switch on and off, and in motion that reads as black flickering marks [C emulator frames report: 0 PSP-only dark areas in 16 frames, PSP-lighter shadow-shaped patches in 10 of them; I for the perception].
- **The input is right.** The GE gets the right list: the digests equal the twin's in 201 of 201 frames [C rd28.out]. The drop repeats bit for bit across builds: shot_30500 and shot_29500 are identical in gl18me, gl26 and gl28 [C H1 verdict, emulator frames report]. So the fault is in how the real GE computes depth. It is not a memory or timing problem [C+I, section 1].
- **New today: the drop does not follow the lead.** Decals with a lead of only 1.2-4 GE units over small triangles survive. Decals with a lead of 4-31 GE units over huge, steep triangles lose 17-51% of their pixels. The data fit "the GE's per-pixel z error is about 1e-3 times the triangle's z span, plus about 0.3 GE" across 17 decals in 4 frames [C model p29/tri2.py; I for the rule].
- **gl29 is one run, with no fix on by default.**
  - A depth "arm" switch (MGS2_GL_DZ) chosen per frame by vblank. The arms are: off; the exact fix (X: each decal drawn from its base's own vertex data, so its z equals the base's bit for bit); a bias ladder (B1-B8: the viewport z offset word plus n steps); and two diagnostics (W, Y).
  - A Z-buffer dump at the drained shots (MGS2_GL_ZDUMP).
  - R-press accounting by arm.
  - In the static rain wait, drained shot pairs measure each arm without the owner. During the walk, 100-vb windows rotate off, X and B2, and the owner's R presses score them.
- **Expected result if the cause is right.** R is held only in the "off" windows. Live captures in the X and B windows match the emulator. gl30 then makes X the default fix, once the owner has OKed the shots.

## 1. Cause ranking after verification

| # | Cause | Likelihood | Main evidence |
|---|---|---|---|
| 1 | Real-GE depth test drops the 0x42 decal's colour-only pass where the GE's per-pixel z error exceeds the decal's lead over its base | high | below |
| 2 | H2: the depth map jumps between frames | refuted as the cause; minor amplifier on coarse frames at most | H2 verdict |
| 3 | H3: CLUT4 texture or CLUT sampling | low | 30518 vs 30500; arm W tests it |
| 4 | H4: depth blur reads the frame being drawn | low | cannot remove a decal wholesale |
| 5 | H7: TSIZE larger than the stored texture (overread) | low | UVs inside the stored area at 30500 |
| 6 | H5/H6: memory lifetime or cache | very low | serialized main turns; identical across builds |
| 7 | H8: flip race or FB alpha | excluded / irrelevant | fbchk 0/0 |

### 1.1 Cause 1: the evidence

**Only the depth test can drop these pixels.**
- Pass 0 tests alpha with ATMAP and pass 1 with ATINV, its exact complement [C t28/ge/gelist.c:43-44, 509]. So every fragment passes the alpha test in exactly one of the two passes.
- Pass 1 has the depth test on (GEQUAL) and Z write off [C gelist.c:45, 512-516].
- The decal textures' alphas are all below the reference (tex 79 palette alpha 0..40, aref 0x40 → GE 0x80) [C p29/pal.py on tw/ger/f30500.ger; gelist.c:509]. So every decal pixel comes from pass 1.
- The scissor is full-screen and no decal triangle is near the z range or the guard band [C H1 verdict, gerana].
- So a missing decal pixel is a failed depth test. Cs*As = 0 is ruled out by 30500 vs 30518 (same texture, near-identical camera, 30518 clean) [C H1 verdict].

**The shadow is absent, not weaker.**
- Measured over expected darkening is 0.64 at missing pixels, which is the fog factor, and about 0 elsewhere [C H1 verdict].
- At 30500 the PSP is lighter by more than 6 luma in 34,099 px. b808-b812 account for 33,838 of them (51% of b808, 42% of b809, 17% of b810, 30% of b811, 30% of b812) [C p29/cov.py: shot_30500 against emu_shots/shot_30500].

**No other surface sits between the base and the decal at 30500.**
- For b808/b809, the only depth writer close below is the decal's own base: decal minus base is 15-28 GE units (p5-p95) [C model p29/zw.py].
- Every other overlapping depth writer is 80 GE units or more below at its 5th percentile, mostly thousands below. The few in front (b697/b698) correctly hide the decal.
- The layer called L1 (b754, tex 60) is about 12,000 GE units below.
- So "another Z writer" is excluded for this frame [C model].

**The pairs are exact copies.**
- Each decal is its base's triangle set moved +5 s16 in model y.
- World matrix, projection and viewport words are equal in every pair.
- The vertex count and strip runs differ for some pairs: b808/b792 have 12/16 vertices and 2/4 strips, b809/b793 9/11, b811/b795 6/8.
- [C p29/pairchk.py: 5 pairs at 30500, 1 at 31046; H1 verdict tri.py.]

**The loss follows triangle z span, not lead** [C model p29/tri2.py, p29/cov.py; leads are at the vertices, after the frame's map].

| frame | decal | lead (GE) | largest triangle z span (GE) | coverage lost on the PSP |
|---|---|---|---|---|
| 30500 | b797, b800, b801, b803 (tex 75) | 1.2-1.7 | 462-854 | 0-1% |
| 30500 | b805, b806 (tex 78) | 2.4-4.1 | 1,987-2,082 | 2-5% |
| 30500 | b810 (tex 79) | 3.7-6.5 | 6,226 | 17% |
| 30500 | b812 (tex 79) | 10.4-30.8 | 22,698 | 30% |
| 30846 | b791-b795 (tex 100/101) | 1.0-4.8 | 315-1,498 | 0-1% |
| 31046 | b773 | 0.30-0.32 | 5 | 5% (1,805 px) |
| 31046 | b776 | 0.32 | 3 | 0% |
| 31112 | b788 | 0.37-3.03 | 848 | 0.5% |

- Every row fits "loss where the lead is below about 1e-3 × span + 0.3 GE". For example, b805 needs 2.3 against a lead of 2.4 or more and loses 2%; b812 needs about 23 against 10-31 and loses 30%; b773 needs about 0.3 against 0.31 and loses 5% [C model numbers; I for the rule].
- Caveat: the tex 75/78 palettes have alpha 6..38, comparable to tex 79's 0..40 [C p29/pal.py]. But their texel distribution, and so their visible darkening, is not checked [I]. Check E8 closes this.
- Consequences [I from the table]:
  - The thin-lead theory alone (H1 as written) is wrong.
  - The ratio lead/span does not depend on the map, so H2's map jumps cannot be the driver. The absolute floor (about 0.3 GE) only matters on coarse frames, which is why 31046 loses 5%.
  - PPSSPP interpolates z per pixel in float from integer vertex z [C per code hunt: ppsspp-1.20.4/GPU/Software/Rasterizer.cpp:1024-1086], so its error does not grow with span [I].

**Static during the wait, moving during the walk.**
- The hole masks of 29500 and 30500 overlap with IoU 0.89, although the camera sways and S changes by 0.6% [C H1 verdict].
- Within a triangle a straight line separates missing from present pixels in 10 of 13 triangles [C H1 verdict lin.py].
- This fits an error that is a stable function of each triangle's setup and of the pixel's place in it [I]. Per-vertex random rounding would reshuffle every frame [C H1 verdict].
- The exact GE rule is UNKNOWN. The Z dump (G2) and arm Y (G1) measure it.

**Every turn type, both GE2 states, independent of timing.**
- shot_30500 is a main-turn frame (main turns are fully serialized [C t28/psp/xl_psp.c:2161; ge/xme.c:116-123]), and its list is written back before submit [C xl_psp.c:1545].
- PSP 30500 is bit-identical across gl18me, gl26 and gl28 [C].
- Live patches appear in main turns and ME turns, GE2 on and off [C emulator frames report].

**Not explained yet.**
- 30518 is clean although its camera is close to 30500's [C H1 verdict]. Under the span rule this needs the hole pattern to be sensitive to small camera moves outside the wait's sway [I]. UNKNOWN.
- Onset: the drop exists in gl18me, but the owner first noticed marks in "gl26, probably gl25" [C night synthesis; owner]. UNKNOWN. The R-by-arm table in this run settles whether the owner's marks are this drop.

### 1.2 The other hazards

**H2, map jumps: refuted as the cause.**
- No map jump in R windows R1 (30521-30720) or R6. The map sequence is identical since gl19 [C H2 verdict, h2/map22b.txt].
- The 16.77M writer is near-plane geometry, not far or sky depth [C H2 verdict], so a "clamp to GE 0" fix would be backwards [C H2 verdict].
- What remains: on coarse frames (30838-31068) the absolute floor of about 0.3 GE becomes comparable to the leads (0.31 at 31046) [C model]. That is a small amplifier, not the source.

**H3, texture or CLUT sampling: low.**
- 30518 clean vs 30500 with the same texture [C H1 verdict].
- UVs stay inside the stored 416×200 [C code hunt].
- Only 198 PSP-darker px at 30500 [C H1 verdict].
- Arm W gives a direct test: with the depth test forced to always pass, PSP and emulator colour inside the decal coverage must match.

**H4, depth blur: low.**
- Batch 877 is drawn after the decals [C GER order], and the decal holes are full absence [C model]. The blur cannot remove a decal wholesale.

**H5/H6, memory lifetime and cache: very low.**
- Main turns are serialized and written back before submit [C xl_psp.c:1545, :2161].
- The results are deterministic across three builds [C].

**H7, TSIZE overread: low.**
- Not at 30500: tex-79 UVs stay inside the stored area [C code hunt, texs.py]. The tex 3 decals are not checked [I].

**H8, flip race / FB alpha: excluded or irrelevant.**
- fbchk: main turns IMMEDIATE 0, NEXTFRAME 0 [C rd28.out].
- FB alpha does not touch the decals [C model].

**PSP-darker 1-px edges.**
- These are on handrails and rails [C emulator frames report]. They come from rasterization differences on thin geometry and are not the marks [I from the crops].

## 2. The gl29 changes

Every change is behind its own cfg key. With the keys absent, gl29 draws exactly what gl28 draws: same list words, same digests. Gate E1 proves this.

### G1. MGS2_GL_DZ: per-frame depth arms for the 0x42 decals

**Files**
- t29/ge/gelist.c: gx_batch :477-527, xf_rest :148-163 and xf_draw :166-180; a new shared parser.
- t29/ge/gelist.h: new fields appended at the end of GxList; GxCount unchanged.
- t29/ge/xme.h:59: XmeHook gets `dz` and `dzn`, appended.
- t29/ge/xme.c:107: copy h->dz/h->dzn into p->gxl before xm_list.
- t29/psp/xl_psp.c: :2205 sets e.h.dz from the schedule for frame vb-1; gl_list :356-385 sets gxl's fields.
- main copy, boot/xlive_chk.c:232-234: the twin sets the same fields from getenv("MGS2_GL_DZ") with the same parser.

**Grammar:** `MGS2_GL_DZ=<vb0>:<vb1>:<arm>[,...]`, at most 48 entries. A frame whose last vblank (fvb, the same key as the digest and shot names [C xl_psp.c:2205, xme.c:110]) is inside [vb0, vb1] gets that arm; every other frame gets O.

**Which batches it touches:** only 0x42 batches with the depth test on (`(b->flags & GERF_ZTEST)` and blend sig&0xff == 0x42 [C gelist.c:96-102, 512]) of kind GERK_XFORM. That is every deck decal seen so far [C p29/dq.py, 4 frames]. The log counts any 0x42 batch of another kind.

**The arms**
- **O**: nothing changes.
- **X, the exact fix.**
  - Pairing: for a 0x42 XFORM batch, look back up to 64 batches for a Z-writing XFORM batch with:
    - the same world, projection and viewport floats;
    - the same set of distinct vertex positions after one constant s16 offset (|offset| ≤ 64; (0,5,0) in all 6 pairs checked);
    - the same triangle set.
  - Draw: the decal is drawn with the base's own vertex array and strip runs, each base vertex carrying the decal's uv and colour from the matching decal vertex. The vertex copy goes in the list's aux (aux_get). Both passes as before.
  - Result: the GE receives the base's exact vertex data, so the decal's z equals the base's at every pixel, whatever the GE's internal rule [I: the GE is deterministic], and GEQUAL passes exactly where the base is on top.
  - Fallbacks, each counted by reason and leaving the batch as O: no base, matrix differs, vertex set differs, triangle set differs, two decal vertices on one position with different uv or colour, no aux room.
  - Picture effect: the shadow moves by under 1 px, because the decal triangles are the base's within 1 px [C code hunt]. Gate E5 measures this.
- **Y, a diagnostic.** Like X, but with the decal's own strip runs and only the positions replaced. If Y clears the holes in b812 and b810 (same vertex order as their bases) but not in b808, b809 and b811 (different strips), the GE's z depends on vertex order within a triangle [I]. This tells gl30 whether a simpler variant of X is safe.
- **B<n>, a bias ladder.**
  - Both passes send the frame's VPXS+5 word (viewport z offset, gelist.c:154) with n added to its 24-bit field: n units in the last place, nearer since DepthRange is 65535..0 [C xl_psp.c:311 sceGuDepthRange(65535, 0); gelist.c:45 GEQUAL].
  - Why steps rather than a float: putf keeps 15 fraction bits [C gelist.c:53]. At 30500 the offset is about 2.675e6 [C H1 verdict], so one step is 64 GE ≈ 200 PS2 Z units. At 31046 the offset is about 30,854, so one step is 0.5 GE ≈ 136 PS2 units [C arithmetic from zs and vo2 in the H1 and H2 verdicts]. A float bias of κ·zs smaller than one step would vanish in the truncation [C gelist.c:53]. A step is roughly constant in PS2 units across maps [C arithmetic].
  - The biased float is stored back in s_vp so the cache stays right [C gelist.c:155].
- **W, a diagnostic.** For these batches, ZFUNC ALWAYS and Z write on in both passes. The colour shows the whole decal on the GE (the H3 test against the emulator), and the Z buffer then holds the decal's own z (for the Z dump).

**Cost**
- Code: about 200 lines in gelist.c (parser 40, arms 30, X/Y about 130), 15 in gelist.h, 4 in xme.h/xme.c, 20 in xl_psp.c, 15 in xlive_chk.c [I estimates].
- CPU on O frames: one test per batch, about 0 [I].
- CPU on X/Y frames: pairing over about 180 0x42 batches a deck frame, bounded by 64 look-backs with a cheap nv/translation reject first. UNKNOWN, expected under 0.5 ms a frame [I]. The per-window list time (G4) measures it on the PSP.
- List words for B: 6 VPXS words re-sent on entering and leaving a biased batch [C xf_rest's cache, gelist.c:155].
- Memory: no heap, which is nearly full (7,989 of 8,016 KB in use [C runs/gl28/log.txt:311]). X/Y copies come from the list's aux: frame 30500 used 140 KB of 352 KB [C log.txt:2566]. The run's largest list was 342,536 of 360,448 bytes [C log.txt:3025], but in another scene, and X runs only in deck windows.

**Code safety**
- gelist.c runs on the ME in ME turns [C gelist.c:10 ME-safe build; xlme.c:357].
- No new libc calls beyond the memcmp and memset gelist already uses. Scratch space comes from aux, not statics.

### G2. MGS2_GL_ZDUMP=<from>:<views>[:<mirror_vb>]: the Z buffer at drained shots

**File:** t29/psp/xl_psp.c gl_shot :411-417.

**What it writes.** After the BMP, for shots at vb ≥ from, it writes zbuf_<fvb>.raw: the whole depth area, 512 × 272 u16 (278,528 bytes), read from 0x44000000+GL_ZB [C xl_psp.c:172] through the uncached view, as gl_bmp reads colour [C :389-409]. It uses gl_rows as its buffer, so no new memory.

**The mirror view.**
- At the one shot vb == mirror_vb, it also writes zbm_<fvb>.raw from the |0x600000 mirror (0x44600000+GL_ZB).
- Why: on hardware the depth buffer's byte order seen from the plain view may be swizzled. PPSSPP treats |0x600000 addresses as the "auto-swizzling" depth view [C ppsspp-1.20.4 GPU/Common/FramebufferManagerCommon.cpp:2263, TextureCacheCommon.cpp:1791-1799].
- The layout is UNKNOWN. The reader finds it from the pair of views at mirror_vb against the emulator's linear Z (SoftGpu keeps Z in VRAM at ZBP [C per H1 verdict]).
- CPU reads of the mirror are believed harmless [I]. It is done only at the run's last shot so a fault could cost nothing but the fade.

**Matching check.** In the drained state the last list drawn is the frame saved [C xl_psp.c:1593-1606 shot_wait]. gl_shot logs "last list = shot frame: yes/no" (gl_ge 0 and gl_ge_vb == gl_shown_vb).

**Cost:** about 10 ms of VRAM read [I from 18.3 ms for 522 KB of colour, C log.txt:2579] plus about 150 ms of writing [I] per shot.

### G3. Shots

**File:** t29/psp/xl_psp.c.
- The shot list grows from 16 to 48 entries [C shot_vb[16] at :189; the parse loop's shot_n < 16 at :289].
- MGS2_GL_SHOT_RANGE=<from>:<to>:<step> saves every step-th frame as a BMP with no Z dump. It is for the emulator's dense reference only, not set on the PSP.

### G4. Logging for the decision

**File:** t29/psp/xl_psp.c.
- **Digest lines:** each frame inside MGS2_GL_DIG gets the arm and the frame's counts appended: `dz X 42:<n> pair:<n> fb:<base,mat,vset,tri,attr,room> bias:<n> aux:<bytes>`.
- **Window lines:** at the end of each DZ entry and each O stretch of the walk, one line: arm; frames built by turn type; totals of the counts above; list-build ms (sum and largest, by turn type); vb with R held; and the same excluding the first 10 vb of the window.
- **eye lines** [C :1609-1628] and **live lines** [C :1630-1656] gain `dz <arm>` of the shown or captured frame (gl_buf_vb → schedule).
- **Shot lines** gain the arm, the zdump file names, the VRAM read ms and the "last list = shot frame" flag.
- **R by arm:** eye_poll already peeks R at every vblank hook [C :1609-1615]. It also adds 1 to `eye_held[arm][turn]` while R is down, the arm being the shown frame's.
- **End of run** (beside :2588): a table of R-held vb by arm × turn type, and by window.

### G5. The twin

**File:** main copy, boot/xlive_chk.c.
- It reads MGS2_GL_DZ from the environment with the same parser as the PSP, as it already does for MGS2_GL_RAIN and others [C xlive_chk.c:1186-1191], and sets the fields before gx_frame [C :232-234].
- It prints the arm in its digest lines.
- Everything the arms do is a pure function of the frame, so the twin's digests equal the PSP's for every arm [I; gate E2].

### G6. Build recipe

**Make the sources**
- t29 = cp -a t28, then gl29_apply.py with exact string replacements and md5 gates, as gl28_apply.py did [C l28/gl28_apply.py header]. The main copy gets boot/xlive_chk.c and cfg/mgs2_gl29.cfg.
- Nothing is edited in t28, main or any build folder.

**relink29.sh** = relink28.sh plus:
- recompile ge_gelist.o (a gl23 stage object that relink28.sh does not rebuild [C relink28.sh:14-17; build_gl28/stage]) with its logged line (main/build_gl20/u2/cc_ge_gelist.o.log) and gl20 gcda3 profile;
- recompile every other stage object whose source includes gelist.h or xme.h: ge_gelive, ge_xconv, ge_xlme, ge_xme, ge_xme_me, xl_psp [C grep of t28 includes].

**Gates**
- psp.sh's ME gates: tools/me_call_chk.py and tools/xme_rest_chk.py [C relink28.sh comment].
- -Wcoverage-mismatch warnings noted. A speed change from the profile is possible [I]; the per-window list time shows it.

## 3. The run (cfg)

**The cfg.** mgs2_gl29.cfg = mgs2_gl28.cfg [C main/cfg/mgs2_gl28.cfg] with these lines changed or added. Everything else stays as in gl28: TURN 100, TWO 1, GE2 1, GE2_AB 30500, GEFAST 1, FLIPWAIT 2 from 30500, SHOT_FLIPWAIT 1, EYE 1, FBCHK 30500, ME_REST 1, BAT_REC 1, vblanks 31400, 333 MHz.

```
MGS2_GL_DIG=29490:31400
MGS2_GL_LIVE=30500:31400:25:8
MGS2_GL_ZDUMP=29000:1:31264
MGS2_GL_SHOT=8762,10518,15763,23265,24246,24786,29502,29552,29554,29652,29654,29752,29754,29852,29854,29952,29954,30052,30054,30152,30154,30252,30254,30352,30354,30452,30454,30502,30548,30646,30724,30826,30926,31048,31114,31264
MGS2_GL_DZ=29552:29553:W,29652:29653:X,29752:29753:Y,29852:29853:B1,29952:29953:B2,30052:30053:B4,30152:30153:B8,30252:30253:Y,30352:30353:X,30452:30453:W,30550:30649:X,30650:30749:B2,30850:30949:X,30950:31049:B2,31150:31249:X,31250:31349:B2
```

### 3.1 The rain wait (29006-30500): drained pairs, no owner needed

The holes are static here [C H1 verdict: IoU 0.89, 29500 vs 30500], so a neighbouring frame two vblanks earlier is the arm's control.
- Each pair: the shot at xx52 saves the O frame xx50 and the shot at xx54 saves the arm frame xx52. Both carry Z dumps.
- The arm window [xx52, xx53] catches exactly one frame on either vblank parity [I: shots save the frame two vblanks back, C gl28 shot names].
- The pairs sit 50 vb inside a turn, avoiding the boundary problem C7 of the night's critique [C marks_crit.md C7].

| block | turn [C turn rule] | O frame | arm frame | arm |
|---|---|---|---|---|
| 295 | main | 29550 | 29552 | W |
| 296 | ME | 29650 | 29652 | X |
| 297 | main | 29750 | 29752 | Y |
| 298 | ME | 29850 | 29852 | B1 |
| 299 | main | 29950 | 29952 | B2 |
| 300 | ME | 30050 | 30052 | B4 |
| 301 | main | 30150 | 30152 | B8 |
| 302 | ME | 30250 | 30252 | Y |
| 303 | main | 30350 | 30352 | X |
| 304 | ME | 30450 | 30452 | W |

- Regression shots 29502 (frame 29500) and 30502 (frame 30500) are O and must equal gl28's shot_29500 and shot_30500 bit for bit.
- These are the first drained deck shots in ME turns on the PSP [C night synthesis UNKNOWN list]. E3 runs them on the emulator first.

### 3.2 The walk (30500-31400): 100-vb windows, offset 50 from the turn grid

Each window has 50 vb of a main turn and 50 of an ME turn, so arm and turn type are not confounded.

| window (frame vb) | arm | turns | gl28 R held [C rd28.out eye lines] | gl28 live loss px [C emulator frames report] |
|---|---|---|---|---|
| 30500-30549 | O | main | 29 of 50 (30521-) | 30546: 5,904 |
| 30550-30649 | X | main + ME (GE2) | 100 of 100 | 30644: 10,976 |
| 30650-30749 | B2 | ME + main | 71 of 100 | 30722: 14,976; 30746: 8,768 |
| 30750-30849 | O | main + ME | 73 of 100 | 30806: 0; 30846: 752 |
| 30850-30949 | X | ME + main | 100 of 100 | 30946: 176 |
| 30950-31049 | B2 | main + ME (GE2) | 81 of 100 | 30996: 960; 31044: 2,192; 31046: 2,528 |
| 31050-31149 | O | ME + main | 30 of 100 | 31112, 31146: 0 |
| 31150-31249 | X | main + ME | 0 | 31246: 0 |
| 31250-31349 | B2 | ME + main | 0 | (fade from about 31,300) |
| 31350-31400 | O | main | 0 | black fade |

- The game frames repeat exactly run to run [C digests equal the twin], so this table is the baseline per window.
- At about 7-9 vb a second [C eye t values: 30521→30720 in 28.2 s, 30810→31080 in 30.9 s], a window lasts 11-14 s, long enough to see.
- Drained walk shots, all with Z dumps:
  - frame 30546 (O), 30644 (X), 30722 (B2), 30824 (O), 30924 (X), 31046 (B2), 31262 (B2, with the mirror view);
  - plus 31112 (O), which must equal gl28's shot_31112.
- Live captures every 25 vb give two per half-window: 36 periodic, plus up to 8 at R presses.
- Why B2 for the walk:
  - Under the span rule the worst deck triangle at 30500 needs about 13 GE beyond its lead, and coarse frames need about 0.3 GE [C model table, I rule]. B2 gives 128 GE fine and 1 GE coarse.
  - If gate E5 finds visible bleed at B2, the walk uses B1 (64 / 0.5 GE).

### 3.3 What to tell the owner

"gl29 is gl28 plus test switches; nothing to set up. As last time: on the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. While the rain wait runs (before Snake walks) the picture pauses about 21 times, under a second each, and a few single frames may look darker or lighter. Ignore those. During the walk it pauses 9 times and freezes for a quarter second about every 3 seconds (captures). Logs come back as usual in LOGS/gl29."

## 4. What the run must log, and how R decides

**Logged:** G4's lines, plus everything gl28 logs: hashes, digest summary, fbchk counts, eye lines, live lines, battery recorder, ME rest.

**Predictions**

| outcome in the X and B2 windows | R held | live captures vs emulator | meaning |
|---|---|---|---|
| fixed | about 0 vb after each window's first 10 vb | lighter-patch px about 0 | cause 1 confirmed; O windows keep R (gl28: 29, 73, 30 vb) and their patches |
| not fixed | like gl28 (71-100 of 100) | patches like gl28 | X/B do not reach the GE's error: see the decision list |
| objective clean, R still held | high | about 0 | the owner's marks are something else |

**Decision list after the run**
1. **Preconditions.**
   - Digests equal the twin with the same cfg for every frame 29490-31400, arms included (E2): the PSP received exactly the intended lists.
   - Regression shots 29500, 30500 and 31112 are bit-equal to gl28.
   - Otherwise stop and report what differed.
2. **Wait pairs.**
   - Count PSP-lighter px (T=6) inside each decal's coverage from the twin GER, PSP against the emulator's same frame and arm.
   - O frames should lose about 30-50% of tex-79 coverage, like 29500/30500.
   - The X, Y and B<n> counts give each arm's effect directly. The smallest clearing n sizes a bias.
3. **W frames.** PSP colour equals the emulator's inside the coverage → sampling is identical and H3 is dead. Otherwise H3 is alive.
4. **Z dumps.**
   - Layout from the 31262 pair (plain vs mirror).
   - Then ΔZ_base = PSP − emulator on base pixels of O frames, and ΔZ_decal from W frames.
   - Fit per triangle. Test the span rule: does |ΔZ| scale with the triangle's z span, and is it linear within a triangle?
   - Check the hole criterion: emu decal + ΔZ_decal < emu base + ΔZ_base.
   - If |ΔZ_base| ≤ 1 everywhere, the error is not in the base's z. Then look at the decal-only effects that Y separates.
5. **Walk.**
   - The R-by-arm table from the log.
   - an4.py lighter patches on every live capture against E-A's frame (same arm).
   - The drained walk shots with Z.
6. **Outcomes.**
   - **X clears both phases.** gl30 makes X the default plain fix after the owner OKs before/after shots, and DEVIATIONS.md records the under-1-px shadow shift. Unpaired decals stay O, or get B<n> if any loss remains in them.
   - **X fails, B clears.** The GE's z depends on more than the vertex data (unexpected). gl30 uses the smallest clearing B<n>, with its emulator bleed shown to the owner.
   - **Neither clears and W differs from the emulator.** H3: gl30 runs H3's test (tex 79 as a 1:1 sprite) and pads T4 textures.
   - **Objective clean but R held.** Reopen the night's B (display) and E (blur) with the eye and fbchk lines.
   - **R held only in O windows.** The marks are the decal drop, and the onset question becomes moot.

## 5. Checks on the emulator and in the container before delivery

The emulator is the headless PPSSPP 1.20.4 with software graphics and PPSSPP_VFPU_RN=1, as g28m/run.sh uses. Its own ms root goes under g28m.

- **E1. Twin, keys absent.**
  - State hashes equal the reference.
  - List and frame digests for 30500-30900 equal gl28's twin (out_g22b.log). This proves O == gl28.
- **E2. Twin, gl29 cfg.**
  - Digests for every frame 29490-31400.
  - Pairing counts per frame: b808-b812 or their equivalents must pair in every deck frame where they exist. List fallbacks by reason.
  - Largest aux.
- **E3. Emulator E-A** (the PSP's cfg plus MGS2_GL_SHOT_RANGE=30500:31400:2).
  - refcmp.py --partial shows 0 differing hashes; emucmp.py shows digests equal to E2.
  - Every shot line is present with "last list = shot frame: yes", including the ME-turn shots, and none says GAVE UP.
  - The Z files are written.
  - Reading the mirror view is harmless on PPSSPP [I].
- **E4. Emulator E-B** (the same, with MGS2_GL_DZ absent).
  - O frames are bit-equal to gl28's emulator frames (g28m/emu_shots, build_gl28/live_ref) and to E-A's O frames.
- **E5. Arm effects on the correct renderer (E-A vs E-B, same frames).**
  - X and Y: changed px must sit only along decal edges (the under-1-px shift). If not, the remap is wrong: X and Y are left out of gl29.
  - B1-B8: changed px are bleed. Check Snake's boots and the crate bases. If B2 bleeds visibly, the walk uses B1.
  - The threshold is the MGS2 picture margin in mgs2/NOTES.md, which is not in this container. Look it up.
- **E6. Z reader.**
  - Emulator Z (plain view) against the float model (margin.py's z images) on base pixels: |Δ| ≤ 1 expected.
  - Mirror view == plain view on PPSSPP [I].
- **E7. Span rule a priori.** Run tri2.py over the twin GERs for every walk frame and every wait arm frame. This gives predicted loss per frame, so the O windows' expected marks are written down before the run.
- **E8. Visible darkening of the tex 75/78 decals.** Extend expd.py to them. If their darkening is under about 6 luma, their survival in §1.1 proves nothing, and the span rule rests on b810, b812, b773 and b788 only.
- **E9. Build gates.** The ME gates pass, and every recompiled object comes from t29 with its logged compile line.
- **E10. Delivery rule.** The EBOOT md5 delivered is the one E3 ran. Write a README for gl29 (what changed, what to do, what the run should show, where the logs land). Ship only after gl28's log is fully read (done: rd28.out).

## 6. Risks and UNKNOWNs

- **The GE's exact depth rule.** UNKNOWN; G2 and arm Y measure it. X does not depend on it if the GE gives the same z for the same vertex data [I].
- **The hardware Z layout and the mirror read.** UNKNOWN and [I]. The mirror read happens once, at the last shot.
- **Drained shots in ME turns.** First use on the PSP. E3 runs them on the emulator first. If one gives up, the log flags it and that Z dump is not used.
- **What the owner sees.**
  - In the wait: about 6 frames change (W, X, Y, B).
  - In the walk: the X and B2 windows change the shadows. Their look is wrong only if E5 finds more than edges and bleed.
  - Pauses and freezes as in §3.3.
- **Recompiled objects with gl20 profiles.** The speed may move [I]. The per-window list time shows it. Nothing in gl29 is a speed claim.
- **The onset question** (gl25/26 vs present since gl18me). UNKNOWN. The R-by-arm table answers whether the marks are this drop.
- **X pairing is tuned to this mesh pattern** (same triangles, +5 model y). Other decals stay O, and the log counts them.

## 7. Files

- My container scripts (counts only, game data stays in the container):
  - /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g28m/p29/dq.py: decal/base pairs, leads in z/w and GE.
  - /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g28m/p29/cov.py: PSP-lighter px per decal coverage.
  - /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g28m/p29/pal.py: palette and vertex alpha ranges.
  - /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g28m/p29/zw.py: depth writers under a decal.
  - /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g28m/p29/tri2.py: loss vs lead and triangle z span.
  - /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g28m/p29/pairchk.py: pairing facts for X.
- Inputs used:
  - /home/claude/t28 (read only)
  - /home/claude/runs/gl28/{log.txt, rd28.out, shot_*.bmp, live_*.bmp}
  - g28m/tw/ger/f30500, f30846, f31046 and f31112 .ger
  - g28m/emu_shots/
  - l28/marks_plan.md and marks_crit.md
- Nothing was edited under /home/claude/t28, /home/claude/main or any build folder.
