# Native drawing on the PSP: what it saves, what it costs, what stays (2026-11-01)

Task #124 (roadmap Now 6). Ffm asked (2026-11-01): 19:59 "are we applying all the optimization possible? or we got something for after?"; 20:06 "go ahead, at the end if needed we do more optimization, lets be organized"; 20:08 "can we apply the 3? going full psp native is the best choice?"; 20:22 "Rewriting the whole game natively isn't realistic, why?? also pros and cons"; 20:26 "so at the end the deck will be playable at 5 fps???". The answers in the thread (20:14, 20:24, 20:28) are summarised here with their numbers.

Labels: **[C]** measured, or read in the code; **[I]** inferred. No PSP build was made for this study.

Sources: the PSP run of gl11 (run 2, `prof/hw_gl11/run2/log.txt`, the all-on windows; the scratchpad's `g11parts.py`); the gl12 model (PspProf, 80 steps per window, the fit of renderer_notes §11.25; every function placed in a class by the scratchpad's `fnjson.py`, `ndsize.py`, `ndsteps.py`, `ndest.py`); the x86 twin's census of the deck window vb 30,800-30,899 (`/home/claude/sb8/x1/out_s11dig.log`, container only); renderer_notes §11.1, §11.3, §11.8, §11.20; `re/pipeline_hot.md` §1.7, §2.6-2.7, §4.4, §6; `re/dg_objects.md` §4; `re/mesh_packets.md` §1.2; `re/p07_builder.md`; `stage_notes.md`; `outlook_25fps.md`. No game data is quoted.

---

## Short answer

- **The picture is more than half of every step on the PSP [C]:** deck 80 of 154 ms, inside 59 of 96, codec calls 48-51 of 66-68. The exact cuts left inside today's method are small (gl12: about -2% in the model).
- **Native drawing is the biggest lever left for it.** The vertex maths on the GE (lever 1: hardware skinning and lighting) is one part of it, not a separate job.
- **Built on top of today's translator** (five steps, §3), it would take the deck from 153.6 to about 94-114 ms a step (3.3 to 4.4-5.3 fps), inside from 95.9 to 53-68 (5.2 to 7.4-9.4 fps), codec calls from 66.0 to 28-39 (7.6 to 13-18 fps) [I].
- **Rebuilt around the game's own objects** (§4), the picture could get down to about 6-15 ms a step on the deck [I], against 80 today. That is the larger prize, and the larger job.
- **What stays is the game's own code** (logic, the culls, sorting and matrices of its draw pipeline, sound): deck 68 ms a step, inside 34, codec calls 15 [C, gl11 less the builders]. With the planned logic work as well, my estimate is: **deck about 6-15 fps, inside 9-23, codec calls 14-25 [I]**, the top of each range only with the rebuilt renderer (§5).
- **So 25 fps is in reach in light scenes such as codec calls, and not on the rainy deck without lighter content there [I].** This is lower than the outlook of 2026-10-24 (deck 13-18.5 fps), because the picture's CPU cost turned out far larger than assumed then: about 160 ms per shown frame on the deck today against the 20-25 assumed (`outlook_25fps.md`, "What 25 fps needs").
- **Recommendation (§10):** make "Stage C: a native renderer built around the game's objects" the next big stage, one draw kind at a time, with today's renderer kept as the fallback and the picture reference; the texture pages first (exact, every area; revised 20:34, see §10), then the codec faces. The logic work alternates with it. Ffm chooses the order on the card.

---

## 1. Where a step goes now (PSP, gl11 run 2, everything on) [C]

ms a step. A step is one 50 Hz game step; every other step is shown, so 25 fps needs **20 ms a step** at full game speed, and the game runs slower whenever a step takes longer (it has no time step, speed_notes §16). fps here = 500 / step.

| Area (windows) | Step | Logic | Draw pipeline | IOP + sound | GS walk + translator | Picture | fps |
|---|---|---|---|---|---|---|---|
| Waiting on the aft deck (6) | 153.64 | 41.74 | 29.11 | 2.39 | 33.94 | 46.46 | 3.3 |
| Walking the aft deck (2) | 144.56 | 45.01 | 29.21 | 2.75 | 29.09 | 38.51 | 3.5 |
| Inside, 2 minutes (30) | 95.92 | 21.06 | 13.80 | 2.23 | 23.28 | 35.55 | 5.2 |
| Codec call 1 (22) | 66.00 | 9.36 | 5.79 | 2.42 | 19.39 | 29.04 | 7.6 |
| Codec call 2 (14) | 67.88 | 8.31 | 5.83 | 2.26 | 21.17 | 30.30 | 7.4 |

The draw pipeline is the game's own code under DG_ActLast (0x11da90). GS walk + translator and picture are the renderer's. The CPU never waits for the GE (GE wait 0.00 in all of them).

