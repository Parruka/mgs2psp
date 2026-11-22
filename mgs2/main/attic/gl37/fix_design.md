# gl37 fix design: the native ship group's FIXED work (plan step 4), 2026-11-22

Build agent, written before any lever code (the plan's checkpoint). Tree /home/claude/t37 (= t36 + the reuse rule of plan step 3, no lever code).
Marks: [C] measured or read in code, [C model] the PspProf cost model, [I] inferred (basis named), UNKNOWN.

## 0. Sources and units

- PSP stage timers: prof/hw_gl36/natsplit_summary.md (plain rain turn, us a timed group; each stage holds one ~0.46 us clock read) [C PSP].
- Model: build_gl36/model/g36p (the delivered ELF 9c243b19, levers off) minus build_gl37/model/b36n4 (the same ELF, groups not drawn),
  rain wait ME turn 29,810-29,890, 40 pictures, 609 groups a picture: per function build_gl37/split_wait_me.txt, per source line
  build_gl37/pcl_*.txt (objects rebuilt with -g, .text identical to the linked ones) [C model]. b36p (a fresh run of g36p's arm) is
  byte-equal to g36p in all four windows: the model is deterministic [C].
- A native ship group costs 105.3 us model in that window (12,032 instructions, 93.0 I-misses, 103.4 D-misses), of which the ME's own
  native path (walk, front end, GS model, converter, per vertex) is ~69 us [C model].
- UNITS: the model scripts' "ms a step" (mtot35.py, pairmean36.py) are per PICTURE (a window's total / 40; 80 vblanks). The PSP's
  steps.csv has one step a vblank, and natsplit_summary's "native ms/step d" column is (us a group x groups a turn) / 100, per
  VBLANK [C, code and file]. So the gl36 reuse's "PSP -1.6 against model -3.1" compares a vblank with a picture; per group the two
  agree (PSP -5.4 us a group on the timers, model -4.7 us a group, build_gl36/model_lever.out) [C]. Below: us a group (model and
  PSP-timer terms side by side) and ms a picture [C model]; the plan's "x 0.5" is applied only where a step time is meant.

## 1. Where the fixed work goes (per group, rain wait ME turn)

| stage (PSP timer, gl36 plain) | what it does | model us a group [C model] |
|---|---|---|
| walk + pointers 5.6 | merge by key, the record into XlP04, 5 pointers by hint (lw_walk, nat_p04_group) | lw_walk 6.4 |
| register 9.2 | form tests, counters, block (24 qw, q0) + material (8) + header copied into the VU model, 3 stream descriptors + own[] marks, XlRec | xl_p04_fast lines 800-873 9.9 |
| A+D 6.4 | material packet: 5-7 GS register writes (F.gr + gr), TEX0/TEX2 CLUT tracking | xl_p04_fast 874-905 2.5 + gu_clut_load 2.1 + rows 0.6 + clut_new 0.2 + gf_reg 0.5 = 5.9 |
| host call + GS state 5.5 | xm_call -> gl_call (frame bookkeeping, hold test, gb_room) -> xc_call -> xc_gs (decode of ~35 GS fields) | 0.7 + 2.1 + 0.35 + ~1.0 + 1.8 + 0.5 = ~6.5 |
| texture 8.8 (drawn groups only: ~19 a drawn group) | gl_tex memo -> tk_get_um front -> um_hit -> gu_clut_id -> tk_pal, two LRU touches, GerTex, add_tex | 1.2 + 2.3 + 0.9 + 0.45 + 0.7 + 0.4 + 0.2 = ~6.2 |
| way back 2.5, into the frame 0.9 | returns through the layers; gb_xform_in | ~0.8 |
| drawn areas 1.9 | K1's replay of the last group's drawn areas (gu_drawn_again) | 1.6 |
| depth range 1.5 | xc_zr_df0d: the group's z range into the frame | 1.6 |
| walk's end 1.1 | the walk loop, the transfer-end decode (vm_all) share | ~0.5 |
| **sum ~43** | | **~36 (+ transform 2.5, cut test 3.4: not in the plan's fixed list)** |

Why it costs what it costs [C model]: the ME's per-group path touches ~250 distinct 64-byte code lines (~16 KB: xl_p04_fast 57,
lw_walk 30, xc_call 24, xc_zr 15, xc_off 13, gu_clut_load 12, vc_df0_pos_k 11, gl_call 8, tk_get_um 8, ...), about the ME's 16 KB
instruction cache, and misses ~93 lines a group (19 us model, 18% of the group). D-misses 103 a group (27 us model): the VU model's
copies (5.3), the stream reads, the texture cache's entries and stamps, the counters' lines.

## 2. The texture stage, and why gl35's texture keep cost +0.8

- [C, gl35 report 3.2 + code] gl35's K3 replayed a look-up that took the cache's "pure path" (front hit, um_hit, palette by hint).
  The replay must redo every effect of that path to keep the cache's state exact (uclk, ufm, two use stamps, eight counters, two LRU
  touches, gu_clut_id CALLED for real as the guard of the palette, the frame's texture table), so it is nearly as long as the path
  itself (model: tk_get_um -1.41 and tk_pal -0.23 ms a picture saved, but gl_tex +1.08, tk_keep_again +0.79, tk_keep_ok +0.53).
- [C, gl35 counts] For the ship's own look-ups (bit 4) it hardly fired: 36,522 replays on the twin against 139,819 records; its
  replays came from other callers' look-ups (bit 8). Each material has its own CLUT (TEX2 CLD 4 with another CBP in five groups of
  six), so the gl_tex memo (gen_of moves with every draw and every CLUT load) hits 70 of 263 drawn look-ups a picture [C model calls].
