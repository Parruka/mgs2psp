# Cull study v3: an exact early cull with a cheap test (2026-11-22)

Tree: /home/claude/t37d. It is the current /home/claude/t36 (gl36 with fix A and fix B; every file matches build_gl36/t36_files_B.md5, checked again when the patch was made) plus this study.
Patch: cull_v3_vs_t36.diff (next to this file). Keys: MGS2_NAT_CULL, MGS2_NAT_CULL_CYCLE, MGS2_NAT_CULLCHK.
Marks: [C] measured or read in code, [C twin] x86 twin, [C emu] PPSSPP 1.20.4, [C model] PspProf cost model (PSP ms = (1.65 x instr + 68 x I-miss + 86 x D-miss) / 333,000), [I] inferred (basis named), UNKNOWN.
Nothing here ran on a PSP. All model figures come from a NO-PGO ELF, and both arms of each pair use that same ELF.

## Short result

- **What it does.** It finds effect strips (25f0 / 2870) that would draw nothing, before they are converted, and leaves them out. The test is now a bound over the vertices' box, not a rerun of every vertex.
- **Exact.** Every gate passed:
  - twin: 685/685 = c9ddb593 in 9 arms; 17,664 frames with 0 differing in every pair; check mode DIFFERING 0 on 294,164 left-out calls;
  - emulator: 62/62 and 916/916 in the off, on, cycled and check arms; DIFFERING 0 on 245,840 calls; 51 of 52 pictures byte-equal, the 52nd being the known noise frame.
- **The test's cost** [C model, rain wait ME turn]:
  - v3: 183k instructions, 1.71 ms a step;
  - v2: 452k instructions, 2.91 ms (v2's ELF: gl36's PGO objects, with its own five objects untrained).
  - The test now costs 28.5% of the instructions it removes (v2: 73%), or 36% in ms.
- **Net on the ME** [C model, ms a step]: rain wait -2.99 (ME turn) and -2.43 (main turn); rain walk -3.73 and -3.53. The main side moves -0.07 to -0.31, inside the noise.
- **PSP expectation** [I]:
  - rain wait: the ME saves 1.1-2.0 ms a step;
  - rain walk: 1.6-2.5;
  - in the pessimistic reading, 0.2-1.9.
  - Unlike v2, no reading of the calibration gives a loss.

## 1. What changed from v2

| | v2 (cull_design.md) | v3 |
|---|---|---|
| Switch to the ME | a new int XmeHook.cull | bits 8-15 of the existing XmeHook.begin word (begin = 1 \| cull << 8). Static asserts hold XmeHook at 40 B and XmeE at 132 B, gl35's sizes. Fix B's 80 B assert on XmeRec is kept. |
| Test | reruns vc_vu_clip for every vertex in the conversion's own order (2 coordinates a vertex, or 6 in mode 1) | vertex 0's exact flags give the candidate planes; a box over all positions; a bound per plane with a proven rounding margin; a second pass over the attributes only for a proven call |
| Mode 1 (each triangle off, no shared plane) | yes | dropped. Such calls are converted as before. |
| Thinned calls (keep > 1) | tested, thin counters replayed | kept (converted as before): 190 calls in the whole twin run [C twin] |
| Clamped clip path | reproduced per vertex | never needs a bound: the call is kept unless every input is within the fast path's range. In that range the clamped path gives the same values (vu1conv.h's proof), so the bound also covers clip-fast off. A counter proves it ran (section 5). |
| Check mode | the predicted counters, compared | the same, plus every vertex's flags (Fx.f.cf) from the full conversion must carry the proven plane's bit |

Calls left out over the whole twin run [C twin]:
- v3: 294,164 calls (9,216,184 vertices). v2: 298,925 (9,431,186).
- The 4,761 calls v2 left out and v3 keeps all draw nothing. In check mode, kept calls that drew nothing went from 52,105 (v2) to 56,866 (v3); the difference is the same 4,761.

## 2. The test (ge/xconv.c xc_fpos_off, xc_cull_box, xc_bound, xc_cull_p2)