## 2. Where the renderer's time goes (model, gl12) [C model]

Each function of the gl12 model (the deck, inside and codec windows of gl11 and gl12's checks) in one class. ms a step; totals 144.8 / 92.5 / 88.0 (the model matches the PSP to about 2.5 ms per build, §11.25).

| Class | What | Deck | Inside | Codec |
|---|---|---|---|---|
| B: the game's packet builders | P120860, P12e8b8, P12d910, P13bf78 / P13bd00 (sea), P1214a0; P136f38 / P137730 (codec faces) | 5.69 | 3.09 | 2.86 |
| G: the per-call packet path | translator calls and VU memory bookkeeping (ud_new, call, ud_eager, spans, the lanes), GS front end, call hooks | 14.51 | 10.10 | 8.22 |
| Vs: skinning and lighting on the CPU | vc_p06_rec + xc_lit (P06, characters); vc_p07_rec + xc_lit (P07, codec faces) | 6.66 | 5.95 | 13.22 |
| Vc: static vertex copies | vc_df0_rec, vc_cflags, vc_strips | 2.89 | 3.04 | 0.29 |
| X: per-call setup | xc_call, vc_xform, xc_hold, xc_gs, xc_xform, xc_state | 10.79 | 7.98 | 3.93 |
| W: the DMA walk | io_dma_start | 8.09 | 4.81 | 4.80 |
| T: the texture path | the GS memory map (gsup2), texture cache and conversion, gif_qw (uploads) | 15.65 | 15.77 | 13.79 |
| L: libc | memcpy, memset, memcmp | 5.74 | 4.74 | 3.95 |
| S: stays | the game's other code, runtime, VU0, IOP; the effects' vertex conversion, frame and batches, GE list, timers | 74.79 | 37.01 | 36.96 |

- Per vertex today: P06 about 1.4-1.7 µs on the CPU (deck 6.66 ms a step over about 4.6 k vertices a step), P07 about 4 µs (13.22 ms a codec step over about 3.4 k vertices; 1 to 4 bones a vertex, three lights, normals renormalised) [C model + x86 census].
- The deck window draws about 870 GE batches per shown frame out of about 1,800 VU1 calls; P06 about 9,200 vertices per shown frame [C x86 census, vb 30,800-30,899].

## 3. Five steps on top of today's renderer [I]

Model ms a step saved, before the PSP's own spread. Each step is one change per measurement, as always.

| Step | What | Exact? | Deck | Inside | Codec |
|---|---|---|---|---|---|
| C1 | GE hardware skinning and lighting for P06 and P07 (the GE's 8 bone matrices and 4 lights fit both: P07 uses at most 8 matrices, abs 32-63; three directional lights + ambient) | No: rounding, see §7; needs Ffm's OK | 5.2-6.2 | 4.4-5.4 | 11.7-12.7 |
| C2 | Static vertex data kept in the GE's format (converted once per model, not every frame) | Yes | 2.0-2.6 | 2.1-2.7 | 0.2-0.3 |
| C3 | A native path per call for every drawn kind; the translator's per-call path and the VU memory model dropped (rebuilt on demand if a kind ever declines) | Yes | 11.9-19.5 | 8.5-13.9 | 6.1-9.8 |
| C4 | Textures by the game's own texture pages (`dg_objects.md` §4: 32 pages, each re-sent every frame it is used) instead of following every upload through the GS memory map; effects, fonts, render targets and the codec images stay on the map | Yes, if the texels are the same | 8.4-14.2 | 8.4-14.0 | 7.3-12.2 (refined 2026-11-01 21:16 for step 0, lookups only: deck ~5-6, inside ~6.5-7, codec ~4 [I]; `stagec_notes.md` step 0) |
| C5 | The game's packet builders skipped on shown steps, and the walk of their packets | The game's RAM changes (§8) | 10.3-14.1 | 6.0-8.4 | 5.7-8.0 |
| | **All five** | | **37.8-56.7** | **29.4-44.5** | **31.0-42.9** |
| | PSP step after all five (fps) | | 94-114 (4.4-5.3) | 53-68 (7.4-9.4) | 28-39 (13-18) |

The ranges come from the shares assumed for each class: Vc 70-90%, G 60-90%, X 30-60%, T 50-80%, W 50-90%, L 20-50%, and 0.5-1.5 ms of new set-up for C1 (bone matrices, lights). The PSP figures scale the model's picture savings by the PSP's own picture time (`ndest.py`).

**What is left of the picture after all five: deck 27-46 ms a step, inside 19-34, codec 13-24 [I].** It is the effects' vertex conversion, frame and batches, the GE list, the textures that are not pages, and the per-call set-up that the native path still needs.

## 4. A renderer rebuilt around the game's objects [I]

The other shape: on a shown step, walk the game's own draw lists (the DG objects and their parts, the texture pages, the sort pass's effect meshes; `re/pipeline_hot.md` §6 lists what each piece keeps on the CPU and what the GE can take) and write GE lists straight from them, with vertex data kept in the GE's format, GE skinning and lighting, textures by page, and one GE draw per mesh. No PS2 packet is built, walked or translated for a kind that has a native path; kinds without one go through today's renderer as now.

- **Its floor:** about 870 draws per shown frame on the deck at 5-10 µs each (today's GE list builder spends about 4.7 µs per batch, gx_batch), about 3.7 ms for the effects' vertices (what today's lane 3 converter costs per shown frame), 1-2 ms for page textures, 2-3 ms for the frame: about 11-17 ms per shown frame, **6-9 ms a step**; with margin for what is not known yet, **6-15 on the deck, 4-12 inside, 3-8 in codec calls** [I].
- **Its cost:** every draw kind needs its own path from the game's records: the codec faces (P07, `re/p07_builder.md`), the characters (P06), the opaque meshes by page (P1214a0 / df0), the effects (the sort pass), the sea (Bézier patches on the GE), the 2D and HUD, the radar and the rest. The records of the first four are already read by Stage B's lanes, so that work is not from zero.
- **What it keeps:** today's renderer, as the fallback for every kind without a native path and as the reference that each native kind's pictures are compared with.

## 5. What stays: the game's own code [C], and the end state [I]

Game side after native drawing (PSP, gl11, ms a step): deck 67.6 (logic 41.7, draw pipeline without its builders 23.4, IOP + sound 2.4), inside 34.0, codec call 14.7.

It is concentrated [C model, gl12]: on the deck, 30 game functions carry half of the game code's time, 110 carry 80%, 200 carry 90% and 326 carry 95%, of about 930 that run in the window at all (inside 26 / 100 / 193 / 310 of 754; codec 7 / 28 / 58 / 102 of 791). That is where native rewrites pay.

End state with the logic work too [I]. Assumed: logic cut to 34-52% on the deck (the outlook's levers without lighter rain: the new code generator, native effects, native guards and player), to 50-70% inside, to 75-90% in codec calls (the code generator only); the draw pipeline's culls, sort and box test (VU0 box cull 4.49 ms a step on the deck in the model) in native exact forms, to 50-70%.

| | Deck | Inside | Codec call |
|---|---|---|---|
| Now | 153.6 ms, 3.3 fps | 95.9, 5.2 | 66.0, 7.6 |
| Native drawing on today's renderer (§3) | 94-114, 4.4-5.3 | 53-68, 7.4-9.4 | 28-39, 13-18 |
| ... + the logic work | 55-87, 5.8-9.0 | 37-58, 8.6-13.5 | 24-37, 13.6-21 |
| Rebuilt renderer (§4) + the logic work | 34-56, 9-14.6 | 22-36, 13.7-22.6 | 14-21, 24-25 |

- 25 fps (20 ms a step) is in reach only where the game's own code is light, such as codec calls [I].
- On the rainy deck, the game's own code alone stays above 20 ms a step even after the logic work (about 28-40 [I]), so the game runs slower than normal there at any frame rate. Getting closer needs lighter content there (rain beyond half, a simpler sea, effects updated every other step, or a shorter draw distance), each Ffm's call with shots (`outlook_25fps.md`, "Not bit-exact").

## 6. GE time [C before, I now]

- Measured (renderer_notes §11.8-11.9, MGS2GE v4-v6, the deck frame of vb 24,050 before the rain was halved): hardware transform costs the GE nothing over through mode (-0.28 ms for 1,439 calls and 32,118 vertices); fog adds 0.47 ms; the frame takes 38.9 ms with the CPU idle and 41.8-45.1 ms with it busy.
- Not measured: the GE's cost for skinned and lit vertices (C1). P06 draws about 9,200 vertices per shown frame on the deck, P07 about 7,000 in a codec call (309 calls of about 22.5 vertices) [C census]. **First measurement of the plan: a GE probe in the next PSP build** (only after gl12's log is read): those calls drawn with GE skinning and lighting next to today's path, timed on the GE, with shots.
- The deck sits near the GE's 40 ms per frame. That does not bind today (the CPU is 3-4 times slower), but it would on the deck at 25 fps [I].

## 7. Pictures

- **C2, C3, C4 and C5 keep the picture exact:** the same vertices, texels and draws as today, byte for byte against today's shots, and checked the way the lanes were (x86 digests on three routes, PPSSPP 1.20.4 and master, then the PSP).
- **C1 changes it slightly [I]:** the GE skins, lights and renormalises in its own precision, so positions move by fractions of a pixel and colours by a level or two. The light formula maps onto the GE's (three directional lights + ambient, clamped; the PS2's 0x80 = 1.0 colour scale through the texture function's colour doubling); pass 3's texture coordinates come from the normal through the GE's texture matrix. To check: PPSSPP master pictures of both paths, then the GE probe's shots on the PSP, put in Ffm's folder for the OK (never in messages).

## 8. The game's state (exactness)

- C1-C4 do not touch the game: its state stays exact by construction, and every state hash stays as today.
- C5 and the rebuilt renderer stop the game writing its packet bytes on shown steps, so the game's RAM differs from today's build there. The rules from renderer_notes §11.20 (option 1, "against"): a from-boot read check showing the game never reads the unwritten bytes (the method of speed_notes §34, which already proved it for P1214a0 on unshown steps); the packet pointer still advanced by the original size (P1214a0 drops groups when the buffer would overflow); P12d910's return value, which its caller compares (0x121d5c); the chain still walkable, with its FINISH / SIGNAL in place; then new hash references, made once and checked on x86, PPSSPP and the PSP as today.
- The reduced pipeline already skips some builders on unshown steps exactly, which is half of all steps [C]; C5 extends that to shown steps.

## 9. Why not rewrite the whole game (Ffm 20:22)

- **Size [C]:** the game is about 13,500 distinct functions (about 5.9 MB of PS2 code, from `stage_notes.md`'s survey of all 123 stages' code images; each stage carries its own relinked copy, 1-3 MB) plus the main program's 128 k instructions, with no source.
- **Fidelity:** a rewrite drifts (float rounding, the order random numbers are drawn), and those bugs show up hours in; with no exact build to compare against, 99% fidelity could not be shown.
- **Little gain for most of it [C]:** about 930 functions run in the deck window, and 200 of them carry 90% of the game code's time (§5).
- Pros of a whole rewrite: the most speed possible, less memory, freedom to redesign for the PSP. Cons: by far the most work, nothing playable until most of it is done, fidelity that cannot be proven, and a real risk of never finishing.
- So: the exact recompile for everything, native code where the time goes (the renderer, the draw pipeline's culls and sorting, effects, guards and player), each part checked against the exact build.

## 10. Recommended order

1. **Stage C, a native renderer built around the game's objects, one kind at a time** (§4), with today's renderer as fallback and reference. Order by gain, exactness and how well the records are known (revised 2026-11-01 20:34: the texture pages moved to the front):
   0. **the texture pages (C4) first:** exact, in every area (8-14 model ms a step [I]; refined 21:16: deck ~5-6, inside ~6.5-7, codec ~4 [I] for the lookups by upload identity, design in `stagec_notes.md` step 0, with the page lane below as a later trim of about 1 ms), and the converter is already checked: on the deck 318 of the 322 textures a frame are page textures, and `ge/texconv.c` made all 318 from the game's page records with 0 of 16.3 M texels different (renderer_notes §9, "Textures in detail"). The walk names the page at its upload (the CALL into the page entry, `mesh_packets.md` §1.2) instead of following 3.7 MB of uploads a frame through the GS memory map; the few other textures (the font, the frame-buffer textures, the codec images) stay on the map;
   1. the codec faces (P07): the largest share of a codec step, records known (`re/p07_builder.md`), streams static, bones double-buffered in the object; first with today's exact CPU skinning (pictures byte for byte), then C1's GE skinning and lighting behind a switch for Ffm's OK;
   2. the characters (P06), the same way;
   3. the opaque meshes (P1214a0 / df0), on the page textures of step 0;
   4. the effects (the sort pass), keeping the game's draw order;
   5. the sea, the 2D and HUD, the radar, the rest;
   6. the builders skipped on shown steps (§8) once their kinds are all native.
2. **The logic work alternates with it:** the code generator's steps 4-5, and native exact forms of the box cull and the draw pipeline's culls and sort.
3. **First steps, no PSP build:** a design note for Stage C (each kind's records, its GE draws, its fallback, its checks); then the texture pages on x86 (every texture use compared with today's map, on the three routes), then the codec faces with PPSSPP master pictures. The GE probe (§6) goes into the next PSP build, after gl12's log is read.

The alternatives on the card: logic first and the renderer after; or the five steps of §3 on today's renderer, which is less work per step but leaves the picture at 27-46 ms a step on the deck.

## 11. Corrections to what I said today

- 20:03: "about 63 ms" for drawing straight from the game's data counted the whole draw pipeline. Only its packet building goes (about 5.7 ms a deck step in the model); its culls, sorting and matrices stay because the game reads them (`re/pipeline_hot.md` §2.6).
- 20:14: "codec calls could get near 25 fps with native drawing" was too optimistic: native drawing alone gives about 13-18 fps there [I]; near 25 needs the rebuilt renderer and the logic work too (§5). The sentence is struck through in that message.