- [C model] Its new code added 3,400-6,700 I-misses a picture in the rain (placement), which is where the loss came from.
- What is skippable exactly in the texture stage now: building GerTex only when add_tex inserts (it is read only by gb_tex on an
  insert) and gen_of read once: about -0.3 us model. The rest is state (stamps, counters, LRU) or a guard: not skippable.

## 3. What is skippable exactly, stage by stage, and the estimate

F1, register: the last-writer skip of gl36's R-a for EVERY run that is not replayed, the S set from a pre-pass over the run's records
(no record, no look-up).
  - The walk has a run's records before it feeds them (as xl_nr_begin does). A backward pass per buffer (the two VU buffers alternate
    with each group's MSCAL) keeps: a later q0 group seen (covers T+4..0x13, 0x16, 0x17), any later group seen (covers the material
    T+0xe8..0xef), and the largest later stream count per stream (the streams fill T+0x100+s+3k, k < num[s], prefixes). A group is in
    S when it writes a quadword no later group of the run writes: exactly the set gl36's notes give (nr_seg) for a run with no barrier.
  - A group outside S takes xl_p04_fast's existing XR_SKIP branch (matrix, tag template, header, counts and XlRec as today). Exact by
    gl36's R-a argument: each skipped quadword is written again later in the run by its last writer, and nothing reads it in between
    (the df0 converter reads T+0..3, 0x14, 0x15, abs 2..5; the A+D kick reads the source). Taken only when every group of the run has
    the fast form from its record (MSCAL 0x1be, NATG_FAST, three streams, counts and regions in range), the VU buffers in range at the
    run's start and VPsrc[0x1be] = df0; a group that still declines at run time (tables full, path 1 busy) goes to xl_p04_core, which
    writes the same quadwords: exact (gl36's [17] case). Off when xl_nr_ok is 0 (the twin's whole-VM checks). Check mode: the plan
    computed and NOT applied, the run fed in full with the notes, and the two S sets compared (a group the plan skips that the notes
    put in S: DIFFERING).
  - Estimate [C model]: the skippable lines cost 5.6 us model a group (copy loop 2.5, own clears 0.6, descriptor fills 1.4, own marks
    0.8, less the matrix / header copies that stay); gl36's model put R-a at -3.8 us a group in the wait's ME turn with 80% of the
    groups skipping (model_lever.out), i.e. -4.75 a skipping group; 92% of a run's groups skip [C twin: 1,310,024 of 1,421,319].
    Pre-pass ~+0.3 [I: ~30 instructions a group]. F1 = about -4.1 us model a group. PSP timers: gl36's register stage went 9.2 -> 5.7
    in reuse turns (87% replayed) [C PSP], i.e. about -4.4 a skipping group: about -3.8 us a group on the PSP's timers [I].
