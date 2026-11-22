# Codec speed: where the time goes and the plan (task #85, 2026-10-28)

Ffm 22:24: "can we optimize the codec?". Numbers only, no game data. [C] measured, [I] inferred.

## Where the time goes

- **PSP, gl4c (faces not drawn), vb 31,500-31,600 inside an Otacon call [C]:** 75.9 ms a step = GS walk 34.5 + draw pipeline 28.9 + logic 8.1 + IOP and sound 2.3 + picture 2.1 (`prof/hw_gl4c/run3/log.txt`).
- **Model, gl5 (faces drawn), the same vblanks [C]:** PspProf vb 31,510-31,590, 80 steps, whole step (cfg `prof 31510 31590 all`, PPSSPP_VFPU_RN=1): **132.7 ms a step** = 54.2 instructions + 49.8 I-cache misses + 28.7 D-cache misses (125 cycles a miss, `tools/psp_prof.py`). The model has read about 7% low on the PSP, so about 140 ms there [I]: the faces nearly double the cost of a codec step.
- **PSP, gl5 (faces drawn), the same window [C, gl5 run 1, 2026-10-30, `prof/hw_gl5/run1/log.txt` window vb 31,600]:** **132.19 ms a step** = picture 49.67 + GS walk 42.04 + draw pipeline 28.77 + logic 8.95 + IOP and sound 2.76. So the model was right within 0.4% here (the "about 140" above was wrong) and the faces cost 56.3 ms a step on the PSP (picture +47.6, GS walk +7.7, logic +0.9). All call windows of vb 34,001-40,000: 130.26 (`psp_harness.md` "Build gl5", Run 1).
- **By part (model ms a step):**