It runs at the same point as v2's test, with the same preconditions:
- after the texture look-up, the choice between the prim's streams and the VU memory (with its -8 fallback), and the one-point test;
- before vc_fpos_rec / vc_fpos;
- only with XC_CULL_FX, no trace, and (x86 only) no rec-check call.

Steps in order:
1. **Count.** n0 must be 2..1024; otherwise the call is kept (cull_n[6]).
2. **Thinning.** If the call has a thin entry with keep > 1, it is kept (cull_n[11]).
3. **Matrix.** T+0x10..0x13, 16 words. Each must be within vc_clip_in's range (0, or 2^-28 <= |x| < 2^60); otherwise the call is kept (cull_n[7]). In that range vfl leaves a word as it is, so the test's m equals vc_xform's.
4. **Vertex 0, the early keep.** Its position must be in range. The test computes vc_vu_clip_nc and vc_cflags exactly as the conversion does. If vertex 0 has no flag bit, the call is kept at once (cull_n[3]). In the rain wait most kept calls stop here: 15.6-15.8 of the 28.5 kept a step [C twin].
5. **Pass 1 (xc_cull_box).** One loop over the n0 positions, three words a vertex. It keeps the least and the largest value per axis as int keys that order floats (key = u ^ ((int32)u >> 31 & 0x7fffffff); the key is its own inverse). Both ends of each axis must be in range; otherwise the call is kept (7). NaN and infinity sort beyond every finite value of their sign, so they always land at an end and fail the range check.
6. **Bound (xc_bound, section 3).** For each flag bit of vertex 0, lowest first, both of the plane's linear forms must be proven below the margin over the box. The first plane proven wins. If none is proven, the call is kept (cull_n[4]).
7. **Pass 2 (xc_cull_p2), only for a proven call.** Per vertex:
   - no nonzero position value below 2^-28. The box already bounds |x| from above, but a box that straddles 0 can hold tiny values;
   - no colour word over 255, and UV q = 0x1000 when textured, decoded as fpos_rec_loop decodes them (K = 2 or 3) or as vc_fpos reads the VU memory. Otherwise vc_fpos would return -4, so the call is kept (5);
   - the ADC bit (flag word bit 15) counted.
8. **Left out.** XcStats takes what the conversion would add:
   - clip[0] += n0;
   - clip[1] += 0: every vertex is in range, so none goes through the clamps with clip-fast on, and the clamped branch adds h->clipfast = 0 with it off;
   - tris[0] += the ADC vertices among i >= 2;
   - tris[2] += the other n0 - 2.
   Then it returns 0, as the conversion does for a call that draws nothing.

**Why one plane on every vertex means nothing is drawn.** Every triangle of the strip (vertex i >= 2 without ADC) has three vertices that share that bit. Each is classed 2 (trivially rejected) and none reaches the clipper. The conversion sets no "drawn" flag and returns 0. This is v2's argument without mode 1 [C code]. The check mode tests it directly (section 5).

