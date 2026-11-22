# Exact early cull of converter calls that draw nothing (cull study, 2026-11-22)

Tree: /home/claude/t37c (t36 as copied 2026-11-21 ~22:45 UTC, plus this study). Keys: MGS2_NAT_CULL, MGS2_NAT_CULL_CYCLE, MGS2_NAT_CULLCHK.
Marks: [C] measured or read in code, [C twin] x86 twin, [C model] PspProf cost model (PSP ms = (1.65 x instr + 68 x I-miss + 86 x D-miss) / 333,000),
[C emu] PPSSPP, [I] inferred (basis named), UNKNOWN. Nothing here ran on a PSP.

Short result:
- What the cull does: it leaves out effect strips (25f0 / 2870) that would draw nothing, before they are converted.
- Exactness on the twin [C twin]: every gate passed.
- Speed in the model [C model]:
  - the ME saves 1.4-2.35 ms a step in the rain;
  - the test itself costs 2.6-3.0 ms of that.
- PSP expectation [I]: from a 1.0 ms loss to a 2.35 ms gain a step, most likely 0.6-1.6 ms saved.

## 1. What draws nothing

### Source and units

- The census comes from twin run cen2 (MGS2_CULL_CEN; study_cull/cen_all2.txt and cen_tables2.txt) [C twin].
- Figures are calls a step, and a step here is one vblank.
- The census classes each call by why it draws nothing:
  - **one-point**: every vertex is at one point, so the call already returns before conversion;
  - **all-off**: every vertex is beyond one clip plane (the cull's target);
  - **each-tri-off**: every triangle is trivially rejected, but the vertices do not share one plane;
  - **clipper-empty**: the call reaches the clipper and comes out empty or back-facing;
  - **df0 cut**: an existing ship-mesh cut after the transform;
  - **held**: P06 / P07, which are always drawn.

### By window (calls a step [C twin])

| window | chain / kind | calls | drawn | one-point | all-off | each-tri-off | clipper-empty | df0 cut | held |
|---|---|---|---|---|---|---|---|---|---|
| rain wait 29,800-29,900 | 6 fx25f0 | 80.0 | 17.4 | 7.0 | 48.5 | 0 | 7.1 | - | - |
| | 6 fx2870 | 48.0 | 0 | 44.0 | 0 | 0 | 4.0 | - | - |
| | 3 df0 | 285.0 | 130.8 | - | - | - | - | 154.1 | - |
| | 2 P06 | 141.0 | - | - | - | - | - | - | 141.0 |
| rain walk 31,000-31,100 | 6 fx25f0 | 65.6 | 4.2 | 7.1 | 54.2 | 0.02 | 0.17 | - | - |
| | 6 fx2870 | 48.0 | 0 | 44.0 | 0 | 0 | 4.0 | - | - |
| | 3 df0 | 108.4 | 35.4 | - | - | - | - | 73.0 | - |
| | 2 P06 | 208.7 | - | - | - | - | - | - | 208.7 |
| d01t deck 24,000-25,100 | 7 fx25f0 | 135.2 | 21.5 | 10.0 | 102.9 | 0.46 | 0.39 | - | - |
| | 7 fx2870 | 26.9 | 0 | 23.5 | 2.2 | 0 | 1.1 | - | - |
| | 6 fx25f0 | 7.4 | 0.4 | 0.6 | 6.3 | 0.03 | 0.03 | - | - |
| | 4 df0 | 38.8 | 7.1 | - | - | - | - | 31.8 | - |
| | 3 P07 / 2 P06 | 66.3 / 30.4 | - | - | - | - | - | - | all |
| opening 10,300-11,100 | 6 fx25f0 | 6.45 | 3.26 | 0.49 | 2.28 | 0.06 | 0.35 | - | - |
| | 6 fx2870 | 1.44 | 0 | 1.26 | 0.10 | 0 | 0.08 | - | - |
| | 2 df0 | 11.6 | 4.4 | - | - | - | - | 7.2 | - |
| route 0-31,400 | 6 fx25f0 | 6.51 | 1.31 | 0.59 | 4.17 | 0.01 | 0.43 | - | - |
| | 7 fx25f0 | 4.78 | 0.76 | 0.35 | 3.63 | 0.02 | 0.01 | - | - |
| | 6 fx2870 | 3.89 | 0 | 3.56 | 0.01 | 0 | 0.32 | - | - |
| | 3 df0 | 19.9 | 8.6 | - | - | - | - | 11.3 | - |

- Fx calls have about 40 vertices each in the rain [C twin]. A df0 cut group has 17.5 [C twin].
- No call hit these classes [C twin]: bad attribute, c4 none (c4 is off in this configuration), and df0 "other empty".

### By owner, rain wait 29,800-29,900 (calls a step [C twin])

- d_splash_fall, fx25f0: 34. Of these, 29.0 are all-off, mostly plane bit 08 (y < -w), and 5.0 are clipper-empty.
- sky_column: 8 all-off.
- sling: 6 all-off.
- set_tex: 4 all-off.
- fog_wall: 1.5 all-off and 0.5 clipper-empty.
- blood_wl, fx2870: 32. Of these, 28 are one-point and 4 are clipper-empty.
- Unnamed fx2870 calls: 16, all one-point.

In the rain walk, d_splash_fall again has 34 calls a step, all of them all-off [C twin].

### Per 100-vblank window to vb 31,400

- The full list, by window, chain, kind and reason, is in /home/claude/main/study_cull/cen_all2.txt [C twin].
- The table below sums it in blocks of 1,000 vblanks, as calls a step [C twin]:
  - **fx test-cut**: what the cull's test can catch (all-off plus each-tri-off);
  - **held**: P06 / P07;
  - blocks with no converter calls are left out (0-2,999, 5,000-7,999, 13,000-14,999, 20,000-21,999).

| vb block | all calls | fx | fx empty | fx test-cut | df0 | df0 cut | held |
|---|---|---|---|---|---|---|---|
| 3,000-3,999 | 17.2 | 17.2 | 17.2 | 0 | 0 | 0 | 0 |
| 4,000-4,999 | 9.5 | 9.5 | 9.5 | 0 | 0 | 0 | 0 |
| 8,000-8,999 | 4.6 | 2.2 | 2.1 | 1.2 | 2.0 | 1.8 | 0.3 |
| 9,000-9,999 | 2.9 | 1.5 | 0.6 | 0.1 | 1.4 | 0.6 | 0 |
| 10,000-10,999 | 13.3 | 4.9 | 2.8 | 1.4 | 7.8 | 4.8 | 0.6 |
| 11,000-11,999 | 4.9 | 2.4 | 1.7 | 1.1 | 2.5 | 1.7 | 0 |
| 12,000-12,999 | 2.0 | 0.8 | 0.4 | 0.2 | 1.2 | 0.9 | 0 |
| 15,000-15,999 | 39.7 | 0.4 | 0.3 | 0.2 | 0.4 | 0.2 | 38.9 |
| 16,000-18,999 | 161.0 | 0 | 0 | 0 | 0 | 0 | 161.0 |
| 19,000-19,999 | 146.2 | 1.6 | 1.6 | 0.2 | 0 | 0 | 144.6 |
| 22,000-22,999 | 2.1 | 0.2 | 0.1 | 0 | 1.9 | 0.1 | 0 |
| 23,000-23,999 | 112.8 | 26.0 | 21.7 | 9.3 | 56.9 | 36.7 | 29.9 |
| 24,000-24,999 | 340.1 | 187.4 | 163.4 | 122.8 | 45.0 | 36.7 | 107.7 |
| 25,000-25,999 | 2.4 | 1.4 | 1.1 | 0.8 | 0.2 | 0.2 | 0.8 |
| 26,000-26,999 | 132.9 | 0.2 | 0.1 | 0 | 1.9 | 1.4 | 130.7 |
| 27,000-27,999 | 161.0 | 0 | 0 | 0 | 0 | 0 | 161.0 |
| 28,000-28,999 | 115.9 | 1.6 | 1.6 | 0.2 | 0 | 0 | 114.3 |
| 29,000-29,999 | 526.9 | 118.9 | 102.1 | 45.2 | 263.5 | 142.6 | 144.5 |
| 30,000-30,999 | 544.8 | 125.5 | 110.9 | 49.3 | 259.6 | 143.1 | 159.7 |
| 31,000-31,399 | 339.8 | 90.0 | 87.8 | 43.7 | 97.6 | 69.5 | 152.2 |

The busiest 100-vblank windows for test-cut fx calls are all on the d01t deck, 24,000-24,800, at 99-158 calls a step [C twin].

## 2. Where the time goes

### The model's per-function split

- Read off minus on, same ELF (study_cull/psp), window by window [C model].
- So the saving shows where the calls the cull leaves out spent their time.
- The rain wait ME window is 29,810-29,890 (model/mc_off and mc_on).

| function (ME) | off: ms a step | on minus off: ms a step |
|---|---|---|
| xc_call (the converters, xc_fpos inlined) | 7.42 | -1.66 |
| vc_fpos_rec (vertex decode) | 2.48 | -1.35 |
| vc_cflags | 1.03 | -0.56 |
| vc_xform | 1.09 | -0.26 |
| xc_xform | 0.79 | -0.17 |
| me_memset (the class array; 7,573 fewer instructions a step) | 1.50 | -0.08 |
| gu_drawn, gu_clut_load, ud_new, ud_eager, gf_reg, gl_tex | - | -0.02 to -0.04 each |
| **the cull's test, xc_fpos_off1** | 0 | **+2.91** |

- The GE-side functions in the last plain row run exactly the same number of instructions in both arms [C model]. Only their cache misses change, so their small gains are a cache effect.
- So the calls the cull leaves out cost about 4.3 ms a step of ME time in the rain wait [C model], and about 5.0 in the rain walk.
  - About 80% of that is the conversion proper: decode, clip flags and the triangle loop.
  - About 10% is the transform setup.
- The test reads about 2,000 left-out vertices a step [C twin census: 1,938 in the wait, 2,273 in the walk]. That is about 180-230 instructions a vertex [I: 452k / 407k test instructions a step, C model].
- An earlier study's figure for comparison (verifyA fo2): dropping all of d_splash_fall's calls cut the ME by 5.31 ms a step [C model]. That includes calls it draws.

### PSP per-stage timers (psp_harness.md, the gl34 native-group lines)

- These timers cover the native ship groups (df0) only. There are no PSP stage timers for the fx converter, so the fx share on the PSP is UNKNOWN.
- A ship group in the rain wait costs 63.3 us in the fast arm [C PSP]. Its stages:
  - walk+pointers 7.6, register 9.1, A+D 6.0, call rest 2.3, host + GS state 6.0;
  - transform 4.2, positions 4.2, cut 4.8;
  - texture 8.2, drawn areas 4.6, the rest about 10.
- A df0 group the cut removes still pays every stage up to the cut. That is about 44 us [I: the sum of the stages before and at the cut], times about 154 groups a step in the rain wait [C twin].

## 3. The test per kind

### Kinds worth it (at least 0.3 ms a step of ME in the rain [C model / I])

- **fx25f0 / fx2870 all-off and each-tri-off**: about 4.3-5.0 ms gross [C model]. Prototyped.
- **df0 cut groups**: the front end before the cut is about 44 us a group [I, PSP timers]. Not prototyped (section 7).
- **fx clipper-empty**: about 11 calls a step in the rain wait, 550 vertices [C twin], about 1 ms [I: 2.2 model-us a vertex from the gross above]. Left out: the only exact proof is the clipper itself.

### Kinds not worth it, or not candidates

- **fx one-point**: these calls already return before conversion. What is left is the texture look-up, which changes state and must stay.
- **P06 / P07**: always drawn, so they are not empty.
- **c4**: off in this configuration.

### Earliest point for fx (ge/xconv.c xc_fpos_full)

The test runs after these steps, which it leaves in place:
1. the texture look-up in xc_call, which writes the frame's texture table and texcache counters;
2. the choice between the prim's streams and the VU memory (vc_fpos_rec_ok, the xc_rec3 counters, xl_vm_sync);
3. the one-point test.

It runs before vc_fpos_rec / vc_fpos.

Everything the test needs is in memory before the work it skips:
- the matrix at T+0x10..0x13;
- each vertex's position, colour and UV words;
- the header's count;
- the thinning table (XcHost.thin) and XcHost.clipfast.

For fx, no data is known only after the skipped work.

### The test (xc_fpos_off, xc_fpos_off1)

**It reproduces the conversion's own result.** It is not a bound:
- Each clip coordinate of vc_vu_clip / vc_vu_clip_nc is its own sum over one matrix column, in a fixed order.
- The test computes the same expression, in the same order, in the same file, with the same flags (-ffp-contract=off, xc_vfl = vfl, the same fast / clamped choice per vertex).
- So it gets the same bits, NaN and infinities included.
- The flag bit is vc_cflags's own comparison.

**Mode 0:**
- The first vertex that gets a clip position gives its lowest set bit b. If it has no bit, the call is kept.
- Every later counted vertex must have bit b, which takes two coordinates each.
- If one does not, the test goes to mode 1. This is the v2 fix: v1 switched planes without checking the earlier vertices again, and the check mode caught it.

**Mode 1:**
- Each vertex's six bits are computed.
- Every triangle the conversion would class (no ADC, not thinned out) must have a common bit on its three vertices. That is class 2, trivially rejected.

**When a left-out call converts to nothing:**
- If every classed triangle is class 2, xc_fpos sets no "drawn" and returns 0.
- Thinned strips never share a triangle with kept strips: a strip starts at an ADC vertex followed by another. The test counts strip starts exactly as xc_fpos does.

**The test keeps a call (it converts as before) when:**
- the count n0 is under 2 or over 1024 (vc_fpos refuses more than 1024, -1);
- any attribute would make vc_fpos return -4 (a colour word over 255, or UV q not 0x1000 when textured);
- no vertex is counted;
- h->trace is set;
- on x86 only, the call is a rec-check call (chk).

## 4. The state the skip must leave, and how

| state the full path writes for a call that draws nothing | how the skip leaves it |
|---|---|
| XcStats: clip[0], clip[1], tris[0], tris[2], thin_tris, thin_calls, thin_strips, thin_drop | Replayed exactly. XcCullD collects the same increments and xc_cull_add adds them. The check mode compares every XcStats field before cull_n with memcmp: 298,925 calls, 0 differing [C twin]. |
| tris[1,3,4,5], poly_* | Not written: no triangle is inside or reaches the clipper. A rejected 2870 triangle counts in tris[2] only [C code]. |
| Fx.f scratch (v, fl, drop, cc, cf, cls, pv, pfan) | Proven unread. Every later call writes each element it reads first: v and fl by vc_fpos for i < n, cf and cc for each counted vertex, drop when keep > 1, cls by memset(n). Only the trace reads them afterwards, and a call with a trace is never skipped [C code]. |
| VcXform / GerXform (locals), the return value 0, no batch, no blob | Locals. The return value is the same 0, so the caller does the same thing [C code]. |
| VU-memory sync, xc_rec3_calls / xc_rec3_vm | Done before the test, so unchanged. The -8 fallback after vc_fpos_rec cannot happen once vc_fpos_rec_ok has passed with n0 <= 1024, so the skip changes no fallback [C code]. |
| texture table, texcache counters | The look-up runs before the test, as before. |
| the error paths (-1, -4) | The test keeps those calls, so the same error is returned. |

- Instruction counts in every GE-side function match between the model arms. Only xc_fpos_off1, the conversion functions and me_memset change [C model].
- The switch reaches the ME through the frame hook (XmeHook.cull, copied into glf.h.cull at xm_begin). The main side sets it at each picture's start.

## 5. Prototype gates and results

Twin: /home/claude/main/study_cull/tw2/x86 (final x86 code). Base keys as gl36's gate: NAT_CYCLE=3,3,3,3, INPL, FAST, KEEP=1, REUSE_CYCLE=1,1,0,0, DZ, X.

### Hashes [C twin]

- These arms each gave 685/685 hash lines, byte-equal to hashref c9ddb593:
  - off;
  - on (MGS2_NAT_CULL=1);
  - chk (+MGS2_NAT_CULLCHK=1);
  - cy (MGS2_NAT_CULL_CYCLE=1,1,0,0);
  - cy2 (0,0,1,1);
  - lw2off / lw2on (MGS2_NAT_LW=2);
  - cen2.

### Frames [C twin] (cmp.sh, as g1_cmp36)

- These pairs gave 17,664 frames with 0 differing in every column (list/frame digest, so GE words, dz, content, natb, natz, natq, nats): off-on, off-chk, off-cy, off-cy2, lw2off-lw2on.
- For the cycled pairs that holds in both arms and in each turn's first picture.
- Other log lines: 0 differing, apart from the cull's own lines, the key echo lines and times.
- gl36's out_rx against off: 0 frames differing. Eight other lines differ, all of them key echo lines and the hashref path.

### Counters [C twin]

| arm | tested | left out | left-out vertices | kept, first vertex inside | kept, a triangle | left out by the triangle test |
|---|---|---|---|---|---|---|
| on | 436,237 | 298,925 | 9,431,186 | 75,848 | 61,464 | 31,388 |
| cy | 212,403 | 148,303 | 4,704,068 | - | - | 15,093 |
| cy2 | 199,286 | 136,952 | 4,175,744 | - | - | 14,070 |

- In every arm: bad attribute 0, count 0, none counted 0.

### Check mode [C twin]

- 298,925 left-out calls were converted anyway. DIFFERING: 0.
- 52,105 kept calls drew nothing. These are the test's misses: it is conservative.
- The v1 code failed this gate (DIFFERING 1). Its logs are in study_cull/v1 and study_cull/tw.

### Rebuild and PSP ELF checks [C]

- Rebuild after the PSP-only key change: study_cull/tw3/x86/boot is byte-identical to tw2's (md5 bebe9f847a06982f9cb3ffb4b7bef230), and every object is the same.
- PSP ELF study_cull/psp:
  - EBOOT md5 7bd98b309b94a201ded9b342f3fa1e1d;
  - text 4,419,652 B, which is gl36 +3,040 B; bss +1.8 KB;
  - xme_rest_chk, mesec_chk and me_call_chk PASS.

### Emulator model runs [C emu]

- In both arms, 59 of 62 hash lines equal the hashref.
- The 3 that differ are the profiler's own vblanks (30,000, 30,500, 31,000), as in gl36's and gl35's model runs.
- The on arm: tested 332,769, left out 242,917 (8,768,176 vertices).
- The effect-vertex counter is equal between the arms (12,828,888).

### Not run

- The check mode on the emulator: UNKNOWN.
- Pictures on the emulator: not compared, UNKNOWN. The twin's frame digests cover the GE words.

## 6. Model figures and PSP expectation

### Model figures

- The ELF: study_cull/psp.
- PGO: gl36's PGO objects and code order are used, except five objects recompiled from t37c without a profile (ge_gelive, ge_xconv, ge_xlme, ge_xme, xl_psp). Both arms use this same ELF.
- Windows: 40 pictures each, ms a step [C model].

| window | ME off | ME on minus off | of which test | gross removed | main (+hand-off) off | main on minus off |
|---|---|---|---|---|---|---|
| rain wait, ME turn 29,810-29,890 | 100.89 | -1.38 | +2.91 | -4.30 | 77.50 + 13.31 | +0.12 / +0.09 |
| rain wait, main turn 29,910-29,990 | 94.57 | -1.66 | +2.99 | -4.64 | 76.09 + 1.37 | -0.05 / -0.01 |
| rain walk, ME turn 31,010-31,090 | 90.19 | -2.35 | +2.65 | -5.00 | 82.37 + 14.00 | -0.16 / -0.02 |
| rain walk, main turn 31,110-31,190 | 80.67 | -2.33 | +2.64 | -4.97 | 77.14 + 0.83 | -0.03 / +0.01 |

ME split in the rain walk ME window [C model]:
- xc_call -1.99, vc_fpos_rec -1.59, vc_cflags -0.56, vc_xform -0.31, xc_xform -0.19, me_memset -0.10;
- the rest -0.05 or less each;
- the test +2.65.

### PSP expectation [I]

Calibration: on recent levers the model overrated ME savings 1.5-2.2 times, and about 1 time on keeps.

**(a) The overrating scales all ME work, the test too.**
- Rain wait: the ME saves 0.6-0.9 ms a step.
- Rain walk: 1.1-1.6.
- With 1x: 1.4 and 2.35.

**(b) Only the removed work is overrated, and the test costs what the model says.**
- Rain wait: from a 1.0 ms loss to a 0.05 ms loss.
- Rain walk: from a 0.4 ms loss to a 0.7 ms gain.

Most likely: (a), so 0.6-0.9 ms saved in the rain wait and 1.1-1.6 in the walk. The basis is the earlier levers, whose overrating was not split by cause, so reading (b) cannot be ruled out.

**Main CPU:** within ±0.16 [C model], inside the ±0.5 run-to-run noise. Expect no change [I].

**The limit:** the ME stays the limit, since the model's ME minus main gap (~10 ms a step) is larger than the saving [C model].

**How to settle it:** a PSP run with MGS2_NAT_CULL_CYCLE in turns would resolve 0.3 ms on the ME side, where the noise is ±0.1. It is needed before keeping the lever.

### Cutting the test's cost [I, not prototyped]

- Fusing the test into vc_fpos_rec's own decode loop would read each vertex only once.
- The clip work would stop at the first vertex inside the plane.
- That would recover part of the +2.6-3.0.

## 7. What is left out and why

- **An earlier df0 cut, before register / A+D / GS state.**
  - It would save the about 44 us front end of about 154 groups a step [I], at most about 1.5-2.3 ms a step [I: transform + positions + cut, plus the host/GS state, about 10-15 us a group].
  - The test needs the box, which is only known after the transform, unless gl36's reuse (R-b) keeps it.
  - A skip must leave the GS registers, CLUT loads, VU memory model, zr and drawn areas exactly as the full path does. That is gl36's R3/R4, rejected for risk.
  - Not prototyped.
- **Clipper-empty fx calls.** About 11 a step in the rain wait [C twin]. Only the clipper (vc_clip_tri and the back-face test) proves them empty, so no test cheaper than the conversion exists.
- **One-point fx calls.** They already return before conversion. What is left is the texture look-up, which writes the texture table and texcache counters that later work reads, so it stays.
- **P06 / P07.** Always drawn. Leaving them out would change GE words.
- **c4 (spr / quad / line).** Off in this configuration. No calls [C twin].
- **A test before the texture look-up.** It would need proof that the texture table and texcache state are unread, which was not attempted.
- **PGO for the five recompiled objects.** Not trained, so the model's absolute figures differ from gl36's. The off-on difference uses one ELF.
- **The PSP run itself.** Not allowed in this task. The PSP figure is UNKNOWN until it runs.

## 8. Patch and files

- Patch: /mnt/project-files/mgs2/main/attic/cull/cull_vs_t36.diff
  - diff -ru of t37c against t36 as copied; ge/, boot/, psp/; 8 files.
  - Applies to the base with patch -p1 and gives t37c's files exactly.
- Twin builds and logs: /home/claude/main/study_cull/tw2/x86, tw3/x86, tw/, v1/; cmp.out, cen_all2.txt, cen_tables2.txt.
- PSP ELF and model: /home/claude/main/study_cull/psp, model/ (mc_off*, mc_on*, log_mc_*.txt), pgo_mc_off.out, pgo_mc_on.out.