| Part | Total | Instructions | I-misses | D-misses |
|---|---|---|---|---|
| Game code (logic and draw pipeline) | 34.2 | 12.7 | 11.4 | 10.1 |
| Converter and GE list (the faces' calls) | 28.4 | 8.8 | 14.0 | 5.6 |
| Texture lookups | 24.4 | 14.1 | 8.2 | 2.1 |
| GS walk and translator front | 21.1 | 6.9 | 9.7 | 4.6 |
| VU data unpack | 12.1 | 6.4 | 2.0 | 3.7 |
| Other (kernel poll, IOP, sound, libc) | 12.5 | 5.2 | 4.5 | 2.8 |

- **Per step [C]:** 161 converter calls (154.5 of them P07), 362 texture asks (gl_tex), 320 texture-cache lookups (tk_get), 12 uploads (24 a frame), 7,042 unpack runs by the generic decoder (ud_any).
- **Texture lookups are repeated work [C]:** 3.39 M of 3.40 M cache uses hit (1,289 conversions in the whole run); the cost is working out each texture's identity again: block_id 5.5 ms, state_id 4.6, tk_get 3.3, gu_area_id 3.1, gu_clut_id 2.7. Each lookup walks about 24 blocks and 70 block states (7,671 block_id and 22,383 state_id a step). Why: the game uploads the faces' 24 textures (about 1.8 MB) again every frame with the same bytes; each upload marks its pages written, so no area identity stays kept.
- **Code fetches [C model]:** 133 k I-cache misses a step. The picture and walk code alone misses about 90 k lines a step, about 580 lines (37 KB) per face call, against a 16 KB I-cache: the call's path (translator, unpack, converter, texture lookup, GE list) is fetched again on every call. Examples: xc_call 7.3 k misses a step from 54 lines, tk_get 5.8 k from 29, vc_p07 8.4 k from 224. At the PSP's measured 218 ns a line (psp_harness.md probe 12) these are about 29 ms; the model's weight says 50. The PSP run of gl5 tells which.
- **Game code [C]:** P11fa58 4.1 ms, vupv_660a267e_0 3.9, P120538 2.4, P136f38 2.0, rt_qcopy_nov 1.7.

## The plan: exact changes, one per measurement

Each keeps the game's hashes and the frame digests equal on x86, PPSSPP and the PSP. Savings are model ms a step and inferred until measured. Steps 1 and 2 are done on the PC side: "Results 2026-10-30" below.

| # | Change | Saving [I] |
|---|---|---|
| 1 | **Texture lookups.** An upload that puts the same bytes from the same EE source into the same rectangle as the record that already holds all of its blocks leaves the map as it is (no new record, no page written), so the area identities stay kept. Then: the CLUT identity from the rows' words instead of 32 64-bit mixes; a small memo before tk_get for repeated TEX0 / CLAMP / TEXA while nothing was written (keeping the cache's use order, so evictions stay the same). | 15-20 |
| 2 | **Unpack fast kinds for P07's formats**, as task #70 did for P06 (census first: `MGS2_XLIVE_VCEN=1` on the x86 twin). | 5-8 |
| 3 | **Per-call overhead:** xc_call / xc_pass / gl_call path, the double to float conversion per call (soft-float), memsets. | 3-5 |
| 4 | **Code fetches** (moved into Stage B, 2026-10-30: see the last section)**:** rare paths out of line and the per-call functions placed together; then group a frame's face calls so each part's code stays hot (the pure maths in a batch, while texture lookups and the GE list keep the GS's order; renderer_notes 11.16 "Next levers"). | 10-25 |
| 5 | **GS walk:** one VIF decode for the exact walk and the translator front (io_flush and xl_vif1_span parse the same words), and the CLUT-load hashing (gu_clut_load 1.8, whash 1.5). | 4-6 |
| 6 | **Game code:** native exact forms of P11fa58, P120538, P136f38 and VU0 program 660a267e (as P12dbc0, task #2). | 5-8 |
| 7 | **Measure first:** how many P07 calls repeat the last frame's inputs exactly; a repeat could reuse the converted vertices (talking faces move, so likely few). | ? |

Together about 45-70 ms: a codec step with faces at about 65-90 ms model [I], about what gl4c costs without faces. The changes to the call path (1, 3, 4) also help the deck's guards (P06), which use the same path.

## Not exact (would need Ffm's OK; not proposed now)

- The GE's own skinning and lighting for P07 (bone weights, three lights): the CPU would no longer transform the faces, but the GE's float maths differs from the VU's in the last bits, so vertices and colours could move slightly.
- Faces drawn at 15 fps on the codec screen.

## Checks for each change

- x86 twin (`tools/dev/gl1_x86.sh`): "picture:" lines and frame digests equal gl5's; the game's hashes.
- PPSSPP (`GL3=... sh psp/gl3.sh emu codec`): 205 digests e5535d8d57747651, hashes equal hashref; PspProf at vb 31,510-31,590 (PPSSPP_VFPU_RN=1; prof_run.sh does not set it) for the saving.

## Step 1 design (2026-10-28 23:10; measured 2026-10-30: exact but it rarely applies, so not built; see Results)

**Rule.** In `ge/gsup2.c` `gu_image`, when a host -> local transfer's data is all in and it is about to go into the blocks: if it writes whole blocks only and every block of its rectangle, in every lane group it writes, already holds one record y (state `S_REC(y)`, not a list) with the same rectangle, lanes, source, size and final identity as it, the transfer is marked in but leaves the blocks as they are: no `cover`, no `seq++`, no page write count, counted in GuStats `elided` (the field is still in gsup.h from form 1). Area identities (AcE) and CLUT loads then stay kept.

**Why it is exact [reasoned, to check]:** a normal write would leave `S_REC(x)` where `S_REC(y)` was, with x's id equal to y's, so every state id (a mix of record ids), every word (`entry` reads the same EE source through the same rectangle) and every flag is the same; `told` would do nothing (a drawn area's mark in x's lanes comes only with a drawn record's put, which would have replaced `S_REC(y)`); `pdt` only narrows in a normal write (a superset is harmless); a CLUT load made again after a normal write gets the same id (state ids equal), so palette ids are equal; a kept area identity is the value a remake gives. So identities, texture-cache keys, use order, GE lists and digests stay the same; only the map's own counters change.

**Not covered (normal path):** transfers with a partial block (form 1 of the lean map skipped repeat uploads and missed one case there: a skipped re-upload kept its place under a neighbour sharing a block edge; renderer_notes 11.12 "The lean texture map"), data in pieces or after an early commit (epoch), local copies, bad transfers.

**Measure first on the x86 twin** (codec window, vb 30,600-31,400; `/home/claude/codec68/x84r/env.txt` is the codec route's env): transfers that qualify, `areas_kept` / `areas_made`, `early` and epoch changes per frame (an epoch change empties every kept area identity). Then the same counts with the rule, and the checks above.

**Then 1b and 1c:** the CLUT identity (`gu_clut_id`, 32 64-bit mixes a lookup) from a version of the CLUT rows that moves only when a row word changes; a memo before `tk_get` while no write happened (keeping `touch` order).

## Results 2026-10-30 (steps 1 and 2 on the PC side; no PSP build yet)

Four exact changes, each a switch in g_opt, on by default. Model: PspProf vb 31,510-31,590, 80 steps, whole step, PPSSPP_VFPU_RN=1, each change added to the one before. Every build below gave the 205 frame digests e5535d8d57747651 in PPSSPP.

| Build | Model ms a step | M instructions | k I-misses | k D-misses |
|---|---|---|---|---|
| gl5 | 132.5 | 18.00 | 132.5 | 76.4 |
| + A, area memo | 123.9 | 15.34 | 131.6 | 75.8 |
| + B, flags-only render-target test | 121.9 | 14.68 | 129.8 | 77.4 |
| + C, palette memo | 119.7 | 14.09 | 128.9 | 77.3 |
| + D, V4-8 decode (step 2) | 115.3 | 12.58 | 128.2 | 78.2 |
| gl6c: A-D in the shared code, all on | 115.6 | 12.61 | 131.7 | 75.3 |

-17 ms a step (-13%) and -30% instructions. By file: the texture map (`ge/gsup2.c`) 22.9 -> 11.5 ms (5.14 -> 1.22 M instructions), the translator (`ge/xlate.c`) 23.7 -> 18.6 (3.60 -> 2.10 M). The I-misses barely moved; they are step 4's.

- **Step 1 as designed is exact but rarely applies [C, x86 twin]:** only 8,039 of 320,484 uploads on the route qualify (2.5%). The codec streams about 22 uploads a frame (about 1.9 MB) through the same place (bp 0x2000), each replacing the one before, so "the same record is already there" is rare. Not built further; A-C replace it.
- **A, area memo** (`gu_area_memo`, OPT_GU_MEMO 0x10000): an area identity made again walks a block's states only where they differ from the block's before it (a block's identity is a function of its three state words). On the route 60.3 M cells, 4.9% walked.
- **B, flags-only render-target test** (`gu_area_flags` + `tk_flags`, OPT_TK_FLAGS 0x20000): tk_get only needs the flags to tell a texture not converted from a render target; gu_area_flags visits each state word once, a whole page at a time where the area covers it. 11,623 calls, all whole pages.
- **C, palette memo** (`gu_clut_memo`, OPT_GU_CLUT 0x40000): the identity of CLUT entries 0..255 that one load put in place in order (a whole load at CSA 0) is a function of that load alone, so it is kept with the load for its CPSM (a new load starts empty; the rows change only in `rows()`, which also moves the mark). 3,326,574 asks, 2,604,009 answered from the memo (78%). It replaces the plan's 1b.
- **D, V4-8 decode** (`xl_ud_v48`, OPT_XL_V48 0x80000): V4-8 unsigned UNPACKs in MODE 0 without a mask (one a P07 call) get their own decode kind (a word's four bytes to the four lanes); ud_any's 1.54 M instructions a step are gone.
- **1c not built:** every lookup follows uploads into the same pages, and the area memo already keeps an identity per page version, so a memo before tk_get would rarely hit [I].
- **"_ftext" in earlier profiles is io_flush** (`runtime/ee_io.h`, the DMA start with the chain walk inlined, 10,460 bytes): about 8 ms a step. `tools/psp_prof.py` now names it.

**Checks [C].** x86 twin: with the switches on, off (`MGS2_XLIVE_GUMEMO`, `_TKFLAGS`, `_GUCLUT`, `_V48` =0) and checked (`-DGU_MEMO_CHECK`), every "picture:" line, frame digest and texture total equals gl5's; the checked build compared 426,855 area identities, 11,623 area flag answers and 2,602,305 kept CLUT identities with the plain walks, 0 differing; D gave the same vectors as ud_any in 56,054,152 of 56,054,152. The dual-map twin (`boot/gudual.c`: the reference map gsup.c beside the lean one, whole route), with A-D on and with them off: area and CLUT identities change exactly when the reference's do (with A-D off 3,338,377 area and 3,326,574 CLUT asks, 0 changed in one map only), and the same 2,136 area flag answers differ from the reference's either way (the reference says lost, the lean map drawn: the CT16 page at bp 0x2000 that the codec streams through, 1024 x 1024, and T4 areas at bp 0x25eb), so that difference is the two maps' own, not A-D's; it bears on #83. PPSSPP: gl6c with the four switches taking turns every 100 vblanks from vb 30,600 (`MGS2_OPT_CYCLE=30600:100:ffffffff,fffeffff,fffdffff,fffbffff,fff7ffff,fff0ffff`) gave the 205 digests e5535d8d57747651 and no split over hashref's 907 lines (to vb 31,400).

**Profile runs split at their stop, from the profiler itself [C].** Every PspProf run of this window, gl5's own of 2026-10-28 included, splits at its stop, vb 31,600: ee differs (a different value in each build), iop and vfb equal. With a hash every vblank from vb 31,500, gl6c's JIT run to vb 31,600 equals the reference there, while its profile run differs in ee from vb 31,511, the window's first step, to the end, with iop and vfb equal throughout. This is the known one: PPSSPP's interpreter runs only the profiled steps and is not exact (renderer_notes 11.16, gl4: 1,753 words differing, each a float's last bit; `psp/gl3.sh` NOPROF). Every build has the same window, so the deltas above stand [I].

**What is left (model ms a step, A-D on):** vc_p07 8.3, io_flush 8.0, vm_all 6.4, xl_vif1_span 4.7, P11fa58 4.2, xc_call 4.1, VU0 660a267e 4.0, gu_area_id 3.3, tk_get 3.1, gif_qw 2.8, P120538 2.5. The lookups left (gu_area_id, tk_get, gu_clut_load, gl_tex) are mostly I-misses now (gu_area_id: 228 k instructions, 5.3 k I-misses a step), which is step 4 (code layout). whash (1.5) hashes the sources that change and is needed.

**On the PSP [C, gl6 run 1, 2026-10-30, `prof/hw_gl6/run1/`, `psp_harness.md` "Build gl6"]:** gl6 = gl5 + A-D taking turns every 100 vblanks from vb 1,000; exact (hashes, digests, shots). Each setting's call windows (vb 34,100-39,500, reference picture over 9 ms a step) against gl5's run 1 on the same windows:

| Setting | Windows | gl6 ms a step | gl5, same windows | Saving |
|---|---|---|---|---|
| A-D on | 8 | 108.75 | 132.28 | 17.8% |
| A off | 7 | 110.76 | 132.14 | 16.2% |
| B off | 8 | 111.22 | 131.39 | 15.4% |
| C off | 9 | 108.40 | 128.58 | 15.7% |
| D off | 7 | 115.36 | 132.09 | 12.7% |
| all four off | 7 | 133.37 | 132.28 | -0.8% |

- All on: 108.75 = logic 9.04 + draw pipeline 29.13 + IOP 2.70 + GS walk 28.38 + picture 39.50 (gl5, all its call windows: 131.35 = 8.87 + 28.60 + 2.63 + 42.06 + 49.19). The model said -13% (132.5 -> 115.6); the PSP gives -17.8%, from the GS walk (-13.7 ms) and the picture (-10.1).
- Each change's own part (its saving with the other three on): A 2.2 ms, B 3.4, C 3.4, D 6.8. Together 23.5, more than the sum (15.7): A-C share the lookups' work [I].
- All four off is 1.1 ms (0.8%) slower than gl5 in each of its 7 windows (picture +0.95): the switches' tests and the new code layout [I].
- A codec frame is two steps. With A-D on: the light step 53.4 ms (logic 9.8, draw 29.1, walk 5.2, picture 6.6), the frame step 164.1 (logic 8.3, draw 29.1, walk 51.6, picture 72.4; gl5 211.4).
- Between calls (vb 35,611-35,970, nothing drawn: picture 0.1 ms a step): gl5 19.2 ms a step, of which GS walk 14.0; gl6 with A-D on 10.1.

## Ffm 14:59: "its possible to simplify the codec? only the faces text and the PTT moves" (2026-10-30)

With the picture identical to the original (Ffm 15:00). [C] measured, [I] inferred.

- **What changes from frame to frame [C, the x86 twin's replay files of 8 frames in a row, vb 16,991-17,005, story call 1, `reuse.py` (session scratchpad, on `tools/dev/ger_cmp.py`)]:** a codec frame is 706 lit draws (the faces, P07 passes: 492 KB of draw data) and 293 GS primitives (panel, frames, PTT bars, text, background, glow and overlays: 13 KB). None of the 706 face draws equals one of the frame before (0 of 492 KB); 89 of the 293 GS primitives do (4 of 13 KB). Ffm is right that the panel stays still; the faces change every frame, so their work cannot be kept from one frame to the next.
- **What the still parts cost [C]:** gl4c drew the codec screen without the faces (P07 not converted yet): its picture work was 2.1 ms a step (vb 31,500-31,600). Drawing only what moves would save at most about that, 2 of gl6's 108.75 ms a step.
- **What the faces cost:** their picture work is about 37.4 of gl6's 39.5 ms a step [I: 39.5 less gl4c's 2.1]; they are 98% of the draw data, so most of the GS walk (28.4 ms a step) and of the game's draw code (29.1) is theirs too [I].
- **So the exact ways down are the faces' own:** step 4 (code layout and grouped face calls: a face call runs about 37 KB of code through the 16 KB I-cache; model 10-25 ms), then steps 3, 5-7; then Stage B for P07: the game's face records drawn natively, so the PS2 packet building and the GS walk for the faces go away (`renderer_notes.md` §11.3) [I: not sized yet].
- **Not exact, so not proposed (Ffm's call):** faces at 15 fps; the GE's own skinning (its float maths differs from the VU's in the last bits).

## Ffm 15:30-15:32: step 4 waits for the native part (2026-10-30)

- Ffm 15:30 "if you think is better to wait for the native part, is ok"; 15:32 "ye lets keep on the roadmap, lets not disorganize ourselfs". Step 4 is not done on its own: drawing the faces natively (Stage B, `renderer_notes.md` §11.3) removes most of what it would speed up (the PS2 packet building, the unpack and the GS walk), so it would mostly be redone [I]. Its one lasting idea, handling a frame's face calls together so each part's code stays hot, is built into Stage B's P07 emitter. Steps 3, 5-7 only where Stage B leaves them worth doing.
- Roadmap order (`roadmap.md` "Now"): #83's fix (`renderer_notes.md` §11.19; the model's codec step 118.2 ms, gl6's 115.6), gl7 (the fix, and A-D timed on the deck and inside), then Stage B.

## gl7 on the PSP (2026-10-30 run 1, `psp_harness.md` "Build gl7", `prof/hw_gl7/run1/`)

- Exact to the end; #83's background on the PSP equals the PC's exact emulator (the five codec shots byte for byte).
- A-D (all four on against all four off, windows taking turns): the deck -4.62 ms a step (-2.2%), inside -8.77 (-6.5%), the story calls about -12 ms (-13% to -15%). The PSP gives about twice the model's figure.
- #83's cost with A-D on (gl7 - gl6 run 1, same game, both all on): call 1 +0.65 ms a step, call 2 +3.63 (the model: +2.6).
- With A-D off, gl7's call steps are about 11 ms faster than gl6's, all in the GS walk: so since the fix A-D save about 12 ms a codec step, not gl6's 24 [I: the cause not looked into].