**Why two passes, not one** [I, from the model's per-function counts]:
- In the rain wait, 29% of the box vertices belong to calls the bound cannot prove. Pass 2 is never run for them.
- Now: pass 1 costs 30 instructions a box vertex, bound included, and pass 2 costs 34 a proven vertex. That is 147k a step.
- A fused loop would do pass 2's work, less its second read of the positions (about 27 instructions), on every box vertex. That is about 155k a step.
- A fused loop would save some I-misses (pass 2 has 266 a step). The two are about even, so the split stays.

## 3. The bound and its proof

### 3.1 What vc_cflags compares

- The conversion's clip position is c = M (p, 1), with each sum rounded in a fixed order: c_k = ((m_0k p_0 + m_1k p_1) + m_2k p_2) + m_3k, where m_ik = m[4i + k]. This is vc_vu_clip_nc; with every input in range, vc_vu_clip gives the same values.
- With axis a = b >> 1:
  - an odd bit b means c_a < -|c_w|, which is equivalent to (c_a + c_w < 0 and c_a - c_w < 0);
  - an even bit means c_a > |c_w|, which is equivalent to (-c_a + c_w < 0 and -c_a - c_w < 0).
- So bit b is set when both forms L_t = s c_a + t c_w are negative, with s = +1 (odd) or -1 (even) and t = +1, -1.
- These are exact statements about the float values the conversion computes; vc_cflags's comparisons do not round.

### 3.2 The forms over the box

- In exact arithmetic, L_t(p) = sum over i < 3 of g_i p_i, plus g_3, where g_i = s m_ia + t m_iw.
- Over the box lo <= p <= hi, the largest value is G = sum of (g_i >= 0 ? g_i hi_i : g_i lo_i), plus g_3.

### 3.3 The error of the conversion's sums

- With every input 0 or 2^-28 <= |x| < 2^60:
  - no nonzero product is below 2^-56;
  - a nonzero sum of two floats is a whole multiple of the finer one's last bit, so it is at least 2^-79;
  - nothing exceeds 2^122.
- So there is no underflow, no denormal and no overflow, and each step has a relative error of at most u = 2^-24 (round to nearest).
- The standard bound for a product sum of 4 terms gives |c~_k - c_k| <= gamma_4 x sum_i |m_ik| |p_i|, with p_3 = 1 and gamma_n = n u / (1 - n u).
- Hence |(s c~_a + t c~_w) - L_t(p)| <= gamma_4 x S(p), where S(p) = sum over i < 3 of (|m_ia| + |m_iw|) |p_i|, plus |m_3a| + |m_3w|.
- Every vertex in the box has |p_i| <= P_i = max(|lo_i|, |hi_i|), so S(p) <= S, where S is the same sum with P_i in place of |p_i|.
- **Sufficient:** G < -gamma_4 S implies the bit on every vertex in the box, hence on every vertex of the call.

### 3.4 The test's own float steps (xc_bound)

1. **g~_i = fl(s m_ia + t m_iw).** A float sum of two in-range values is 0 exactly when the real sum is 0, and has the real sum's sign otherwise. So the test's choice of hi or lo per axis is the exact maximiser's choice; when g = 0 both choices give 0.
2. **G~.** G~ = (((0 + g~_0 q_0) + g~_1 q_1) + g~_2 q_2) + g~_3. Counting the roundings each term goes through (at most 5) gives |G~ - G| <= gamma_5 x sum_i |g_i| P_i <= gamma_5 S, since |g_i| <= |m_ia| + |m_iw| and |q_i| <= P_i.
3. **S~.** It is a sum of non-negative terms, each with at most 5 roundings, so S~ >= (1 - gamma_5) S.
4. **M~.** M~ = fl(S~ x 2^-16 + 2^-60). The product with 2^-16 is exact (no underflow), and the sum rounds once, so M~ >= (1 - gamma_6) x 2^-16 x S.
5. **The test.** It passes when fl(G~ + M~) < 0. Rounding is monotone and keeps the sign, so this implies G~ + M~ < 0 exactly.

### 3.5 The margin

If the test passes:

> G <= G~ + gamma_5 S < -M~ + gamma_5 S <= -((1 - gamma_6) 2^-16 - gamma_5) S

This is below -gamma_4 S whenever (1 - gamma_6) x 2^-16 > gamma_4 + gamma_5:
- gamma_4 + gamma_5 = 9 x 2^-24 x (1 + 2^-21) = 5.36e-7;
- 2^-16 = 1.53e-5.
- So the margin is 28.4 times the sum of every rounding error of both the conversion and the test.

Edge cases:
- **S = 0.** Then G~ = 0 and M~ = 2^-60, so the test fails and the call is kept. The absolute term 2^-60 only makes the test stricter.
- **Other rounding modes.** Under any IEEE rounding mode the per-step error is at most 2u, and monotonicity and sign hold, so the factor is still about 14.
- **Fused operations.** The proof needs no fused multiply-add. xconv.c and vu1conv.c are compiled with -ffp-contract=off on both targets.
- **Calls near a plane.** They fail the bound and go the full path. In the rain wait that is 12.8-13.0 calls a step [C twin].

### 3.6 The clamped path

- The bound covers both clip paths because the call is kept unless every input the conversion reads for the clip is in range:
  - the matrix words, checked one by one;
  - the box ends;
  - the tiny values, checked in pass 2.
- In that range vc_vu (the clamps) leaves every product and sum as it is, and the same steps run in the same order (vu1conv.h, sb3). So vc_vu_clip and vc_vu_clip_nc give the same c.
- With clip-fast off (XcHost.clipfast 0) every vertex takes the clamped branch, and the bound still holds. The cf0 arms prove it (section 5).

## 4. The state a left-out call leaves

The v2 table (cull_design.md 4) holds, with these changes:
- **XcStats.** clip[0], clip[1], tris[0] and tris[2] are replayed. The thin counters are no longer touched, because thinned calls are kept. The check mode compares every XcStats field before cull_n with memcmp.
- **Fx.f scratch.** Not written by the test. As in v2, every later call writes each element before reading it, and a call with a trace is never left out.
- **The -8 fallback.** It is decided before the test (rr is reset at that point), so it cannot happen after a left-out call. Pass 1 and pass 2 read the same streams vc_fpos_rec would read (kind[0] 1, kind[1] 2 or 3, n0 <= num).
- **New state.** Cpred, Cdue, Cb and Cn are written only in check mode. Otherwise the test writes only St.cull_n and its own locals [C code].

## 5. Gates

### 5.1 Twin [C twin]

- The twin is /home/claude/main/study_cull/v3/twh/x86.
- It runs on gl36's delivered base: NAT_CYCLE=3,3,3,3, INPL, FAST, KEEP=1, REUSE_CYCLE=1,1,0,0, DZ X, to vb 37,800.

Arms:
- off;
- on: MGS2_NAT_CULL=1;
- chk: on plus MGS2_NAT_CULLCHK=1;
- cy8: MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0, in MGS2_OPT_CYCLE's 100-vblank turns from vb 24,001, so it does not line up with REUSE_CYCLE=1,1,0,0;
- l3off / l3chk: MGS2_SB_LANE=11, lane 3 off;
- cf0off / cf0chk: MGS2_OPT=fbffffff and fbffffff in every turn of MGS2_OPT_CYCLE, so clip-fast is off;
- win: on plus MGS2_CULL_WIN=10, for the window counts of 6.1.

**Hashes.** Every arm gave 685/685, and each arm's hashref file is byte-equal to c9ddb593.

**Frames** (cmp3.sh, with build_gl35/g1cmp35.py's columns: list and frame digests with counts, dz text, content, natb, natz, natq, nats):
- These pairs each gave 17,664 frames with 0 differing in every column: off-on, off-chk, off-cy8, l3off-l3chk, cf0off-cf0chk, off-cf0off.
- In off-cy8 that holds in both arms (0 of 14,419 and 0 of 3,210) and in each turn's first picture (0 of 35).

**Other log lines:**
- Filtered (the natcull totals and the cull's key echo left out): in each of the five cull pairs, 4 lines differ. All four are times: the twin's "xlive time" and the ms of the state-hash line.
- Unfiltered: 7 lines. These are the same 4, plus the two natcull totals and the key echo.

**Against t36's own twin** (build_gl36/tw/x86/out_rxB: fix B, the delivered keys), the off arm:
- 0 frames differing;
- other lines: the 2 time lines, the hashref path, and the off arm's natcull totals (all 0). Nothing else.

**A twin-only fix in this round.**
- The first final build (twg) read the cull keys through nat_setup at the first picture (vb 31). That set the native switches up earlier than t36 does (vb about 4,253), so the twin walk started counting there.
- Its "nat: twin walk" line read 350,255 transfers against t36's 322,725. Every hash, frame and other counter was equal.
- v2's twin had the same line. v2's doc missed it.
- boot/nat_stub.h's x86 reader now has its own key parse, as the PSP's does. twh gives t36's line exactly.
- The PSP is untouched: boot.c as psp-gcc preprocesses it is byte-identical to t36's.
- All twin figures here are from twh. The cull counters are the same in twg and twh.

**Lane 3 off itself.** off against l3off (both arms off) has equal GE words, but nats differs in 16,409 frames. That is the lane-3-off base, not the cull. The cull pair l3off-l3chk is equal in every column.

Counters over the whole run [C twin]:

| arm | tested | left out (vertices) | kept: vertex 0 inside | kept: no plane proven | kept: thinned | check: converted / DIFFERING / kept that drew nothing | clamped path: left-out vertices / converted through the clamps |
|---|---|---|---|---|---|---|---|
| on | 436,237 | 294,164 (9,216,184) | 75,666 | 66,217 | 190 | - | 0 / - |
| chk | 436,237 | (checked) | 75,666 | 66,217 | 190 | 294,164 / **0** / 56,866 | 0 / 0 |
| l3chk | 436,237 | (checked) | 75,666 | 66,217 | 190 | 294,164 / **0** / 56,866 | 0 / 0 |
| cf0chk | 436,237 | (checked) | 75,666 | 66,217 | 190 | 294,164 / **0** / 56,866 | **9,216,184 / 9,216,184** |
| cy8 | 215,631 | 148,574 (4,554,300) | 36,669 | 30,292 | 96 | - | 0 / - |

- In every arm: bad attribute 0, count out of range 0, out of the clamps' range 0.
- The cf0chk column proves the clamped branch ran: every vertex of every checked call went through vc_vu_clip's clamps, and DIFFERING is still 0.
- Vertices read by the test: 12,428,524, of which 3,136,674 were in calls it kept.

### 5.2 Emulator [C emu]

- Build: the no-PGO PSP build of t37d, /home/claude/main/study_cull/v3/nopgo, EBOOT md5 6f1b45d976da25576f411200dde53292.
- It follows build_gl36/nopgo:
  - every object is copied from build_gl36/nopgo;
  - the seven objects whose psp-gcc -E -P text differs from t36 are compiled again with psp.sh's own lines and post-steps: ge_gelive, ge_xconv, ge_xlme, ge_xme (ME-safe), ge_xme_me (ME-safe, xme_me_chk PASS), ge_fxthin and xl_psp;
  - psp.sh's RELINK branch then runs, with no code order. Its gates xme_rest_chk, mesec_chk and me_call_chk PASS.
- ge_fxthin.o and ge_xme_me.o came out with the same instructions as nopgo's. boot.c as the PSP preprocesses it is byte-identical to t36's, before and after the last x86-only change, so boot_rt.o stays nopgo's.
- The package (study_cull/v3/pk3) is the delivered no-PGO package (build_gl36/pk_nopgoB) with this EBOOT and its overlays.

| arm | cfg | hashes | digests 29,490-31,400 against the twin arm | pictures against off |
|---|---|---|---|---|
| off | the package's | 62/62 | 916/916 (455 ME turns, 461 main) | - |
| on | + MGS2_NAT_CULL=1 | 62/62 | 916/916 | 51 of 52 byte-equal; shot_24784 differs (326 pixels, luma <= 72, difference <= 3.3) |
| cy8 | + MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0 | 62/62 | 916/916 | 51 of 52; shot_24784 (280 pixels) |
| chk | + MGS2_NAT_CULL=1, MGS2_NAT_CULLCHK=1 | 62/62 | 916/916 | 51 of 52; shot_24784 (439 pixels) |

- shot_24784 is the known noise frame. t36's own no-PGO run (build_gl36/emu_nB, EBOOT c725cbae) also differs from this off arm in shot_24784 only (310 pixels), and is byte-equal in the other 51.
- Check mode on the emulator (the ME objects):
  - 245,840 left-out calls converted anyway, DIFFERING 0;
  - 28,772 kept calls drew nothing.
- On arm to vb 31,400: tested 340,085, left out 245,840 (8,862,918 vertices). The twin's counters at its first frame start after that point (vb 31,557) are the same in all 16 places.

### 5.3 Heap and code [C emu, C]

- **Heap low point.** The break's highest point is 0be5a000 at vb 23,677, against 0be57000 in t36's no-PGO run (+12 KB). The headroom at its lowest is about 120 KB, against 140 KB.
- **How to read that.** The headroom probe moves in 16 KB steps. The image grew 8,960 B (the heap block at the start: 0b229600 to 0b22b900).
- **PSP expectation** [I]: gl35 read 64 KB at vb 23,677. This patch takes about 9-12 KB of it, which leaves about 52-55 KB if gl36 reads what gl35 read. gl36 on the PSP: UNKNOWN.
- **Code.**
  - text 4,354,220 B, +6,108 against build_gl36/nopgo; bss +832 B; data the same.
  - ME: xc_fpos_off 1,232 B, xc_cull_box 1,404 B (xc_bound inlined), xc_cull_p2 892 B, xc_call +460 B.
  - xc_df0_off is now out of line (504 B). It costs 0.215 ms a step in both arms [C model].
  - main: rt_nat_cull_at 744 B.

## 6. Model figures (no PGO)

- ELF: study_cull/v3/nopgo, the same ELF for both arms.
- Cfg: build_gl36's model cfg (mgs2_m36_r.cfg, reuse in every turn, as in v2's model runs), with MGS2_NAT_CULL=1 for the on arm.
- Hashes: 59 of 62 equal to c9ddb593 in both arms. The 3 that differ are the profiler's own vblanks (30,000, 30,500, 31,000), as in gl36's and v2's model runs.
- Windows: 40 pictures (80 steps) each. Figures are ms a step [C model].
- The test is xc_fpos_off + xc_cull_box + xc_cull_p2.
- Gross removed = the ME's change minus the test.

| window | ME off | ME on - off | the test | gross removed | main + hand-off, off | main / hand-off, on - off |
|---|---|---|---|---|---|---|
| rain wait, ME turn 29,810-29,890 | 112.55 | **-2.99** | +1.71 | -4.71 | 85.57 + 13.35 | -0.15 / +0.02 |
| rain wait, main turn 29,910-29,990 | 103.96 | **-2.43** | +1.70 | -4.13 | 83.87 + 1.26 | -0.07 / +0.00 |
| rain walk, ME turn 31,010-31,090 | 102.33 | **-3.73** | +1.64 | -5.37 | 90.78 + 14.09 | -0.20 / -0.11 |
| rain walk, main turn 31,110-31,190 | 90.75 | **-3.53** | +1.64 | -5.17 | 84.89 + 0.77 | -0.11 / +0.00 |

Instructions a step [C model]:

| window | the test | ME change | gross removed | test / removed |
|---|---|---|---|---|
| rain wait, ME turn | 182.7k | -458.2k | 640.9k | 28.5% |
| rain wait, main turn | 183.1k | -432.9k | 616.0k | 29.7% |
| rain walk, ME turn | 181.3k | -540.1k | 721.4k | 25.1% |
| rain walk, main turn | 180.8k | -537.9k | 718.7k | 25.2% |

Where the removed time goes, rain wait ME turn [C model]:
- xc_call -1.87, vc_fpos_rec -1.50, vc_cflags -0.58, vc_xform -0.32, xc_xform -0.15;
- everything else under 0.11 each.

The test's cost by part, rain wait ME turn (instructions / I-misses / D-misses / ms a step) [C model]:
- xc_fpos_off: 35.3k / 1,137 / 64 / 0.42;
- xc_cull_box: 81.4k / 942 / 694 / 0.78;
- xc_cull_p2: 66.0k / 266 / 514 / 0.51.
- In the rain walk ME turn: 28.7k (front), 75.3k (pass 1) and 77.3k (pass 2) instructions. Fewer calls stop early there, and more are left out.

Comparison with v2:
- v2 had 452k test instructions against 620k removed: 2.91 ms of test, -1.38 net (rain wait ME turn). Those figures are from v2's ELF, which carried gl36's PGO in every object but its five recompiled ones.
- The gross removed is about the same on both ELFs: 4.3-5.0 ms (v2) and 4.1-5.4 ms (v3).
- So most of v3's gain over v2 is the test's own cost: -1.2 ms a step [C model].
- v3's net has not been measured on a PGO ELF: UNKNOWN.

### 6.1 Per tested vertex, and the share spent on kept calls

- Counts per window come from the twin run "win" (MGS2_CULL_WIN=10: the counters at each tenth vblank's frame start), as differences over the same 80 vblanks [C twin].
- The model's per-function cost is divided by them.
- The kept share allocates [I]:
  - the front by calls, (tested - left out) / tested;
  - pass 1 by vertices, the box vertices of calls kept / all box vertices;
  - pass 2 to no kept call, since no call failed pass 2.

Counts a step [C twin]:

| window | tested | left out (vertices) | kept at vertex 0 | no plane proven | vertices read (pass 1 box; of them in kept calls) |
|---|---|---|---|---|---|
| rain wait, ME turn | 77.0 | 48.5 (1,937) | 15.75 | 12.75 | 2,740 (2,724; 787) |
| rain wait, main turn | 77.0 | 48.5 (1,937) | 15.55 | 12.95 | 2,752 (2,737; 800) |
| rain walk, ME turn | 62.6 | 54.1 (2,273) | 2.59 | 5.75 | 2,515 (2,513; 240) |
| rain walk, main turn | 61.0 | 55.0 (2,276) | 0.42 | 5.58 | 2,505 (2,505; 229) |

The test's instructions [C model] divided by those counts [C twin]:

| window | per vertex read | front, a call | pass 1, a box vertex (bound included) | pass 2, a vertex | spent on kept calls [I] |
|---|---|---|---|---|---|
| rain wait, ME turn | 66.7 | 458 | 29.9 | 34.1 | 36.6k instr (20%), 0.38 ms (22%) |
| rain wait, main turn | 66.5 | 458 | 29.9 | 34.1 | 37.0k (20%), 0.38 ms (22%) |
| rain walk, ME turn | 72.1 | 458 | 30.0 | 34.0 | 11.1k (6%), 0.11 ms (7%) |
| rain walk, main turn | 72.2 | 462 | 30.0 | 34.0 | 9.6k (5%), 0.10 ms (6%) |

- The twin's counters equal the model on-run's own totals at that run's end (vb 31,200) in every place, so these counts are the model run's own [C twin, C emu]. Each window's counts are taken between the frame starts at its two ends.
- Fixed costs: the front costs about 460 instructions a call, and about 15 I-misses a call [C model]. Of those 460, about 300 are the matrix's 16 words [C code, disassembly].
- So in the rain wait about a fifth of the test is spent on calls it keeps: 37% of the calls (the front) and 29% of the box vertices. In the walk it is 5-7%.

## 7. PSP expectation [I]

The calibration is the same as v2's: on recent levers the model overrated ME savings 1.5-2.2 times. Noise is about ±0.5 ms on totals and ±0.1 on the ME.

**(a) Everything on the ME is overrated, the test too.** Net / 1.5..2.2:
- rain wait: -1.4..-2.0 (ME turn) and -1.1..-1.6 (main turn);
- rain walk: -1.7..-2.5 and -1.6..-2.4.

**(b) Only the removed work is overrated, and the test costs what the model says.** Gross / 1.5..2.2, minus the test:
- rain wait: -0.4..-1.4 and -0.2..-1.1;
- rain walk: -0.8..-1.9 and -0.7..-1.8.

Reading:
- v2's reading (b) ranged from a 1.0 ms loss to a 0.7 ms gain. v3's is a gain in every window.
- Most likely (a): about 1.1-2.0 ms a step saved on the ME in the rain wait, 1.6-2.5 in the walk.
- **Caveats:**
  - This is a no-PGO ELF. A trained build changes both the removed work and the test's I-misses (the test's code is untrained here).
  - In the rain walk ME turn, the model's main side (90.78 + 14.09 = 104.9) is already longer than the ME (102.3) [C model]. There, an ME saving may not shorten the step [I].
  - In the other windows the ME is the longer side by 5-19 ms [C model].
- **Main CPU:** within -0.07..-0.31 [C model], inside the noise. Expect no change.
- **How to settle it:** a PSP run with MGS2_NAT_CULL_CYCLE in turns. An 8-turn pattern such as 1,1,1,1,0,0,0,0 does not line up with REUSE_CYCLE=1,1,0,0.

## 8. What is left out, and cheaper still

- **Mode 1 (each triangle off with no shared plane).** Dropped, as allowed. Over the whole run it is at most the 4,761 calls v2 left out and v3 keeps [C twin]. In the rain windows it is UNKNOWN, but small: v2's census had 0.00-0.02 each-tri-off calls a step there.
- **Further cuts, not done** [I, each needs the full gate again]:
  - **Tiny-value check only where needed.** Run pass 2's tiny-value check only on axes whose box straddles 0; an axis with both ends of one sign has every value in range. That saves up to 15 of pass 2's 34 instructions a vertex, up to 0.17 ms a step in the walk. The share of boxes that straddle 0 is UNKNOWN.
  - **Cheaper matrix check.** The range check and load of 16 matrix words cost about 300 of the front's 458 instructions a call. A branch-free check or a reuse of the last call's matrix could save about 0.1 ms a step.
  - **Fewer I-misses.** The front misses about 15 times a call, about once per 64 B line: the conversion between two calls evicts it. Smaller code, or placing the test next to xc_call in a trained code order, would cut part of the test's 2,000-2,350 I-misses (0.45 ms a step).
  - **Out of scope, as in v2:** df0 cut groups, clipper-empty calls, one-point calls, P06 / P07 and c4.

## 9. Files

- Patch: /mnt/project-files/mgs2/main/attic/cull/cull_v3_vs_t36.diff. It is diff -ruN of /home/claude/t37d against /home/claude/t36 (8 files: ge/gelive.c, xconv.c, xconv.h, xme.c, xme.h; boot/nat_stub.h, xlive_chk.c; psp/xl_psp.c). boot_g0, rt and runtime are unchanged.
- Study folder: /home/claude/main/study_cull/v3.
  - Twin: twh/x86 (final), with logs out_<arm>.log.gz and r_<arm>.out.
  - Comparisons: cmp3.sh / cmp3.out.
  - The window counters: winc.py.
  - The PSP build: nopgo (pnp.sh, pnp.out).
  - The emulator: pk3 (pk3.md5), em3.sh, emu_<arm> (logs, refcmp.out; the pictures stay in the container), rdh_<arm>.out (digests against the twin), picmp.py.
  - The model: model/ (m3_off*, m3_on*, logs), fsplit3.py, tfn.py, pgo_m3_*.out.
  - The job lists: q.sh, q2.sh.

## Independent verification (2026-11-22 ~06:55 UTC, workflow wf_49b8a836-4bb: proof check, fuzz check, judge)

- Verdict: holds with caveats; no code change needed for exactness. Cleared for the gl36 retrain and the PSP measurement build in turns.
- Proof re-derived by two checkers [C]: forms, -0.0, key order (all 2^32 patterns checked by program), interval step for every coefficient sign, no underflow/overflow in range, fast path = clamped path in range, pass 2 (-4 rule, small values, ADC count, XcStats replay), nothing read before written. Margin: 28.4x the rounding error under round to nearest, 14.2x under the PSP's round toward zero [I, derived].
- Fuzz [C twin]: 17.2M converter calls through the real xc_fpos_full with and without the cull (round to nearest; round toward zero + FTZ + DAZ): FALSE OFF 0, STATE DIFF 0, RESULT DIFF 0. Edge-of-margin: 10.7M cases, 0 false. Misses first appear only at margins of 2^-23 / 2^-24 (the rounding error itself).
- Gates re-read [C twin, C emu]: 9 twin arms 685/685 = c9ddb593, frames 0 differing; extra arm MGS2_NAT_LW=2 + cull + check: DIFFERING 0; emulator 62/62, 916/916, 51/52 pictures (shot_24784 noise); model table reproduced exactly from the raw profiler files.
- Notes: the xc_fpos_off comment's "at least 28 times" holds only under round to nearest (14 on the PSP); the test's own sums cannot underflow either (multiples of 2^-102; S x 2^-16 >= 2^-72); the skip also skips vc_xform's sticky FCSR flags (their only reader is compiled out by -DVU_MCL_RZ); scratch poisoning covered xc_fpos_full only; em3.sh's own refcmp call had a wrong path (re-run by hand, results right).
- MUST for the run cfg: MGS2_OPT_CYCLE must parse as "from:turn" or the cull cycle is silently ignored.
- UNKNOWN until the PSP: exactness on the real Allegrex FPU (a PSP check-mode stretch settles it), which borderline calls the PSP skips (each still exact), the speed on a PGO ELF and on hardware, and whether the walk's ME saving shortens the step (main is the longer side there in the model).