F2, A+D: TEX0's gu_clut_load call left out when the format is not indexed or CLD is 0 (gu_clut_load returns at once then [C, code];
  gl35's K2 test made unconditional for fast groups): -0.15 [I]. The CLUT load itself is already kept by parameters (ld_now); its
  rows and counters are state. Lazy CLUT loads for groups the cut then leaves out would need settle()'s side effects reordered: not exact
  by construction, left out.
F3, host call + GS state: xc_gs's decode of the registers the material packet does not write (scissor, offset, zbuf, test, frame,
  fog...) kept while xl_gs_gen is unchanged (it moves with every other GS write [C, xlate.c 892]); gl_call's effect-thinning tests
  skipped for the df0 entry (they test FX/FX2/LINE/SPR entries only [C, code]): -1.0..-1.4 us model [I]. xm_call's counters and
  timing, gl_call's frame state, gb_room and the hold test stay.
F4, texture: section 2: -0.3 [I].
F5, walk's end: fewer descriptors from F1 to decode at the transfer's end: -0.2..-0.5 [I].
Walk + pointers, way back, drawn areas (already K1), depth range, into the frame: nothing exactly skippable found (each is either
  state the hashes see or a camera-dependent result).

| part | model us a group | PSP-timer us a group [I] |
|---|---|---|
| F1 register (last-writer skip, pre-pass) | -4.1 [C model basis] | about -3.8 |
| F2 A+D | -0.15 | ~-0.1 |
| F3 host call + GS state | -1.0..-1.4 | ~-0.7..-1.2 |
| F4 texture | -0.3 | ~-0.3 |
| F5 walk's end | -0.2..-0.5 | ~-0.2..-0.4 |
| **all** | **-5.8..-6.5** | **about -5.1..-5.8** |

Against the fixed work: -5.8..-6.5 of ~36 us model (-16..-18%) or of the ~51 us model the same functions cost with the transform and
the cut test (-11..-13%); on the PSP's timers about -5.1..-5.8 of 43 (-12..-13.5%). In step terms: 609 groups a picture, so about
-3.5..-4.0 ms a picture [C model estimate] (-1.8..-2.0 ms a vblank), of which F1 is two thirds.

RISK on top: gl35's keeps showed that new branches in this path cost I-misses by placement (+5.6..11 a group in the rain, model);
F2-F5 are each a few hundred model ns and could net to zero that way [C model history].

## 4. Conclusion

THE DESIGN CANNOT BE EXPECTED TO REACH -15% OF THE FIXED WORK MEASURED THE WAY THE PLAN MEASURES IT (the PSP's 43 us): best estimate
-12..-13.5% [I], with F1 the only part over 1 us a group; on the model's own fixed sum it reads -16..-18% only if the transform and
the cut test are left out of "fixed". The -30% target (-13 us a group) is not reachable by exact skips: what remains after F1 is
instruction-cache misses (~93 lines a group on a ~16 KB path) and state work (stamps, counters, GS model), not repeated work.
Per the plan (step 4: "If the design says it cannot reach -15%, report that and stop before building") the lever is NOT built.

What could reach further (each its own design build; not started):
- (A) F1 alone as a small lever: exact by gl36's own proof, about -4 us model a group (-2.5 ms a picture [C model estimate]) in every
  scene with ship groups, walk included; the reuse's replays then add only the kept boxes.
- (B) one fused df0 path for fast groups (gl_call + xc_call + xc_gs + the texture's front hit in one function, the generic path kept for
  the rest), so the per-group path's code fits the ME's I-cache: about -4..-6 us model a group [I, from the I-misses of part 8],
  exact by construction (the same statements) but a large second copy of the converter's front; check by arms on the twin.
- (C) a run's groups stage by stage (all front ends' pure parts, then all converters): I-misses -50..-70% [I], but the order of
  effects changes, so exactness needs a dependency proof per stage; the largest gain and the largest risk.
