# Stage C notes: the native renderer, step by step

Stage C started 2026-11-01 20:50 UTC (Ffm chose "Native renderer" on the card; 20:54 asked whether A still holds after the corrections: yes). Sizing and order: `native_draw_study.md` (§3 table, §10 order). This file holds each step's design, then its checks and results. Tags: [C] checked, [I] inferred, [D] reference docs (a claim until checked on our disc). What the reference changes for Stage C: "What the docs study changes" below.

## Step 0: texture lookups by upload (design 2026-11-01, task #125; built and checked on x86 the same evening, task #128; on the PSP as gl13)

### 0.1 Short answer

- **What:** two memos in front of today's texture path, keyed by **what was uploaded** (the upload records' content identities) instead of **when** GS memory was last written. The map, the converter and the texture cache stay as they are. Each memo has its own g_opt bit and a check mode that compares it with today's path.
- **Why:** the game sends each texture page (a TRI: one CT32 transfer, DBW 1, texels first, CLUTs last [C]; the same layout [D]) into the same GS area at block 0x2000 every time it is used, so pages take turns there (A, B, A, ...) [C: renderer_notes §9 and dg_objects.md §4, "re-sent every frame it is used"]. Today each upload ages every kept area identity of that area: the next lookup walks the blocks again. Each upload also makes every CLUT load a new load: a block walk, a new identity, new rows, and gu_clut_id's kept identity starts over. When page A comes back, its record has the same content identity as last time, so keys built from identities survive the turns.
- **Size [I]** (model ms a step, the gl12 build's windows): textures deck ~3.9, inside ~4.7, codec ~2.9; CLUT loads deck 1.5-2, inside ~2.1, codec ~1.2. Together: deck ~5-6, inside ~6.5-7, codec ~4. This replaces the study's C4 figure of 8.4-14.2 (which was the whole map, uploads included). gl12 showed the model can overstate cuts of simple instructions two to three times on the PSP (psp_harness "Build gl12", Read), so the step is judged by the PSP's all-on windows against the build before.

### 0.2 Today's path, measured [C] (gl12 build `sbg12/d`, PspProf windows of `sbg12/model`; calls per step from `g12*.calls`, model ms from `fn_g12*.json`)

| Per step | Deck (30,810-30,890) | Inside (33,410-33,490) | Codec (31,510-31,590) |
|---|---|---|---|
| gl_tex calls / of them into tk_get_cl | 599.6 / 382.6 | 521.9 / 385.7 | 362.6 / 321.4 |
| touch / geomx calls | 765 / 765 | 771 / 771 | 641 / 643 |
| uploads (gu_image to the end, cover) | 17.6 | 18.3 | 12.0 |
| clut_new / gc_maybe / state_id calls | 55.0 / 80.5 / 870 | 76.7 / 102.5 / 1,232 | 33.6 / 50.6 / 516 |
| gu_area_id / tk_get_cl / gu_clut_id ms | 2.13 / 1.89 / 0.79 | 2.74 / 2.01 / 1.06 | 2.06 / 1.47 / 0.77 |
| geomx / touch ms | 0.74 / 0.60 | 0.72 / 0.64 | 0.78 / 0.43 |
| gu_clut_load / clut_new / state_id / gc_maybe / rows ms | 1.43 / 0.65 / 0.43 / 0.38 / 0.34 | 1.16 / 0.98 / 0.59 / 0.46 / 0.29 | 1.01 / 0.43 / 0.24 / 0.14 / 0.29 |

A lookup costs about 16 µs of model time on the deck (tk_get_cl 4.9, gu_area_id 5.6, gu_clut_id 2.1, geomx 2 × 1.0, touch 2 × 0.8). Line profile of gl12 (-g objects with the same flags and profile, mnemonics equal): tk_get_cl's hash finds 0.40 and mixes 0.39 ms, the description copy 0.30; gu_area_id's kept-entry check about 0.4, the rest walks (mix 0.38, st_of 0.25, blk 0.18). Conversions are rare (0.1 a step on the deck): the cost is in finding what is already converted.

### 0.3 Step 0a: the texture memo by uploads (gelive.c gl_tex, after the one-entry memo; texcache.c, gsup2.c helpers)

1. **Settle** the map first (gu_area_id and gu_area_flags do this on every call today: a pending transfer goes into the blocks at the same moment).
2. **Geometry slot G**, keyed by TEX0 (TBP0, TBW, PSM, TW, TH, CPSM, CSA) and CLAMP: the region (tc_region), the size (tc_texture_size), the lane groups (as gu_area_id) and the 8 KB pages the region's cells touch (at most 16, else no memo). Made once per key, so tc_region and geomx no longer run per use.
3. **Upload signature** of those pages. For each page and lane group, the page must have had no single-block write since its last whole-page write (pm 0). Its state must be one record that is complete and plain: not drawn, not bad, bw ≠ 0, got ≥ need, identity made from all its bytes, or a local copy with its own bytes. The signature mixes those records' identities (a run of pages with the same state word counts once). If any page fails, the memo is skipped and today's path runs.
4. **Palette part:** gu_clut_id's identity as today, cheap through the identity it keeps with a whole-load row set (0b keeps that load). Its flags must be 0.
5. **Texture slot T**, keyed by (G, signature, palette identity, TEXA where it counts). It holds the cache entries (index and identity) of the texture and the palette, the frame key, the GE texture description, u0/v0 and the lowest alpha. On a hit, `tk_reuse` checks that both entries still hold those identities. It then does exactly what tk_get_cl's hit does, in the same order: use counts, touch the texture then the palette, the lowest-alpha check. Then add_tex.

**Exact because:** today's area identity is a function of the state identities of the region's blocks. With every page of the region in one whole-page record state, every block's state is that record, so the area identity depends only on the region (in the key) and the records' identities (the signature). The flags are 0, so the cache key is the one today's path would make. The entry check stands in for the hash find, because keys are unique in the cache. The side effects keep their order, so the LRU order, the evictions, the pool addresses and therefore the GE lists stay the same. A collision of the 64-bit signature is the same kind of risk today's 64-bit identities already carry.
**Skipped per hit:** gu_area_id (the kept-entry check or the walk), tk_get_cl's region, both hash finds and their mixes, the TkTex clear and the description copy, and gl_tex's tc_texture_size.
**Memory [I]:** G 256 × 64 B + T 512 × 64 B = 48 KB of heap (the PSP's lowest headroom: about 580 KB in gl12).

### 0.4 Step 0b: CLUT loads by identity (gsup2.c gu_clut_load)

Today the table of loads by parameters keeps one load per CBP/CPSM/CSM/size/TEXCLUT. A load is used again only if its blocks were not written since, or still hold the same state words. After a page turn the CLUT blocks hold the new page's record, so clut_new runs: a sweep check, a block walk, a new load, identity mixes over up to 4 blocks × 3 lane groups, and new rows (deck: 55 a step).
**Step 0b:** a load's identity is a function of its parameters and its blocks' state identities. A small table (64 entries) keeps loads by identity: only loads whose snapshot states were all single, plain, complete records (or empty) when made, so their identities never change. When ld_now fails, the candidate from the identity table is compared block by block and lane group by lane group: the current record's identity against the identity of the load's snapshot record. On a match the kept load serves again: its snapshot, seq and epoch are refreshed, the parameter table and the rows point at it, and gu_clut_id's identity kept with it stays valid. Otherwise clut_new runs as today, and the new load joins the table if it qualifies. The sweep marks the table's loads like the rows' (cap 64).
**Exact because:** the identity and flags of a load used again are what clut_new would make: the same parameters and the same state identities in the same order, and those identities are final. Its CLUT entries read the same bytes: the same identity means the same source address, size and content key, read from guest RAM when used, as today (a local copy: the same full content hash).
**Log lines:** the load counters (kept vs made) and sweep times change, so the end-of-run summary lines differ. The frame digests, hashes and shots must not.

### 0.5 Checks, in order

1. x86, three routes (deck, inside, codec), each memo with a check mode: 0a computes the memo's answer without side effects, runs today's path, and compares the entry, frame key, description, u0/v0, lowest alpha, data and palette pointers. 0b builds the load as clut_new would into a scratch load and compares identity, flags and blocks. Both expect 0 differences. Counters on every route: hit rates, why a memo was skipped (page not whole, record not plain, too many pages), table fill.
2. The 434 hashes, 311 vuh and 199 digests with the bits on, off and in turns (the cfg cycle, as the lanes were checked).
3. PPSSPP 1.20.4 NOPROF and master (with the FPU and VFPU diffs): the 16 shots equal gl12's.
4. The model on the three windows, then gl13 on the PSP (no word needed, Ffm 22:00), judged by its all-on windows against gl12's.

### 0.6 Not in step 0

- **Page lane** (later, about 1 ms a step [I]): textures named from the game's page records (list 0x1e6820, entries 0x360 bytes; material +0x14 page id, +0x20 TEX0/CLAMP/TEX2 [C]) without the map. It is the input the native meshes need. [D] adds a 64-byte tag per texture inside a TRI (TEX0 relative to the upload, CLAMP, ALPHA, u/v offset and scale, id). It was not among the study's checks on our disc, and our page records already give TEX0/CLAMP/TEX2, so the tags would only confirm them; checked on the stage files before the page lane relies on them.
- gu_drawn (deck 0.94 ms, 1,595 calls a step) and gif_qw (3.95) stay until the native kinds replace the packets.

### 0.7 Build plan (x86 first). Held 21:20-21:40 for the docs study (Ffm); the review below changes nothing in it. Ffm 21:35: reorganize, check everything is in order, then start

- Tree `/home/claude/sb9` = sb8's sources, the diff saved to `tools/dev/sb9/`. g_opt has no free bit: all 32 are taken (OPT_XL_MARK is bit 31). So a second word `g_opt2` goes in boot.c: MGS2_OPT2, and MGS2_OPT2_CYCLE taking turns on MGS2_OPT_CYCLE's vb:n. It is all on by default, so old cfgs keep their meaning. OPT2_TX_UMEMO 1 (0a) and OPT2_GU_LDID 2 (0b) are applied per transfer in xl_psp.c, as OPT_GU_MEMO is. On x86, xlive_chk.c does the same, plus MGS2_TX_UMEMO_CHK and MGS2_GU_LDID_CHK.
- New code: gsup2.c `gu_settle`, `gu_area_pages` (the pages of the region's cells, at most 16) and `gu_pages_sig`; 0b's `ld_by_ident` before clut_new (its block list comes from the parameter table's load for the same parameters; Ld gains `fin` and `ser`; the sweep marks the table's loads); texcache.c `tk_reuse` (TkTex gains the two entry indices); gelive.c's G and T tables, and on a hit the one-entry memo set as the slow path sets it.
- Points found while checking exactness: (1) settle before reading page states. (2) A signature that only lists runs of equal state words can collide (A | A B against A B | B), so each run's start position goes into the mix with its identity. (3) Only loads made from final single records enter the identity table: a load made over an unfinished record kept an identity that can change. (4) Keep tk_get_cl's counters (uses, hits, palette uses and hits) and put the memo's own counters on new lines, so the old log lines compare.
- Container disk: 988 MB free after removing sb2, sb6m, sb8n, sb8m and sbg10 (21:18).
- **Progress (2026-11-01 22:10; paused by Ffm for the other thread's information, resumed 22:11 since step 0 needs none of it):** written and compiling: g_opt2 (MGS2_OPT2, MGS2_OPT2_CYCLE), the map's 0a helpers (gu_settle, gu_area_pages, gu_pages_sig) and all of 0b (identity table of 64 loads, kept alive by the sweep; check mode). Not yet: texcache.c's tk_get_um with the G and T tables and the shared palette part, gelive.c, the per-transfer bits and counter lines, the scripts, any run. State, decisions and the exact order of what is left: `tools/dev/sb9/README.txt`; code so far: `tools/dev/sb9/sb9_wip.diff`.
- **Results (2026-11-01 23:10, x86; `tools/dev/sb9/README.txt`): exact.** Check modes (each would-be hit also done the old way and compared): 0 differing of 5.70 M / 2.13 M / 5.53 M texture look-ups and 1.10 M / 0.57 M / 0.93 M palette loads on the indoor / deck / codec routes; answered 99.41% / 99.14% / 99.63%. Switched on: every picture line and state hash equal to gl12's x86 runs (indoor 17,664 frames / 378 hash lines, deck 11,766 / 252, codec 18,931 / 400), and the same with the two bits taking turns as gl13's cfg sets them. Model (sb9m, no PGO; gl12's windows): -5.80 ms a step on the deck, -6.66 inside, -3.81 in the codec call; on the deck 0a alone -5.25, 0b alone -0.69. PSP build gl13 = gl12 + step 0 with the profile and code order made again.
- **gl13 (2026-11-02 00:46, delivered as MGS2T_gl13; `psp_harness.md` "Build gl13"):** exact on both emulators and against gl12's runs (434 hash lines, 311 vuh, 199 frame digests, 16 shots); the emulator log's step 0 line: 4,320,753 look-ups by upload, 99.39% answered, 813,751 palette loads by identity. Model with PGO and code order (gl12's windows, everything on): deck 144.8 -> 141.5 (-3.3), inside 92.5 -> 87.8 (-4.7), codec 88.0 -> 86.2 (-1.8) ms a step; the no-PGO model's larger figures were mostly I-misses (4.4 of the deck's 5.8), which weigh far less with the PGO code order. The two bits take turns with everything on in gl13's cfg (cfg 22 texture memo off, 23 CLUT by identity off); the PSP's all-on windows against gl12's decide.

## Step 1: codec faces (P07), exact first (2026-11-02, task #133; tree `/home/claude/sb10`, saved in `tools/dev/sb10/`)

### 1.1 Short answer
Before any GE skinning, two exact parts on today's CPU path, each behind its own g_opt2 bit: **T1** a front for step 0's texture memo (`OPT2_TX_FRONT` 4) and **T2** the P07 calls held to their transfer's end, as step c holds P06's (`OPT2_C7` 8). Both give the same pictures byte for byte: on x86 every frame line, content digest and state hash equals the run without them on the three routes. Model (no PGO, 80 steps): T1 deck -2.0, inside -1.2, codec -2.5 ms a step; T2 codec route -4.5 ms a step (deck and inside draw no face: unchanged within 0.3). An output memo for face calls was measured and dropped (1.4). C1 (the GE skins and lights the faces) changes the picture by rounding, so it waits for Ffm's OK with shots.

### 1.2 Where the codec's P07 time goes [C model, sb10m no PGO, codec window 31,510-31,590]
Translator side: the slot walk `xl_lane_p07` 1.48 ms a step, `ud_eager` 0.73, `call` 1.26 (0.79 of it I-misses), `kick_vm` 0.43, `vm_dec` 0.34. Picture side: `gl_call` 0.58, `xc_lit` 2.85 (1.24 I-misses), `xc_state` 0.71, `xc_pass` + `xc_pass_do` 0.59, `gb_xform_in` / `_at` 0.64, `gx_batch` 1.31, the vertex loop `vc_p07_rec` 10.00. The work around the loop (these eleven, 10.9 ms a step) costs as much as the loop itself. x86 facts on the codec route: 2,262,731 P07 calls, all from the game's streams (none fell back to the VU memory, none empty or bad), 22.5 vertices a call; passes 2 and 3 drawn 1,320,065 times each.

### 1.3 T1: the texture memo's front (texcache.c `tk_ufront`)
32 entries (16 sets of 2) keyed by TEX0 & 0x3ffffffff, CLAMP and TEXA, valid while no block of the map has changed (the sum of the map's transfer, local transfer, drawn and failed-transfer counts is the generation). A look-up the memo answered before goes straight to its T entry's check, skipping G's hash look-up, the pages' signature and T's search; counters and use stamps are kept as the memo's own path makes them. Filled only after the memo's own path, which settles the map first. Sized on the three routes before building: 75-84% of look-ups. Check mode `MGS2_TX_FRONT_CHK=1` answers each look-up both ways and compares: 0 differing. Model off -> on: deck 166.30 -> 164.27 (-2.02: the map -1.51, `gu_pages_sig` 1.84 -> 0.46, `ug_get` 0.55 -> 0.17; the front's own +0.35), inside 106.11 -> 104.93 (-1.18), codec 99.64 -> 97.19 (-2.45: `gu_pages_sig` 1.94 -> 0.24).

### 1.4 Dropped: a memo of the face calls' output
A part's vertices could be reused when its call repeats exactly. Measured on the codec route: the bones are the same as the part's last call only 4.3% of the time (the face moves every frame), so the memo would almost never answer. Not built.

### 1.5 T2: P07 calls held to the transfer's end (xconv.c `hold7_test`, `hold7`, `place7`, `sv7_rebuild`; xlate.c `lo_touch`, `lo_src`)
**Sized first** by trace replay (`stepc7_sim`, from step c's `stepc_sim`, on gl13's build, codec route, 4 windows of 10 steps): I-misses 70.15 k a step as traced (PspProf's count), 37.77 k with the calls and their passes held and the GS state and texture looked up at the call (the form built): -32.4 k, about -6.6 model ms before the hold's own code. Step c for P06 (11.29) kept about 80% of its replay figure on the PSP.

**Why P07 needs more than P06's record.** A P07 call reads VU addresses 0 .. 63 besides its own slot: abs 2 .. 11 (fog, clamp and alpha, viewport, the matrix), 16 .. 18 (pass 3), 24 .. 31 (lights, colours, ambient) and the bones at 32 .. 63. The game sends those only when they change (bones by REF 4 or CNT 0 into abs 0x20 + 4 i; abs 0 .. 11 by a CNT per pass, 16 .. 31 per object), so by the transfer's end the memory has moved on. Copying 50 quadwords a call would cost more than holding saves.

**What the record keeps.** The translator gives each 4-quadword slot of abs 0 .. 63 a generation, bumped by every write that reaches it (UNPACK lazy or eager, the per-word path, the template copies, a transfer's end, a reset). When the slot's last write was one straight copy of all of it from the game's data (V4-32, MODE 0, no mask, CL = WL, no wrap), the translator also notes that data's address. A held call's record keeps, for each of the 14 slots it reads (0, 1, 2, 4, 6, 7, 8 .. 15), that address while the generation still matches (the game's data does not change until the transfer ends), else a 64-byte copy in a small log (128 slots, one copy per slot and generation, emptied when the held calls have run); plus T + 0 .. 3 and T + 0xfe, 0xff. At the run the record is placed in the scratch memory with T at 64, each slot copied only when the scratch does not hold that data already, and the usual code converts it from there: the same quadwords, so the same results. When the streams' fast loop declines (vc_p07_rec -8), the vertices are rebuilt in the scratch at T + 0x100 + 7 i as the VU memory held them.

**Which calls are held** (`hold7_test`): a call whose every read lies in its record and that makes a batch: count 1 .. 64, 1 .. 4 bones, no flag 0x804 or 0x500, passes 2 and 3 both or neither (0x210), T + 1 x zero, the vertices without a wrap, the streams the fast loop reads, every UV1 z 1.0 and with passes 2 and 3 every UV2 z, every bone offset below 29 (each matrix inside abs 32 .. 63, which the exact path then reads too) and a strip. The rest run at the call as before.

**Checks [C, x86, 2026-11-02 02:2x]:** with T2 on, every frame line, content digest and state hash equal to T1's runs: deck 11,766 frames / 252 hash lines (639,618 P07 calls held, every one that converts), inside 17,664 / 378 (1,104,330 held), codec 18,931 / 400 (2,262,731 held, all of them). Check modes on all three routes, each 0 differing: `MGS2_C7_CHK=1` (each held call's placed memory against the 60 quadwords the translator decoded at the call), `MGS2_SB_CHK=1` (the streams against the VU memory), `MGS2_SB_POISON=1` (the scratch memory poisoned before each placement); on the codec route `MGS2_SB_FORCE=1` with poison sent all 2,262,731 held calls through the rebuild, frames equal. The final source (`up_last` without divisions for the usual cycle settings, checked for all 16.8 M inputs) gave the same lines again on the three routes (02:38). Slots copied at the hold: 35,753 on the codec route (1.6% of calls); slots placed at the run: 2.5 a call of 14.

**Model [C model, sb10m2 no PGO, EBOOT e1c979e3, 80 steps; scratchpad t2_model.txt]:** off -> on in the same build: codec route 98.00 -> 93.55 (-4.45 ms a step, -4.5%): I-misses -5.71 ms, instructions +1.31. The loop keeps its code: `vc_p07_rec` 10.13 -> 8.55, `xc_lit` 2.87 -> 1.95, the per-call transform and pass records 0.72 -> 0, the translator -1.26, the texture map -0.85, frame and batches -0.53, the GS front end -0.37. The hold itself costs +2.89 (`xc_hold_test` 1.14, partly the streams' first reads moved there from the loop; `xc_hold` 1.07; the placement in `xc_held_run` 0.78), so the gain before it is 7.3 against the sim's 6.6. Deck 166.18 -> 165.85 and inside 105.97 -> 105.72: no face is drawn in those windows. With T2's code present but off, against the T1 build: deck +1.91, inside +1.04, codec +0.81: the slot bookkeeping's instructions (the translator +0.38 / +0.22 / +0.28) and the layout of a build without PGO; the PSP build with PGO settles the rest.

### 1.6 Next
- C1: the GE skins, lights and maps the faces (1.7). Its picture differs from today's by rounding, so it goes behind a switch and Ffm sees shots before it is on.
- Small exact trims found on the way: the slot bookkeeping only while P07's program is resident (deck and inside about 0.2-0.4 ms of instructions [C model]); `hold7_test`'s per-vertex UV z checks left to the run, whose fallback rebuilds exactly (part of 0.55 ms of instructions [I]); an A+D kick fast path (about 0.25 ms on the codec route [I]); `ud_eager`'s zero fill as memset (about 0.1 ms [I]).
- Both parts go into the next PSP build after gl13's run is read (cfg 24 "texture front off", 25 "face hold off" take turns with everything on).

### 1.7 C1 for the faces: the GE skins, lights and maps them (design, 2026-11-02 [I] unless marked)

**The mapping** (P07's arithmetic: `ge/vu1conv.c` `p7_vertex`; the GE's: PPSSPP 1.20.4's software GPU, `GPU/Software/TransformUnit.cpp` and `Lighting.cpp`, which its authors matched to hardware tests [C for what the code does]):
- Vertices: built once per group from the kept streams and cached: 8-bit weights (128 = 1.0 on the GE as on the PS2 [C PPSSPP's decoder]), one per bone slot the group uses (a vertex's pairs spread by slot = offset / 4; a 1-bone group gets weight 128), UV1 s16 as today, normal s16 4.12 as is, position s16 12.4 as is (the GE reads s16 / 32768, so x 2048 goes into each bone matrix's 3 x 3: a power of two, exact). 18-24 bytes a vertex against today's 20.
- Matrices: world identity, view = today's world (the call's V), projection, viewport, fog and texture scale as today. Bones: the call's slots (abs 32 + 4 s), column by column, float24 rounded to nearest.
- Lights: three directional lights, direction = column j of abs 24..26 normalised, diffuse colour = row j of abs 28..30 times that column's length, global ambient = abs 31, its alpha the call's GE alpha (today's doubled PS2 alpha); material ambient and diffuse white from registers, emissive 0, no specular. The GE lights in world space after the bones, the PS2's space.
- Pass 2: pass 1's buffer with today's UV offset (uv2 1); a second cached buffer with UV2 for uv2 2.
- Pass 3: texture-matrix mapping from the normalised normal (after the bones in PPSSPP's model [C code]; hardware to confirm with the probe), matrix rows s_j R[.][j], translation o_j, q 1.
- Strips: the ADC splits from stream A's flags, once per group.

**Where the picture moves** (to be measured on x86 before any shot): the GE lights in integers [C PPSSPP's model]: each light's dot product quantised to 1/512 (rounded up), each light's term truncated to 8 bits before the sum (the PS2 sums in float and truncates once), so colours can come out up to about 3 levels darker, about 1 on average [I; measured otherwise, 1.8: 0 or +1]; a +1 ambient bias may centre it. Light colours rounded to 8 bits. Bone and view matrices in float24 (15-bit mantissas): fractions of a pixel. A 1-bone vertex's normal is normalised on the GE, not on the PS2 (lengths within 4.12 rounding of 1 if the bones are rotations [I]).

**Which calls:** the ones p07_main's fast path converts and T2 holds, with the clamp abs 3 x = 255 (the GE's own clamp), light colours times lengths in 0..255, ambient in 0..255, bone offsets multiples of 4; the rest go through today's exact path.

**CPU work per call that stays:** T2's record (GS state, texture, slot pointers); bones only when a slot's data changed (pointer and generation, as T2 tracks them); lights per object; pass 3's matrix when its material changes; the draws on the cached buffer. Gone: the vertex loop (10.0 ms a step on the codec route), the placement in the scratch memory, the per-vertex strip work.

**The cache:** keyed by the streams' addresses and counts; checked at each call by a 64-bit hash of the stream bytes the call reads (the game can load another face at the same address); x86 check mode rebuilds and compares every call. About 170 KB a codec scene (7,000 vertices x 24 bytes) [I].

**Order:**
1. x86: a census of the constants (clamp, alpha, light lengths and colours, ambient, pass 2 kinds, bone offsets and scales, 1-bone normals, weight sums) and a model of the GE's arithmetic next to today's exact vertices: position (GE pixels), colour (levels) and pass 3 UV differences on the three routes. Exact runs stay exact (the model only reads).
2. The GE path in the PSP build behind a g_opt2 switch, off by default; PPSSPP master pictures of both forms in the container.
3. The next PSP build (after gl13's log): C1 taking turns with GE time and shots of both forms in Ffm's folder, for Ffm's OK.

### 1.8 C1 measured on x86 (2026-11-02, task #134; tree `/home/claude/sb11`, `ge/c1model.c`, MGS2_C1_MODEL=1)
The census and the GE model ran on the three routes from boot and the runs stayed exact (frames, contents and hashes equal sb10's final runs; the model only reads). Codec route figures unless said; the deck and inside routes agree.
- **Calls:** 2,262,731 P07 calls (51.0 M vertices; deck 0.64 M calls, inside 1.10 M), all in the fast form, at most 64 vertices a call (22.5 on average). C1's conditions hold for every one: none is left to the exact path.
- **Constants:** clamp abs 3 x = 255 and alpha 128 in every call; one light (light 0's direction has length 1 to six digits, lights 1 and 2 are zero); colour x length at most 187 (a third not whole: lengths a hair off 1), never from 255.5 or below 0; ambient 13..110, always whole.
- **Bones:** offsets always multiples of 4 and at most 28 (8 slots); 2-4-bone weights always sum to 128; a vertex's pairs often name one slot twice (14.9 M repeats; their weights add); bone matrices are rotations to 0.0008 (column lengths 0.99925..1, |cos| up to 0.00058); a 1-bone normal's length within 0.00115 of 1.
- **Positions:** the GE's within 0.099 GE pixel of today's (97.4% under 1/64, 2.6% 1/64-1/16, 0.03% 1/16-1/4; the deck route all under 1/64), none behind the eye. The largest come from float24 bone translations rounded to nearest (truncating them as `putf` does would double that).
- **Colours:** GE minus exact is 0 (68%) or +1 (32%), nothing else (mean +0.32; 1-bone and 2-4-bone alike). So the GE comes out brighter, not darker as 1.7 guessed: its dot product is rounded up and gains 1/512 (ceil(512 d + 1)), and with one light and a whole ambient nothing truncates twice. The ambient one level up gives +1 or +2 (worse), so it stays as is. In the frame (modulate with doubling) +1 is at most +2 of 255.
- **Pass 3:** UVs within 3.1e-5 (under 1/64 texel of a 256-texel texture).
- **Cache:** keyed by the streams' addresses, the count and the bones a vertex: 1,122 keys over the whole codec route (25,658 vertices, 601 KB at 24 bytes; deck 309 keys and 162 KB, inside 813 and 438 KB); no key ever met other bytes, so the faces are read in place and a key repeats about 2,000 times; 821 bytes to hash a call.
- **Weights:** distinct slots a call 1: 28%, 2: 17%, 3: 6%, 4: 7%, 5: 5%, 6: 8%, 7: 10%, 8: 19% (4.0 on average), so the weights are packed by the slots a group uses, not up to the highest.

**What it means [I]:** the picture moves by sub-pixel positions and +1 colour levels in a third of vertex colours: a visual change, so shots for Ffm first. CPU per call: the hash (about 820 bytes, mostly D-misses the exact path pays too), the bones that changed, the light and the list words, about 3.5 k cycles against about 11 k today: roughly 7 ms a step less on the codec route, before the GE's own time. The PSP probe has to show the GE's lighting as PPSSPP models it, the texture mapping from the skinned normal, and the GE time with up to 8 weights.

### 1.9 C1 built and checked (2026-11-02, task #135)
Tree `/home/claude/sb11`, saved in `tools/dev/sb11/`; PSP model builds without PGO `/home/claude/sb11m3/d` (c1opt2) and `/home/claude/sb11m4/d` (c1opt3).

**Short answer:** C1 works behind its switch (off by default). With it on, the game state is the same on x86 and the emulator, every frame's GE list is the same on both, and the faces differ from today's picture by 1 colour level in 1-2.6% of a codec frame's pixels (a few edge pixels by more). On the CPU it runs 2.5 ms a step fewer instructions than today's P07 path on the codec window [C model], but in plain model builds (no PGO, no layout) the instruction cache takes back part or all of that, by where the linker happens to put the face path's functions: -1.35 ms a step in one build, -0.06 and -0.08 in the next two. The PSP figure needs gl14's layout search to see C1 on. Without 1.7's vertex cache C1 is a small CPU gain at best; what it brings now is the GE path (bones, weights, lights) that C2, the people, needs. It needs Ffm's OK with shots before it is on; the GE's own time with up to 8 weights is the question for the PSP probe.

**The cache in 1.7 is not built (for now).** Each call builds its GE vertices into the frame's data block, as today's path builds its own. Reason then: checking a kept copy against the game's streams reads the same ~820 bytes a call that building reads. The model since shows the building is mostly instructions, not misses (below), so the cache stays the lever (see "Next").

**The blob** (`ge/replay_fmt.h` GerC1, GERL_C1; `ge/xconv.c` `xc_lit_c1`; `ge/vu1conv.c` `vc_p07_c1`, `vc_p07_c1s`, `vc_p07_c1b`, `vc_p07_bone`): GerXform, one GerLit a pass (its pass | GERL_C1, its vtx pointing at the GerC1), pass 1's vertices, pass 2's own (UV2 kind 2 only), the GerC1 (three lights, ambient, pass 3's texture matrix, the call's bones by packed slot), the strips; each part from a 16-byte boundary, every pad zeroed (the frame digest hashes them). A vertex: nw 8-bit weights (padded to even), UV 16-bit (the GE reads it unsigned, / 32768), normal s16, position s16; stride align2(nw) + 16; VTYPE 0x342 | (nw - 1) << 14. Held calls (T2) take C1 too; the room reserved for one is the larger of T2's bound and C1's (`xc_c1_hold_bound`).

**The list** (`ge/gelist.c` `c1_draw`): WORLD identity and VIEW the call's world (putf, the exact form's bits), so the GE lights in the skinned space as the VU program does; LIGHTING 1; a white material (ambient, diffuse, alpha 255; emissive and specular 0; no vertex colour); lights directional, diffuse only, no light ambient or specular; global ambient and its alpha; the bones from BOFS 0, only those that differ from what the GE holds; pass 3 by TEXMAPMODE 1 | 2 << 8 (texture matrix on the normalized normal) with the pass's texture scale and offset folded into the matrix (the GE ignores TSU..TOV in that mode), passes 1 and 2 by their UVs with TSU..TOV. Bones, light vectors and the texture matrix are rounded to the nearest float24. A batch that is not C1 sets LIGHTING 0 and texture map mode 0 again, and a transform batch VIEW identity; the frame's end restores the first two. A frame without C1 passes has exactly the words it had before.

**Switch:** g_opt2 bit OPT2_C1 (16), off by default (OPT2_DEFAULT = ~16 in `runtime/host_time.h`, `boot/boot.c`); x86 MGS2_OPT2=ffffffff turns it on; PSP cfg 26 "face GE lighting on" in the timing windows.

**Back to today's path** (counted in the log's "P07 C1" line; none on the three routes): `vc_p07_c1`'s conditions (the fast form's, the clamp 255, light colours and ambient in 0..255), a bone offset not a multiple of 4 or above 28, a slot's summed weights above 255, a UV outside 0..32767 (pass 2's own too), a bone matrix p7_loop would not take.

**Depth range** (`xc_zr_c1`): each vertex's own skinned view depth (one row of each bone with the view folded in, the weights q / 128), so the frame's depth map stays today's: on the codec route the maps of the face frames moved by at most 2.1e-6 of their width (0.14 of one 16-bit step) against C1 off. A bound over the box of the call's positions (each bone's depth over the box, times the least and largest weight sum) was a median 18% of the map's width too wide in face frames, up to 103%, so it does not set the range; since c1opt2 it only skips the per-vertex depths of a call whose bound lies inside the range already kept for its projection (`xc_c1_zin`: the range cannot move, the result is the same). On x86 that bound was inside for 77% of the codec route's C1 calls and 70% of their vertices (exact depths: 93% and 91%); it never missed an exact depth, and the PSP code in the emulator took the same 1,740,167 calls.

**PPSSPP master's GE model** (83e616ea, our exact picture reference; its lighting was changed to match hardware tests): light colour product x = ((2l+1)(2m+1)) >> 10; light factor s = clamp(floor(256 f), 0, 256), term ((2x+1)(2s+1)) >> 10; ambient ((2 mac + 1)(2 A + 1)) >> 10, which is A with a white material (alpha 255 for 255); the world normal is not normalized but the dot products are divided by its length; texgen normalizes the skinned normal. 1.8's colour figures used 1.20.4's formulas; the pictures below are master's.

**Checks [C]:**
- x86, three routes from boot (deck to vb 25,200, inside to 37,800, codec to 40,000): C1 off equals sb10's final runs (frames, contents, state hashes). C1 on: every state hash equal to C1 off (the picture never feeds the game); 7,465 of the codec route's 18,931 frames have other list words (the frames with face passes; a frame without them has the same words by design).
- Emulator (PPSSPP master with our two patches, `tools/dev/ppsspp_master_run.sh`), codec route to vb 40,000, both settings: every frame's list digest and frame digest equal x86's (18,931 frames), state hashes equal x86's (83 of them); again for each form below.
- Shots (master, codec route), C1 on against off, pixels of 480 x 272: 15,801: 1.26% (largest 6); 17,829 to 39,307 (11 shots with faces): 2.1-2.6% (largest 16-63); 98-99% of them by 1 level, all inside the faces' area; a cutscene body at 24,680: 10.8% (89% by 1, 13 pixels by 17 or more, largest 24); 20,130, 24,132, 24,400 (no P07): equal. Comparison pictures of five of them (both forms, where they differ, a x4 zoom) wait in the container for Ffm's PC folder: game pictures, never in messages.
- Every form below gives the same output: on x86 each one's runs equal the first form's in both settings (frames, contents, hashes, three routes), and in the emulator each one's every-frame digests equal the first form's, with the same shots.

**CPU [C model, no PGO, codec window 31,510-31,590: 154 P07 calls and 3,471 vertices a step; C1 on minus off inside each build; today's P07 vertex path `vc_p07_rec` 8.7 ms]:**

| Form | Change | Instructions | I-misses | D-misses |
|---|---|---|---|---|
| First (`vc_p07_c1v`: one pass, constants recomputed at each call, memcmp for the list's compares) | +0.65 | -0.04 | +0.66 | +0.02 |
| c1opt (a checks pass `vc_p07_c1s` before the bones and the writer `vc_p07_c1b`; the constants by a 33-word key; word compares in the list; the depth as the vertices are written) | -1.35 | -1.71 | +0.31 | +0.06 |
| c1opt2 (bones unrolled, 273 instructions a bone; the key compared in place; the list's 12-word compares leave at the first word that differs; the slot scan unrolled; the depth skip above) | -0.06 | -2.28 | +1.86 | +0.35 |
| c1opt3 (c1opt2 with the skip's bound by the box's centre and half extents, no branch per axis: 570 instructions a call to 188) | -0.08 | -2.52 | +2.05 | +0.39 |

The C1-off total itself moved between these builds (94.5, 94.7, 95.9, 95.3 ms) with no change to the off path: placement.

**The instruction cache is the swing [C model].** In c1opt2's build, ten I-cache sets (16 KB, 2 ways, 64-byte lines) each hold four lines of the per-call loop (`xc_lit` with `xc_lit_c1` inlined, `xc_state`, `xc_held_run`, `vc_p07_c1b`, `vc_p07_c1s`, `xc_xform`, `vc_xform`), so each of those lines misses once a call: 154 a step each, +1.9 ms in all. The loop's hot code is smaller with C1 than today's: lines run at least 100 times a step, 221 lines (14.1 KB) with C1 against 296 (18.9 KB) with `vc_p07_rec`; c1opt's build had 189 lines and 17 sets with 3 or more of them, c1opt2's 42, c1opt3's 219 lines and 39: with about 1.7 hot lines a set on average, placed at random, a quarter of the sets get 3 or more [I], so only a layout that sees the loop keeps it out of conflict. So a layout that sees C1 should give fewer I-misses than today's path, not more [I]; the gl builds' layout search (`pgo_gl.sh`: PGO training and traces) runs with the defaults, so with C1 off its functions would be cold (PGO optimizes them for size and the layout puts them anywhere). For gl14: training and traces with C1 on in part of the codec frames, or C1 left out of gl14.

**Next [I]:**
- The cache of 1.7 as the next lever: `vc_p07_c1s` and `vc_p07_c1b` are about 670 k instructions a step (3.3 ms), of which the per-vertex depths (about 85 k) stay for the calls the bound cannot place; a 64-bit hash of the 820 bytes a call is about 200 k (the misses the same): about 1.5-2 ms a step more on the codec window; memory about 170 KB a codec scene (the route's 1,122 groups 601 KB), against about 436 KB of headroom on the PSP (gl13 estimate): a bounded cache only, after the probe.
- gl14 (after gl13's run is read): T1 and T2 (exact) in the shared code; C1 only with Ffm's OK on shots of both forms (master pictures in Ffm's folder first): cfg 26 taking turns in the codec windows, a window with the GE's own time (no overlap) for up to 8 weights, PSP shots of both forms against master's, and the layout above.

## Step 2: the people (P06) skinned, lit and mapped by the GE (design 2026-11-02, task #140; tree `/home/claude/sb12`, `ge/c2model.c`, MGS2_C2_MODEL=1)

### 2.1 Short answer
- **The GE can draw every P06 call of the three routes itself** (2.07 M calls, 50.5 M vertices): skin, light, and map pass 3. No call needs today's path (2.2).
- **C1's mapping does not carry over as is.** The PS2 does not renormalise a P06 normal after the skin blends two bones (lengths down to 0.16), but the GE divides by the length. So with C1's mapping the joints would come out brighter: up to 90 levels, and 1.8-2.1% of skinned colour values by 5 or more.
- **Mapping (b) (2.3) fixes that.** The PS2's ambient goes in the GE's emissive colour. Each vertex's colour, used as the material's ambient and diffuse, is the PS2's normal length in 8 bits. Then [C model]:
  - 81-88% of colour values equal today's, and the rest differ by 1. Only 0.13% differ by 2, and 10 of 69 M by 3.
  - Alpha always equals today's.
  - Skinned positions are within 1/64 GE pixel.
  - Pass 3's texture coordinates are within 1/64 texel, using the texture matrix on the normal as the GE holds it (mode 3, not C1's normalised mode).
- **Cost [I, model]:** the CPU spends about 2 ms a step less on the deck and inside, because the per-vertex loop drops from 150-190 instructions to about 50. The GE spends about 1-1.2 ms a step more. That is hidden while the CPU is the slower of the two, but the deck's GE frame is already near 40 ms (2.6).
- **It is a visual change.** It goes behind a switch, off by default, with master pictures and PSP shots for Ffm's OK, as C1 did.

### 2.2 The census [C: x86, three routes from boot; each run's frames, contents and hashes equal sb11's C1-off runs]
- **Calls.** The deck route has 265,586 calls (6.58 M vertices), indoor 1,426,658 (34.5 M) and codec 373,221 (9.41 M).
  - Indoor by entry: +0 918,134 calls (23.1 M vertices), +358 skinned 398,877 (8.98 M), +738 with a factor 77,181 (1.79 M), +ab0 both 32,466 (0.63 M).
  - All calls are in the fast form, at most 64 vertices each.
- **Constants:**
  - The clamp abs 3 x is 255 and the alpha abs 3 y is 128 in every call.
  - Lit lights per call are never 0. Deck route: 1 light in all but 581 calls. Indoor: 1 light 400,588, 2 lights 831,240, 3 lights 194,830. Codec: 1 light 357,420, 2 lights 15,801.
  - Light direction lengths run 3.9e-5..1.124. Colour times length is at most 217.1 (187 on the deck and codec routes) and mostly not whole; it is never below 0 or from 255.5.
  - The ambient is 4..110, always whole.
- **Skin.** The weight w (the position's 4th halfword, 4.12) is always in 0..4096.
  - Indoor: 0 in 3.13 M of 9.61 M skinned vertices, 4096 in 3.52 M, a blend in the other 31%.
  - The matrix is a rotation (column lengths 0.9984..1, |cos| up to 0.00107) plus a translation up to 0.374.
- **Normal lengths:**
  - Unskinned data: within 1e-3 of 1, except 10,250 vertex draws (lengths down to 0.031) that come in the same counts on all three routes, so one model in the routes' shared start [I].
  - Blended (the PS2's n w + (B3 n)(1 - w)), indoor: 7.12 M within 1e-3; 0.86 M within 1e-2; 1.10 M within 0.05; 0.30 M within 0.1; 0.22 M within 0.25; 20,598 beyond, down to 0.16.
- **Factor (+738, +ab0).** It is 4096 (1.0) in all four channels of every vertex: 2.41 M indoor, none on the other routes. Lit sums over the clamp: 266,739 vertex evaluations indoor (27,430 with a factor), none on the other routes.
- **Passes.** All passes use modulate, TCC, blending and fog.
  - Pass 1: (Cs - Cd) As + Cd.
  - Passes 2 and 3 are drawn together, in 61% of the deck route's calls and 48% of the indoor calls.
  - Pass 2: Cd - Cs As. It uses pass 1's UVs plus an offset (138,735 deck calls / 653,528 indoor) or its own UVs (22,759 / 36,619).
  - Pass 3: Cs Ad + Cd, always the normal projection (0x200), never the light ramp or 0x100.
  - Every pass multiplies the vertex colour, so the GE lights each one.
- **gl14's model windows (indoor route):**

| Window | Calls | Vertices | Skinned | Factor | Passes 2/3 (each) | By lights |
|---|---|---|---|---|---|---|
| Deck, vb 30,810-30,890 | 15,013 (188 a step) | 359,995 (4,500 a step) | 107,285 | 9,837 | 156,640 | 1 light 143,136; 2 lights 216,859 |
| Inside, vb 33,410-33,490 | 11,140 (139 a step) | 268,460 (3,356 a step) | 87,280 | — | 154,050 | 2 lights 61,344; 3 lights 207,116 |

### 2.3 The mapping (b)
- **Vertices:**
  - Unskinned: 20 bytes (UV u16, colour 8888, normal s16 as the data holds it (4.12), position s16 as the data holds it).
  - Skinned: 24 bytes, with two 16-bit weights first: w x 8 and 32768 - w x 8, exact for w in 0..4096.
  - Pass 2 with its own UVs: a second buffer with UV2. Pass 3 has no buffer of its own.
  - The colour is (d, d, d, a):
    - d = round(255 len), where len is the PS2's normal length: the data's, or for a skinned vertex the blend the PS2 computes, in its order and float mode.
    - a = the call's GE alpha. For a factor entry, a is the PS2's per-vertex alpha.
- **Matrices:**
  - WORLD is identity, so the GE lights in the PS2's (skinned) space.
  - VIEW is today's world rows: x 32768 for s16 positions unskinned, x 1 skinned. So unskinned positions are today's, bit for bit.
  - Bone 0 = 32768 I. Bone 1 = (32768 B3, B's translation), each rounded to the nearest float24.
- **Lights:** the lit ones, directional, diffuse only.
  - Direction: the light's column normalised.
  - Colour: the PS2's colour row times the column's length, rounded to 8 bits.
- **The PS2's ambient** goes in the emissive colour, which the GE adds as it is. The global ambient is (0, 0, 0, 255).
- **MATUPD 3:** the material's ambient and diffuse come from the vertex colour. So the GE's colour is the ambient plus each light's (colour x d) scaled by its factor, clamped at 255. The alpha is a x 255, which equals a exactly. [C PPSSPP master Lighting.cpp: emissive added unscaled; ambient and its alpha ((2 m + 1)(2 A + 1)) >> 10.]
- **Pass 3:** TEXMAPMODE 1 | 3 << 8, the texture matrix on the normal as the GE holds it after the bones, with the pass's scale and offset folded in.
  - Rows: R[.][j] s_j x 8 unskinned (the normal is n / 8), or x 1/4096 skinned (bone 0 makes it 4096 n). Translation: o.
  - C1's mode 2 (normalised normal) would miss by up to 0.114 (29 texels), so mode 3 is needed. PPSSPP models that mode from the skinned normal [C code]; the PSP has to confirm it.
- **Calls left to today's path:** a call not in the fast form; a clamp other than 255; a light colour x length outside 0..255; an ambient outside 0..255; a weight outside 0..4096; an RGB factor other than 4096. None of these occur on the routes.

### 2.4 Where the picture moves [C model: PPSSPP master's GE arithmetic in scalar replicas against today's exact vertices; three routes]
Percentages are of colour values (3 per vertex), GE minus today's.

| Entry | Route | C1's mapping (a): equal / ±1 / 5 or more / largest | Mapping (b): equal / ±1 / ±2 / largest |
|---|---|---|---|
| +0 | deck | 85.8 / 14.2 / 0.057% / 70 | 85.8 / 14.2 / 0 / 1 |
| +0 | indoor | 84.0 / 15.9 / 0.012% / 70 | 84.0 / 15.9 / 0.06% / 3 |
| +358 | deck | 80.3 / 14.8 / 2.15% / 28 | 86.7 / 13.3 / 0 / 2 |
| +358 | indoor | 74.6 / 19.3 / 1.77% / 90 | 81.5 / 18.4 / 0.13% / 3 |
| +738 | indoor | 80.2 / 19.7 / 0 / 2 | 80.2 / 19.7 / 0.11% / 2 |
| +ab0 | indoor | 79.0 / 19.3 / 0.97% / 57 | 80.6 / 19.3 / 0.12% / 2 |
| +0 | codec | 85.6 / 14.3 / 0.037% / 70 | 85.7 / 14.3 / 0 / 2 |
| +358 | codec | 80.8 / 14.2 / 0.98% / 18 | 87.7 / 12.3 / 0 / 2 |

- **Mean (b):** +0.08 to +0.14 levels; the GE comes out a hair brighter, as in C1. In the frame (modulate with doubling), +1 is at most +2 of 255. The other encoding, d = min(255, 256 len), is no better (+358 indoor mean +0.095).
- **Alpha:** equal in all 50.5 M vertices.
- **Skinned positions:** within 0.0175 world units of today's. All are under 1/64 GE pixel except 22 deck vertices at 1/64-1/16. None are behind the eye.
- **Pass 3 UVs (mode 3):** within 4.6e-5, under 1/64 texel of a 256 texture for all 17.7 M indoor.

### 2.5 Not taken: today's loop on the VFPU
The question: could the same arithmetic on the VFPU (4 lanes an instruction) give today's bits with no GE work? It cannot:
- The converter's floats run in round toward zero on both machines, the VU1 program's rule: FCR31 0x01000001 on the PSP from the start (`psp/main_psp.c`), MGS2_FPU_RZ on x86.
- The VFPU only rounds to nearest (speed_notes §22, the v8 probe).

So a VFPU loop would be a visual change too, though a tiny one (a level only where a sum sits within a float step of a whole number). It would cut about half the loop's instructions [I] with no GE time. But it keeps the per-vertex work for good, while C2's unskinned vertices can be kept between frames (2.7). It stays the fallback if the deck's GE time binds.

### 2.6 What it saves and costs [I unless marked]
- **Today [C model, gl14 all on]:** `vc_p06_rec` is 4.56 ms a step on the deck (3.36 instructions, 0.19 I-misses, 1.01 D-misses) and 4.24 inside (3.10 / 0.36 / 0.79). That is about 151 and 187 instructions a vertex.
- **C2 per vertex:** the streams into the GE vertex plus the length colour, about 50 instructions. The length needs float work only for the 35% of skinned vertices that blend (about 50 more each); the rest use an integer test against d's thresholds. Per call, the GE constants take about 300 instructions.
  - In the model that is about 1.4 ms of instructions on the deck and 1.0 inside: about -2.0 ms a step each.
  - Pass 3's own vertices and their copy into the frame go: -0.1 to -0.3 ms (D-misses, `memcpy`).
  - The list's words add +0.2 to +0.3 ms: lights, emissive, bone 1, VIEW and the texture matrix, each sent when it differs from what the GE holds.
- **Net: about -2 ms a step on the deck and inside in the model.** The PSP has given 0.3-0.8 of the model's change (gl14's calls: 1.7-2x), and placement swings plain builds by ±2 ms. So it needs the gl builds' layout search with C2 in the training, as C1 did.
- **GE time:** lighting in each of 2.2 passes a vertex is 8,416 vertex-passes a step on the deck and 7,208 inside.
  - At C1's measured +0.145 µs a vertex-pass (P07, up to 8 weights) that is +1.2 and +1.05 ms a step, an upper estimate for P06's 0-2 weights.
  - Hidden behind the CPU today. But a deck frame took 38.3-38.9 ms of GE time alone (41.8-45.1 with the CPU busy) at vb 24,050 before the rain was halved, so at 25 fps the deck's GE budget has to be checked with C2 in it.

### 2.7 Keeping the vertices between frames (cache census) [C]
- **Every P06 call on the three routes comes from the lane's streams**; none comes from the VU memory. The cache key is the streams' addresses, the count and the entry.

| Route | Keys | GE vertices at 20 or 24 bytes | Same bytes again | Other bytes |
|---|---|---|---|---|
| Deck | 607 | 325 KB | 264,715 | 264 |
| Indoor | 1,455 | 723 KB | 1,424,939 | 264 |
| Codec | 643 | 338 KB | 372,314 | 264 |

- The 264 other-bytes events are the same on each route, so they come from the routes' shared start. gl14's model windows made no new key.
- **With mapping (b), a skinned call that blends any vertex needs that vertex's colour every frame.** Indoor that is 303,672 of 431,343 skinned calls (70%); 31% of skinned vertices blend.
- **So a cache would hold only the unskinned calls and the skinned calls with no blend.** A kept copy still has to be checked against the stream bytes by a hash (as C1's design), which costs about a third to a half of C2's own conversion [I]. That buys about 0.3-0.5 ms a step more [I] for 0.3-0.7 MB, more than the PSP's headroom of about 300-400 KB [I]. It is not in this step. Later it can come as a bounded cache, if the per-vertex work still matters.

### 2.8 Order
1. **x86 (`/home/claude/sb12`):**
   - `vc_p06_c2` (vu1conv.c): the GE vertices from the streams or the VU memory, the GE constants (GerC2) and the strips, with 2.3's conditions.
   - `xc_lit`: the C2 branch, held calls included (step c).
   - `c2_draw` (gelist.c): the state above, with C1's and C2's state switched as `c1_off` does.
   - The depth range: unskinned as today; skinned from today's blend of the positions where the call's bound is not inside the kept range (C1's skip).
   - g_opt2 bit OPT2_C2 (32), off by default.
   - Checks on the three routes: C2 off equals sb11's runs. C2 on gives the same state hashes, and only frames with P06 change their list words.
2. **Emulator (master):** every frame's list and frame digests equal x86's in both settings. Master pictures of both forms, with pixel counts.
3. **The next PSP build after gl14mp's log:** C2 taking turns, a window with the GE's own time, and PSP shots of both forms in Ffm's folder for the OK (never in messages).

### 2.9 C2 built and checked (2026-11-02, task #140)
Tree `/home/claude/sb12`, saved in `tools/dev/sb12/` (sb12.diff against sb11's files); x86 twins `x1` (C1 and C2 off by default) and `x2` (gl15's default: C1 on, C2 off); PSP model build without PGO `/home/claude/sb12m/d`.

**Short answer:** C2 works behind its switch. With it on, the game state is the same, every P06 call of the three routes takes the GE path (none falls back), each frame's draws and depth range are the same, and the emulator builds the same lists as x86. The people differ from today's picture in up to 3.8% of a frame's pixels, 99.3% of the differing pixels by 1 colour level (a few edge pixels by up to 15). It needs Ffm's OK on shots of both forms before it is on; gl15 has it in turns.

**The blob** (`ge/replay_fmt.h` GerC2, GERL_C2; `ge/xconv.c` `xc_lit_c2`, `xc_c2_bound`, `xc_c2_hold_bound`, `xc_c2_zin`; `ge/vu1conv.c` `vc_p06_c2` (the constants by a key, `p6_c2k` / `p6_c2x` / `p6_c2m`), `vc_p06_c2box`, `vc_p06_c2v` (the vertices; the colour's d by `c2_d`, a binary search of 255 thresholds made by `c2tab.py`)): GerXform, one GerLit a pass (its pass | GERL_C2, its vtx pointing at the call's GerC2), the GerC2 (voff for each pass, skin, three lights' colours and directions, the emissive (the PS2's ambient), pass 3's texture matrix, bone 1 when skinned: 164 bytes, 116 unskinned), pass 1's vertices (pass 3 draws them too), pass 2's own when it has its own UVs, the strips. A vertex: two 16-bit weights when skinned (w x 8, 32768 - w x 8), UV 16-bit, colour 8888 (d, d, d, alpha), normal s16 and position s16 as the data holds them: 20 bytes (VTYPE 0x15e) or 24 (0x455e). Held calls (step c) take C2 too; the room kept for one is the larger of step c's bound and C2's (`xc_c2_hold_bound`).

**The list** (`ge/gelist.c` `c2_draw`, `lit_once`): WORLD identity, VIEW today's world rows (x 32768 unskinned, x 1 skinned), LIGHTING 1; the material once a frame (`lit_once`: ambient, diffuse and alpha white, specular 0, MATUPD 3 for C2 and 0 for C1, light kinds directional, no light ambient or specular; C1's and C2's draws switch MATUPD and the emissive only when they differ); per call, only what differs from what the GE holds: the emissive, the global ambient (0, 0, 0, 255), the lights, bones 0 (32768 I) and 1 by BOFS, pass 3's texture matrix (TEXMAPMODE 1 | 3 << 8, the normal's scale and the pass's scale and offset folded in, as C1). A frame with no C2 pass has exactly the words it had before.

**Switch:** g_opt2 bit OPT2_C2 (32). In x1 it is off with C1 (OPT2_DEFAULT ~(16 | 32)); in gl15's tree OPT2_DEFAULT is ~32: C1 on by default (decided 2026-11-02 17:12, Ffm left it to me), C2 off. PSP cfg 27 "people GE lighting on" in the timing windows.

**Back to today's path** (the "P06 C2" log line, four counts; none on the three routes): (1) the fast form's conditions (p06_main's, as today's fast path takes them; the skin matrix as p06_main limits it; no lit light); (2) the clamp other than 255, a light's colour x length or the ambient outside 0..255; (3) a weight above 4096, a tint other than 4096, a UV outside 0..32767 (pass 2's own too); (4) no room in the frame.

**Depth range:** as today's path takes it (xc_zr_lit's w row): each vertex's view depth from its position as the stream holds it, or for a skinned vertex from the PS2's blend, computed as the vertices are written (`vc_p06_c2v`); a skinned call whose bound (the box of its positions through both bones, `vc_p06_c2box`, `xc_c2_zin`) lies inside the range already kept for its projection skips it (C1's skip; the result is the same). The maps equal today's in every frame of the three routes (x86: the frame lines' depth fields).

**Checks [C]:**
- x86, three routes from boot (deck to vb 25,200, inside to 37,800, codec to 40,000), x1: both off equals sb11's C1-off runs, C1 on with C2 off equals sb11's C1-on runs (frames, contents, state hashes). C2 on, with C1 off and with C1 on: every state hash equal (252, 378, 400); every frame's draws, vertices, primitives, textures, calls and depth range equal (only the data size moves); the frames with P06 passes have other list words (C1 on: deck 2,928 of 11,766, inside 7,664 of 17,664, codec 8,217 of 18,931).
- C2's calls: deck 265,586 (77,258 skinned, 6.58 M vertices), inside 1,426,658 (431,343; 34.5 M), codec 373,221 (90,319; 9.41 M): all of 2.2's census; fallbacks 0. In the lists: bones sent for 204,522 of the inside route's 431,343 skinned calls; C2's own words 13.8 M there (780 a frame); the depth by the bound alone for 61,485 calls (1.09 M vertices).
- Frame data: -1.4% on the deck route, -0.6% inside, -0.7% codec in all. Frames with passes 2 and 3 shrink (by up to 26 KB); frames whose people draw pass 1 only grow by the GerC2 and the normal in each vertex (1,180 inside frames, up to +131 KB: 824 to 955 KB at 36,899). The largest frame of each route does not grow (1,107 KB inside and codec; deck 1,138 to 1,121).
- gl15's twin x2 with gl15's nine turns: hashes equal; frames inside a C2-off run of turns equal x1's C2-off run, frames inside a C2-on run equal its C2-on run, and frames at a change equal one or both (deck 6,561 / 4,963 / 242; inside 9,726 / 7,530 / 408; codec 10,399 / 8,111 / 421).
- Emulator (PPSSPP master with our two patches), indoor route to vb 37,800, the PSP model build `sb12m/d` (C1 on in both): every frame's list and frame digests equal x86's with C2 off (x1 `out_it1.log`) and on (`out_it3.log`), 17,664 frames each; no split against the route's 685 reference hashes; 434 hash and 311 vuh lines, as gl14's.
- Shots (master, indoor route), C2 on against off, pixels of 480 x 272: 10,516 (a dark close-up of Snake in the opening) 3.83% (99.96% by 1, largest 13: 2 pixels by 9-16); 34,911 (Snake close, from behind) 3.71% (99.8%, largest 15: one edge pixel); 31,112 1.09% (96.0%, largest 2); 36,941 0.83% (largest 3); 33,613 0.75% (largest 3); 32,501 0.53% (largest 2); 35,589, 24,784, 29,500, 30,500 and 23,263 0.02-0.28% (largest 1-2); 8,760, 15,761, 24,244, 31,663 and 35,027 equal (dark, or nobody in view, though their P06 calls are drawn). Of all differing pixels 99.3% differ by 1. Comparison pictures of the first six are in Ffm's folder, SHOTS\c2_people (2026-11-02 19:05; game pictures, never in messages).
- Not yet known: whether the PSP's TEXMAPMODE 3 uses the skinned normal as PPSSPP does (gl15's shots in C2 turns show it), and C2's time on the PSP (CPU and GE, gl15's turns).

**gl15** (task #141, `/home/claude/sbg15`, scratchpad `gl15/`): sb12's tree, C1 on by default, C2 in four of nine turns (`MGS2_OPT2_CYCLE ffffffdf,ffffffdf,ffffffff,ffffffdf,ffffffff,ffffffdf,ffffffff,ffffffdf,ffffffff` with the GE overlap off in turns 1 and 2); PGO training and both traces with C2 off and on in every other 100 vblanks; frame checks 31,561-31,959; the sixteen shots above. Ffm's card (19:06): test C2 in gl15 or not yet; Ffm chose "Test in gl15" (19:12). Built, checked and in Ffm's folder 2026-11-02 21:50 (`psp_harness.md` "Build gl15"): state hashes, vuh and the 150 frame digests as the PC's in 1.20.4 and the master. One master shot (31,112, 11 vblanks into a C2 turn) differs from the always-on C2 picture in 36 pixels while its lists are the C2-on run's: the game's motion trail (sprites sampling the frame before) carries the off turn's picture in, about half each frame (2,024 pixels at the off turn's last frame, 0 from 30 frames in; the master again with C2 on all the time, `tools/dev/sb12/gl15/trail.sh`). So shots close after a turn change show the trail of the turn before; C2's own lists are right in every frame.

## Step 3: the rooms' meshes (P1214a0 / df0) (design 2026-11-02, task #144; census tree `/home/claude/sb13` (`ge/c3model.c`, MGS2_C3_MODEL=1), first trims `/home/claude/sb14`)

### 3.1 Short answer
- **The GE already draws the rooms with its own transform** (df0 since Stage A, from the game's streams since Stage B's lane 1). There is no skinning or lighting to move, as in C1 and C2. Step 3 is the CPU's cost per call: reading the streams into GE vertices, the scissor cut's box, the depth range, and the translator's bookkeeping.
- **Census [C]:** the indoor route has 1.73 M df0 calls (42.0 M vertices), the deck route 0.65 M (13.6 M), the codec route 0.53 M (10.5 M). All but 69-299 calls a route come from the streams; every stream is V4-16 without ROW; no call is bad. The cut leaves out 37% of the indoor calls and 52% of the deck's before any texture is asked for.
- **Two exact trims first,** built and checked on x86 (3.4-3.6, 3.9): 3v converts in two passes (the positions and their box for every call, the rest only for the calls the cut keeps), 3z takes the depth range from the box when the box lies inside the range already kept (as C1 and C2 do), and 3w writes the vertices straight into the frame instead of copying them there. Model [C model, an unprofiled build, 3.10]: 3v saves 1.2 ms a step in the deck window and 1.3 inside (0.9 in its own functions); 3z saves 0.2 in its own function, which the rest of the window gives back in this build; 3w saves 0.3 to 0.5 in its own functions. The windows themselves move by a few tenths between runs (3.10). gl16 times the trims on the PSP, with its own profile and code order.
- **Not now:** keeping each mesh's GE vertices between frames (3.7: whether a mesh's streams changed is not known exactly), and the translator's bookkeeping per call (3.8: about 2.4 ms a step on the deck [I]; it needs its own design on the VU memory side).

### 3.2 The census [C: x86, three routes from boot on sb13's twin, everything on as gl15's default; each run's frames, contents and hashes equal sb12/x2's]
- **Calls** (calls, vertices; from the streams / from the VU memory):
  - deck (to vb 25,200): 653,468 (13.57 M); 653,169 / 299.
  - indoor (to 37,800): 1,732,548 (41.98 M); 1,732,479 / 69.
  - codec (to 40,000): 530,034 (10.50 M); 529,965 / 69.
- **The cut** (left out off the scissor; drawn, with strips): deck 340,524 calls (6.44 M vertices); 312,944 (7.12 M, 1.54 M strips). Indoor 637,942 (12.69 M); 1,094,606 (29.29 M, 6.21 M). Codec 306,408 (5.70 M); 223,626 (4.80 M, 1.02 M).
- **The depth range** (calls into it; their box inside the range already kept, so the vertex loop gives nothing new): deck 650,976 (13.49 M vertices); 583,890 (11.95 M, 89%). Indoor 1,715,927 (41.70 M); 1,395,870 (33.67 M, 81%). Codec 529,618 (10.50 M); 457,138 (9.02 M, 86%). No depth-writing call has ZTST ALWAYS.
- **The model's windows** (indoor route, 80 steps each): deck 30,810-30,890: 14,344 calls (294,445 vertices, 3,681 a step), 8,209 cut (57%), 6,135 drawn (131,731 vertices), the box inside for 12,333 of 14,341 (250,213 vertices, 85%). Inside 33,410-33,490: 14,866 calls (405,344 vertices, 5,067 a step), 3,686 cut (25%), 11,180 drawn (331,947 vertices), the box inside for 9,605 of 14,586 (280,420 vertices, 70%).
- **Streams:** A (positions) and B (colours, flags in w) V4-16, C (UVs) V2-16, all without ROW. The header's clip flag is set on 2-11% of calls (deck 14,486; indoor 191,281; codec 40,914).
- **Frame data** of the drawn calls (a 188-byte GerXform and 16 bytes a vertex): indoor 699 MB over the route (vertices 67%, transforms 29%); the largest frame 922 KB (vb 22,978, 34,696 drawn vertices).

### 3.3 Today's cost [C model: gl15's build `sbg15/d`, its PspProf windows `sbg15/model`; per step, deck window / inside window]
Per call today: the lane (`xl_lane_p04`: `ud_eager` for the material, `ud_new` for streams A, B and C, `ud_eager` for the header), then `call()` into `xc_call`. There `vc_df0_rec` converts every vertex (57 PSP instructions a vertex), the cut's box loop runs (11 a vertex), and for a kept call come the texture, `xc_df0` (strips, `xc_xform`, the copy into the frame, the state) and the depth range (`xc_zr_df0`: every vertex's w).
- `vc_df0_rec` 1.61 / 2.18 ms (instructions 1.09 / 1.48, D-misses 0.48 / 0.63).
- `xc_zr_df0` 0.69 / 0.94.
- `xl_lane_p04` 0.82 / 1.01.
- Shared with the other kinds: `xc_call` 4.99 / 2.93 (the cut's box loop is inside it), `ud_new` 2.96 / 2.01, `ud_eager` 2.33 / 1.61, `call` 2.47 / 1.77, `memcpy` 2.22 / 1.79 (`xc_df0`'s copy among others), `vc_xform` 1.51 / 1.12.

### 3.4 3v: two passes (`ge/vu1conv.c` `vc_df0_pos`, `vc_df0_att`, `vc_df0_flag`; `ge/xconv.c` `xc_call`; g_opt2 OPT2_C3V 64)
- **Pass 1, every call:** the header, the transform (`vc_xform`), the positions, their box, the 16-bit test of a position (ROW streams only) and the negative-UV test (C's words OR-ed together, bits 15 and 31). A stream without ROW goes by words: A's first word is the GE vertex's x | y << 16 exactly as `vc_df0_rec` stores it, and A's second word with w cleared is its z and pad.
- **The cut** takes pass 1's box (`xc_df0_off`: the same floats as its own loop).
- **Pass 2, only for a call the cut keeps:** the UVs (C's word), the colour (B's two words and the call's alpha) and the flags (B's w). For a call the cut leaves out, the depth range's scan for a drawn triangle reads B's w from the stream (`vc_df0_flag`).
- Every output equals `vc_df0_rec`'s: the count, the transform, every vertex and flag, and the box (checked call by call, 3.9).
- Size [I, from the code; the model measures it]: pass 1 about 20 instructions a vertex and pass 2 about 16, against 57 + 11 today; and the cut calls never read stream B.

### 3.5 3z: the depth range by the box (`ge/xconv.c` `xc_df0_zin`, `xc_zr_df0d`; g_opt2 OPT2_C3Z 128)
- As `xc_c1_zin` and `xc_c2_zin`: over the box, w = a . p + t lies in [d - r, d + r] (d at the box's centre, from exact halved sums; r = |a| . the half extents), widened by 1e-5 of the magnitudes and by 1e-30. When that interval is in front and inside the range kept for the same projection, `xc_zr_df0` would only count the call and its vertices, so 3z counts them and skips the loop.
- The box comes from pass 1 or from the cut's loop, so 3z also works without 3v, and for VU memory calls when the cut ran.
- Exact: the check mode runs the loop on a copy and compares the counts, the w range and the Z24 pair (3.9).

### 3.6 3w: the vertices straight into the frame (`ge/xconv.c` `xc_call`, `xc_df0`; g_opt2 OPT2_C3W 256, with 3v)
- Today `xc_df0` copies the call's vertices from a buffer into the frame's room after the call's GerXform. With 3w, both passes write them there directly, and the copy goes (and with it the buffer's own cache lines). It applies when the room holds 1,024 vertices (always, but at a frame's last 16 KB).
- The room is the frame's data area `gb_room` reserved for this call; nothing else writes there before `xc_df0` (the texture look-up writes only the frame's texture table). A call the cut leaves out or that fails leaves the room unclaimed, as today.

### 3.7 Not now: keeping each mesh's GE vertices
- The census keyed each call by its streams' addresses and count. Deck 5,675 keys (2.67 MB of GE vertices at 16 bytes), indoor 9,085 (3.91 MB), codec 7,837 (3.38 MB). A key seen again had the same bytes in 647,476, 1,723,394 and 522,128 calls, and other bytes in 18 (deck: colours), 0 and 0. Kept copies of 256 KB would serve 99.3% of the indoor route's drawn vertices; the deck needs 512 KB for 97.7%.
- A kept copy is right only while its streams are unchanged, and that must be known exactly. The write watch (sb13 `MGS2_C3_WATCH`: the stream pages write-protected, each store sorted by what it hit) found the stream bytes written by many functions: positions by 26 (deck) and 62 (indoor), colours by 112 and 254, UVs by 9 and 13. Among them are libc's copies and the block-copy idiom, so the loads reuse those buffers [I]. A hook on known writers would not be exact, and comparing the streams each frame costs about what converting them costs. Later.

### 3.8 Not now: the translator's bookkeeping per call (3b)
- Each df0 call costs the lane `ud_eager` twice, `ud_new` three times and `call()` (model averages: `ud_new` about 420 cycles, `ud_eager` 480, `call` 1,270), about 2.4 ms a step on the deck [I]. Cutting it means not recording the VU memory the call would have filled, which a later call might read. That needs its own design (what reads the VU memory after a lane call). Later.

### 3.9 Checks [C]
x86 `sb14/x1`, three routes from boot, gl15's default (C1 on, C2 off); `c4runs.sh` (runs), `c4cmp.py` (every line but the settings' own; the hash lines against sb12/x2's):
- t0 both trims off (MGS2_OPT2=ffffff1f), t1 both on with MGS2_C3_CHK=1, t2 both on, t3 both on with MGS2_SB_CHK=1.
- **t1 against t0:** every line equal (frames, contents, draws, depth ranges, hashes): deck 61,867 lines, inside 92,652, codec 98,375; 0 differ. The state hashes equal sb12/x2's (252, 378, 400). Calls checked: deck 1,237,012 (653,169 two-pass calls, 583,843 by the box), inside 3,128,325 (1,732,479; 1,395,846), codec 987,099 (529,965; 457,134); 0 different.
- Two-pass calls left out before pass 2: deck 340,358 (6.44 M vertices), inside 637,894 (12.69 M), codec 306,360 (5.70 M).
- **t2 against t0:** every line equal on the three routes; the same counts as t1 without the checks.
- **t3 against t0:** only the check counters differ (`sb lane`, `sb lane 3`, `xlive lazy`'s flushes for the check); every df0 call's lane input equals the VU memory's (deck 653,169, inside 1,732,479, codec 529,965 checked, 0 differing).
- **3w** (x86 `sb14/x3`, `c4runs3.sh`: t4 everything on with MGS2_C3_CHK=1, t5 everything on; both against x1's t0): every line equal on the three routes (deck 61,867, inside 92,652, codec 98,375 lines), the hash lines equal sb12/x2's. Every two-pass call wrote its vertices straight into the frame (deck 653,169, inside 1,732,479, codec 529,965: the room always held 1,024 vertices); t4 checked 1,237,012, 3,128,325 and 987,099 calls, 0 different.
- **gl16's turns** (x3 t6, 2026-11-03: MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=ffffffdf,fffffe1f, the trims on and off every 100 vblanks): every line equal x1's t0 on the three routes, the hash lines sb12/x2's; the indoor route ran 855,393 of its 1,732,479 df0 calls in two passes (the on turns).

### 3.10 The model [C model: PspProf, 80 steps, the indoor route's windows; `sb14m/d` for 3v and 3z, `sb14m2/d` for 3w; both built without PGO, so code placement can move a function's misses by a few tenths either way]
- **Deck window (30,810-30,890), ms a step:** trims off 166.55; 3v 165.38 (**-1.17**); 3v and 3z 165.54 (+0.15 for 3z).
  - 3v: `vc_df0_rec` 2.02 gives way to `vc_df0_pos` 1.07 and `vc_df0_att` 0.30 (-0.65, of it instructions -0.52); `xc_call` -0.28 (the cut's own box loop gone); the other functions -0.24.
  - 3z: `xc_zr_df0d` 1.02 -> 0.79 (instructions -0.45, I-misses +0.18: the box test's code runs for every call into the range, and none of the function stays in the I-cache between calls); the other functions +0.38, of it the translator +0.16 with 0.5% more of its calls in the window (the window's jitter, below).
- **Inside window (33,410-33,490):** trims off 105.75; 3v 104.50 (**-1.26**, -1.2%); 3v and 3z 104.52 (+0.02).
  - 3v: `vc_df0_rec` 2.59 gives way to `vc_df0_pos` 1.34 and `vc_df0_att` 0.69 (-0.56); `xc_call` -0.36; the game code -0.19; the other functions -0.15.
  - 3z: `xc_zr_df0d` 1.26 -> 1.01 (instructions -0.52, I-misses +0.22); the game code +0.13; the other functions +0.14.
- **3w** (`sb14m2/d`, 3v and 3z on; 3w off = MGS2_OPT2=fffffedf): deck 165.93 -> 165.19 (**-0.74**): `memcpy` 2.33 -> 2.10 (-0.23: instructions -0.08, D-misses -0.11), `xc_call` -0.08 (D-misses), `vc_df0_att` +0.02, the other functions -0.45 (the window held 0.5% fewer translator calls with 3w on: jitter). Inside 104.66 -> 104.32 (**-0.34**): `memcpy` 1.98 -> 1.45 (-0.53: instructions -0.19, D-misses -0.29), `xc_call` -0.11 (D-misses), `vc_df0_pos` and `vc_df0_att` +0.13 (D-misses: the first write to each line of the frame's room now misses there instead of in the copy), the game code +0.16, the other functions +0.01.
- **The windows' own jitter [C model, from the runs' call counts]:** the deck window runs the same game code in every run, but how much of the picture's work falls inside a window moves between runs: the translator's calls on the deck by up to 0.6% (`gif_qw` 686,687 to 690,799 calls in the five deck runs), about 0.1-0.2 ms. Inside, the game's polls of the hardware vary as well (`rt_mm` 58,157 to 58,703 calls in the inside runs of gl15 and step 3; the game code moved 0.13-0.19 ms between runs), and with 3w on the window held one more pass of the game's per-step work (547 of the 1,078 functions called in the window, 80 calls for every 79). So the firmer figures are the trims' own functions: 3v -0.93 on the deck and -0.92 inside, 3z -0.23 and -0.25, 3w -0.29 and -0.51 (with `memcpy`, whose drop the removed copies account for: 6,212 fewer calls on the deck, 6,135 drawn mesh calls).
- **Reading:** 3v saves about what was designed: 0.73 ms of instructions a step on the deck and 0.80 inside, 40 and 32 instructions a vertex. 3z's own function saves 0.23-0.25 ms, about half its instructions (0.45-0.52) because of its I-misses, and the rest of the window's change is within the jitter. 3w saves 0.3-0.5 ms in its own functions. A PGO build places the new code with the rest, so gl16's own model windows (everything on against the trims off; 3z off, and 3w off, in further runs) and its PSP turns decide.

### 3.11 Order
1. x86 (3.9): done.
2. PSP model (3.10): done.
3. gl16 (task #146): the trims on by default and off in every other 100-vblank turn (MGS2_OPT2_CYCLE ffffffdf,fffffe1f; the log's setting 30 "mesh trims off"); C1 on, C2 off by default (Ffm's card, 23:22); the picture code's PGO profile and code order made again with the trims on and off in the training (`sb14/pgo_gl.sh`; drivers in `tools/dev/sb14/gl16/`). **Built, checked and delivered 2026-11-03** (02:28; `psp_harness.md` "Build gl16"): every hash line, vuh and frame digest equal on both emulators and the PPSSPP master, and all 16 master shots equal the C2-off master pictures, so the trims change no pixel [C]. gl16's own model windows [C model]: everything on 143.2 ms a step on the deck and 87.7 inside, the trims off 144.7 and 89.8, so the trims save **1.5 and 2.1 ms** (3w 0.6 / 1.3, 3v with 3z 0.9 / 0.8; 3z alone -0.3 / +0.3, within the code placement noise). Its PSP turns decide.

## Step 4: the effects the PSP leaves out: sprites, quads, streaks (2026-11-03, task #152; tree `/home/claude/sb15` = sb14 + step 4, saved in `tools/dev/sb15/`)

### 4.1 Short answer
- **What is missing [C: gsref's PS2 picture of three x86 captures, each entry left out in turn].** Inside (crew deck) the PSP already draws all but 0.1-0.2% of the pixels the PS2 writes. On the deck and on the aft deck in the rain a fifth to two fifths of the shown picture changes when the entries the PSP leaves out are left out: almost all of it is P04+3740's sprites (many owners: lamps' glows, a spotlight, smoke and more, 4.9), then the streaks (+2b60), P04+780, the quads (+3a38), the sea (P09) and the radar map's lines (stage program 202008).
- **Three converters, each matching the PS2's micro code [C: gsref's VU1 interpreter on the deck and aft deck captures]:** `ge/vu1conv.c` `vc_spr` (sprites), `vc_quad` (quads), `vc_line` with `vc_clip_seg` (streaks and their clipper). Every drop and clip decision equals the PS2's; corners within 1/16 px of the PS2's, the streaks' ends within 0.11 px (through the call's matrix on the GE, as +25f0's fans: 0.13 px there; since 4.7 the streaks go from their clip positions, 0.062 px); UVs within 1e-4 texel; colours equal.
- **Built and checked (4.5-4.9), off by default.** Drawing them is a visual change: the shots (Ffm's folder `SHOTS\c4_effects`) and the cost go to Ffm first. The sprites are not only the rain's (4.9: lamps' and lights' glows, smoke and more); the streaks are the rain's (rain.c and rain_slow.c, 4.9). Ffm's card (12:17) got "Test, streaks at 1/2" (12:46): the streaks thinned as the drop sheets are (4.11), then gl17 with the effects in turns (4.10).

### 4.2 The census [C]
Captures: the x86 twin (sb14/x1, gl16's tree; `tools/dev/sb15/cap4.sh`), MGS2_GSCAP of 8 vblanks each: deck vb 24,050-24,057 (idle pad), aft deck in the rain 30,800-30,807 and inside 33,410-33,417 (`pad_w01af`). gsref draws the PS2's picture whole and with each entry left out (GSREF_SKIPENT; `tools/dev/sb15/skipall.sh`); the share of the shown frame's 512 x 448 pixels whose largest channel moves by more than 8 levels:

| entry | what | deck (shown vb 24,052 / 054 / 056) | aft deck (30,803 / 805 / 807) |
|---|---|---|---|
| P04+3740 (0x19bb50) | sprites (owners in 4.9) | 11.2 / 17.0 / 9.6% | 39.3 / 41.3 / 38.2% |
| P04+2b60 (0x19af68) | line strips: the rain's streaks (rain.c, rain_slow.c; 4.9) | 2.6 / 2.8 / 2.6% | 2.6 / 2.7 / 2.8% |
| P04+780 (0x198b60) | | 1.5 / 1.8 / 1.9% | 0.03 / 0.03 / 0.00% |
| P04+3a38 (0x19be50) | rotated quads | 1.0 / 1.3 / 1.4% | 0.3 / 0.3 / 0.3% |
| stage 202008 | the radar map's lines | 0.3% | 0.5% |
| P09 (0x1a8148) | the sea | 0.04-0.05% | 0.4 / 0.5 / 3.8% |
| P04+a70 (0x198e58) | | 0.06-0.07% | 0.00-0.01% |
| all of them | | 18.6 / 26.7 / 19.8% | 38.9 / 40.5 / 35.1% |

- **Inside:** the entries the PSP leaves out write 1.2-1.5 K of the 0.59-1.16 M pixels a vblank (stage programs 201aa8, 201e30, 201c10, 201a18, P01b, +780, +a70): 0.1-0.2%.
- **Pixels the PS2 writes for the sprites** (gsref's per-entry counts, `tools/dev/sb15/pxent.py`): deck 1.30-1.63 M a full step (27.5-36.7% of all it writes), aft deck 1.84-1.90 M (38-40%): large translucent sprites over each other, 6 to 8 times the frame's 229,376 pixels. On the PSP's 480 x 272 that is about 0.74-1.08 M GE pixels a step [I], which sizes the GE's fill time (to be measured on the PSP). Quads 76 K (deck) and 19 K (aft deck), streaks 20 K, +780 80 K (deck).

### 4.3 The programs [C: the micro code; each statement checked by the converters against gsref's interpreter, 4.4]
- **Sprites, P04+3740.** Records of 6 quadwords from T+0x100: centre (float), RGBA (integers), UV0 (u, v in 1/4096, q = 0x1000), -, size (integers), UV1; count in T+0x14.x, matrix M at T+0x10..0x13, f = absolute 7.xy. c = M centre; corner 0 = c - size f, corner 1 = c + size f, in x and y only (z and w those of c), each product and sum rounded as the VU rounds. Both corners are CLIP-tested against |c.w| and the flags combined as corner 0 << 6 | corner 1. Any of 0xd7a (the sprite wholly past one side in x or y, or its centre past the near or far plane) drops it; else each corner past its own side is moved to the edge (0x080: x0 = -w, 0x200: y0 = -w, 0x001: x1 = w, 0x004: y1 = w), its UV by the same fraction in screen space (u0 = u1 + (u0 - u1)(n1x + 1)/(n1x - n0x), with n = corner / w and the VU's DIV). One GIF packet of sprite pairs (NLOOP the count; the loop runs once more than the count, the extra record never kicked). The GS draws a sprite with the second vertex's z, colour, fog and q.
- **Quads, P04+3a38.** Records of 12 quadwords: centre, RGBA, UV0, side (sx, sy integers), UV1 (the rest is the clipper's work space). Corners c - f s, (c.x + fx sy, c.y - fy sx), (c.x - fx sy, c.y + fy sx), c + f s, so a square turned by the side's own direction; UVs (u0, v0), (u1, v0), (u0, v1), (u1, v1). All four corners past one plane: dropped. Some corner outside: the record is marked (+8.w and +11.w = 0xcfff), its corners go into the strip with ADC, and after the strip the pre-lit clipper at 0x1710 clips its triangles (0, 1, 2) and (1, 2, 3), as +25f0's clipper does (T+20.x rewritten to 4 x count). Otherwise the four corners go into the strip, ADC on corners 0 and 1.
- **Streaks, P04+2b60.** Vertices of 3 quadwords: position (float), RGBA, UV with flags in w (0x8000: a new strip). Each vertex is transformed by the call's matrix and CLIP-tested; the segment ending at it is drawn in the strip (GS line strip) when both ends are inside, skipped by ADC otherwise. A skipped segment with both ends past one plane is dropped; any other goes to the line clipper at 0x2de8: the far plane (+z) first, then the colours multiplied by w (0x2508), then near (-z), +x, -x, +y, -y. At each plane the end outside is replaced in place by (a dB - b dA) / (dB - dA), and the test for both ends past one plane is repeated after each plane. What is left is kicked as a line of its own (GIF tag template T+22, PRIM 1). On our captures the packets hold 64 vertices and no call has fog.

### 4.4 The converters and their checks (`ge/vu1conv.c`; `tools/gsref.c` spchk, qdchk, lnchk with GSREF_DF0CHK=1) [C]
- **The GE's view of them.** The sprites and quads come out in the PS2's clip space (x, y, with w in z). For the GE the world is the identity, so its view space is the PS2's clip space; the projection is xc_xform's (depth pa w - pb, fog (w + fog1) fog2), as for the fans. The streaks' vertices stay world positions through the call's matrix, as +25f0's (4.7 changed this: the streaks from their clip positions always).
- **Sprites:** deck 1,376 calls, 29,904 sprites, 12,195 dropped by the PS2 and the same by the converter, 17,709 compared (1,089 with moved corners, 17,408 with fog); aft deck 18,240 sprites, 6,511 dropped, none different. Largest differences against the PS2's kicked values through the GE model: x 0.062 px, y 0.0625 px, z16 0.53, fog 1.0 level, UV 0. No input of a kind the converter does not know, no corner outside the GE's 0..4096 range.
- **Quads:** deck 320 quads: 176 whole, as the PS2's strip; 144 clipped, 288 triangles, 32 polygons empty, 256 fans, every one matching the PS2's in order. Largest differences: whole corners x 0.062 px, y 0.0625, z16 0.51, fog 0.69; fans x 0.062, z24 0.5, fog 1.0, UV 6e-5 texel, colour 0. Aft deck: 288 dropped, 32 clipped, 64 fans, all matching.
- **Streaks:** deck 256 calls, 16,384 vertices: 3,903 segments in the strip (the converter's class 1, the same), 3,409 dropped, 880 to the clipper: 867 lines kicked by the PS2, all compared; 13 dropped by both. Aft deck 4,891 / 2,794 / 507 (506 kicked, 1 dropped). Largest differences: strip ends through the call's matrix on the GE model x 0.086 px, y 0.099, z16 0.80 (aft deck 1.04); +25f0's fans, drawn this way since Stage A: x 0.13, y 0.096, z16 2.1. Clipped ends from their clip position x 0.062 px, z24 0.5, colour 0; the same ends from their interpolated world position through the matrix, as the GE will draw them: x 0.092 px, y 0.106, z16 1.05, none outside the GE's range.
- **Inside:** none of the three kinds runs.

### 4.5 As built (`/home/claude/sb15`, saved as `tools/dev/sb15/sb15.diff`, 15 files with the census of 4.9; README there) [C]
- **Two switches, both off by default** (OPT2_DEFAULT fffff9df): g_opt2 **OPT2_C4** (512) draws the three kinds; **OPT2_C4X** (1024, 4.7) sends +25f0's and 2870's strips and polygons to the GE from their clip positions. The PSP cfg's settings (steps.csv): 32 "effects drawn" (fffffbdf, C4 only), 33 "effect strips exact" (fffffddf, C4X only), 34 "effects drawn and exact" (ffffffdf, both). (Before 4x the words meant other things: fffffddf was C4 off, ffffffdf C4 on.)
- **Batches** (GERK_XFORMF as +25f0's, the primitive in GerBatch.prim; in each call the strip first, then what the clipper draws, as on the PS2):
  - sprites: GU_SPRITES, the drawn sprites' corner pairs (moved corners and their UVs as the PS2's);
  - quads: GU_TRIANGLE_STRIP, 4 corners a quad in +25f0's classes, and the clipped triangles' polygons from vc_clip_tri, drawn by fx_draw as the fans;
  - streaks: each run of segments drawn in the strip as GU_LINE_STRIP, then the clipped lines as one GU_LINES.
- **Positions:** all three kinds go to the GE from the PS2's clip positions (x, y, with w in z), the GE's world the identity. The design (old 4.5) sent the streaks' ends through the call's matrix as world positions; 4.7 found that route up to 1.31 px off at the strips' ends and 8.2 px at the clipped ends in the opening, so the streaks use clip positions always (largest 0.062 px).
- **Rain thinning:** Ffm's 1/2 (task #114, MGS2_GL_RAIN and MGS2_GL_SPLASH) still leaves out every other strip of +25f0 / 2870 (the drop sheets and the foot splash). The sprites, quads and streaks are drawn whole.
- **Lane 3:** the converters read the VU memory (xl_vm_need has their ranges: 6, 12 and 3 quadwords a record); no records from the game's data.
- **One work area for the four effect converters** (`ge/xconv.c` Fx, a union: xc_fpos, xc_spr, xc_quad and xc_line run one at a time and keep nothing there between calls). The first build gave the three new ones arrays of their own: 262 KB of static memory, which the heap lost even with C4 off (the PSP model build's heap on the emulator 11,824 KB at the start against gl16's 12,096, 5,320 against 5,592 at its lowest). Shared, the x86 object's static data went from 648,976 to 382,224 bytes; the PSP figures are in 4.8.
- **Calls the sprite converter refuses:** a record with a colour word above 255 or a UV q other than 1.0 (an input whose PS2 result the converter does not model) leaves the call out: 123 calls on the indoor route, 8 on the deck route; not looked at yet.

### 4.6 Checks and cost [C]
- **x86 twin, both routes** (the drawn digests with MGS2_GX_SEM=1, the GE list lines, the hash lines; `tools/dev/sb15/chk4x.sh`): everything off (fffff9df) equals the tree before 4x with C4 off: deck route drawn 0492c821, lists b8355028, hashes d228a016; indoor route 364eea9f / 8fac68eb / 63161e69. The shared work area changes nothing (C4 and C4X on, with and without it: dd02fad5 / 3e47c628 / d228a016). The hash lines are the same in every setting.
- **gsref with 4x** passes its converter checks (4.4) on the four captures: deck, aft deck, the opening (vb 8,770-8,777) and the deck at vb 24,664-24,671.
- **PPSSPP master (exact picture reference), the indoor route with gl16's settings and shots, the PSP model builds** (no PGO; `c4master.sh`, `masters4x.sh`; sb15m/d before 4x, sb15m2/d after):
  - **Everything off (sb15m2/d, fffff9df) equals gl16's master:** 434 hash and 311 vuh lines, the 150 frame digests (ca951d9e7a6170d1), 216,085,943 list words (the largest 62,127), the largest frame 1,877 batches and 1,107 KB, and all 16 shots. Heap at the start 12,096 KB (gl16's), at its lowest 5,576 (gl16 5,592: 16 KB of new code).
  - **C4 on (before 4x):** hash and vuh lines equal; 220,029,856 list words (+1.8%, the largest 69,882); the largest list and aux together still 342,536 of 360,448 bytes; the largest frame 2,124 batches and 1,406 KB. 7 of the 16 shots change (largest channel difference per pixel; share of the 480 x 272 pixels):

| shot (vb) | more than 8 levels | more than 16 | more than 32 | largest |
|---|---|---|---|---|
| 24,784 | 42.6% | 14.8% | 0.40% | 46 |
| 31,663 | 58.5% | 17.9% | 0 | 21 |
| 30,500 | 39.2% | 4.1% | 0.08% | 47 |
| 29,500 | 20.9% | 2.6% | 0.08% | 48 |
| 31,112 | 10.3% | 0.46% | 0.13% | 61 |
| 10,516 | 0.54% | 0.02% | 0 | 21 |
| 23,263 | 0.00% | 0 | 0 | 6 |

  - **C4X only (sb15m2/d, fffffddf):** hash, vuh and list words equal; digests 7ff1687ed873ef18. 8 of the 16 shots move where the rain's sheets lie (pixels of the 130,560 that differ at all / by more than 8 levels; largest): 10,516 42,069 / 525 (34), 30,500 16,824 / 13 (22), 29,500 13,752 / 3 (19), 31,112 11,807 / 0 (5), 24,784 10,805 / 0 (4), 23,263 1,109 / 0 (3), 33,613 195 / 1 (9), 32,501 3 / 0 (1); the other 8 equal. **The opening** (masters o4d / o4x to vb 9,000, `masters_op.sh`; the shots nearest the s87 capture of 4.7 are 8,769, 8,771 and 8,774): equal, because the PSP does not draw that scene yet: 8,769 and 8,771 are black (on the PS2 too, gsref's picture of the capture), but at 8,774 the PSP shows a plain grey rectangle where the PS2 shows the scene in the rain (gsref, from vb 8,772; the opening is an open picture gap, roadmap). So the capture's 4.6 px cannot show on the PSP today; once that scene is drawn, C4X keeps the sheets within 0.062 px. Today the fix shows most at the opening's dark close-up (10,516).
  - **C4 and C4X (sb15m2/d, ffffffdf), against everything off:** hash and vuh lines equal; 220,029,856 list words (as C4 alone; the largest 69,882); digests ab11425a572b9ead; the largest frame 2,124 batches and 1,406 KB; heap 12,096 KB at the start, 5,576 at its lowest (as everything off). 9 of the 16 shots change; 31,663 is the one C4X does not move with C4 on, 32,501 and 33,613 change by C4X alone:

| shot (vb) | differ at all | more than 8 levels | more than 16 | more than 32 | largest |
|---|---|---|---|---|---|
| 24,784 | 71.5% | 42.6% | 14.8% | 0.40% | 46 |
| 31,663 | 99.0% | 58.5% | 17.9% | 0 | 21 |
| 30,500 | 98.9% | 39.3% | 4.1% | 0.08% | 47 |
| 29,500 | 87.7% | 20.9% | 2.6% | 0.08% | 48 |
| 31,112 | 99.2% | 10.3% | 0.47% | 0.13% | 61 |
| 10,516 | 40.7% | 0.98% | 86 px | 1 px | 34 |
| 23,263 | 45.0% | 0 | 0 | 0 | 5 |
| 33,613 | 195 px | 1 px | 0 | 0 | 9 |
| 32,501 | 3 px | 0 | 0 | 0 | 1 |

  These are the pictures in Ffm's folder `SHOTS\c4_effects` (2026-11-03 12:03: per shot today, effects, where they differ, an x4 zoom; `strips_exact_only\` has C4X alone at 10,516, 30,500 and 29,500; README), made with `tools/dev/sb15/pairshots.py` from the masters x4d, x4a and x4x.
- **Counts on the indoor route with C4 on** (the master's log): sprites 230,742 calls (95,792 batches; 134,827 drew nothing), 3,808,357 sprites: 1,731,393 dropped as the PS2 drops them, 78,691 with moved corners, 2,076,964 drawn; quads 84,642 calls (16,740 batches), 108,292 quads: 86,920 dropped, 12,917 whole, 8,455 clipped into 14,946 polygons (1,964 empty); streaks 61,099 calls (51,028 batches), 1,857,077 segments: 1,039,324 dropped, 680,939 in the strip, 136,814 to the clipper (128,144 lines drawn, 8,670 dropped by it). No call lacked room.
- **The cost [C model: PspProf, 80 steps, the indoor route's windows; sb15m/d, C4 off against on, rain and splash at 1/2 as gl16; `models_c4.sh`]:**
  - **Deck window (aft deck in the rain, 30,810-30,890): 164.27 -> 179.55 ms a step, +15.27 (+9.3%).** Converters +7.81 (vc_vu_clip +2.13, vc_spr +1.73, xc_call +1.12, vc_xform +0.77, vc_line +0.72, vc_cflags +0.39, seg_plane +0.19, vc_quad +0.18); the translator +4.33 (vm_all +2.86, ud_new +0.25, gif_qw +0.18: decoding the VU memory the new kinds read [I]); libc +0.81 (memcpy +0.50, memset +0.15); frame and batches +0.57; the texture map +0.47; harness glue +0.43; GS front end +0.28; GE list +0.26; texture cache +0.20.
  - **Inside window (33,410-33,490): 105.09 -> 105.34 (+0.24, +0.2%)**, within the windows' jitter (3.10): none of the three kinds runs inside.
  - **Not in the model:** the GE's own time to fill the sprites, about 0.74-1.08 M GE pixels a step on the deck [I] (4.2). The PSP run measures it.
  - C4X's own cost is not measured; it adds no arithmetic (the clip positions are computed by the clip test already) [I].

### 4.7 4x: the effect strips from their clip positions (task #153) [C]
- **Why.** +25f0's and 2870's strips and polygons went to the GE as world positions through the call's matrix, and the GE keeps its matrices in 24-bit floats (16-bit mantissa). Far from the matrix's origin that loses whole pixels. gsref's GSREF_GEPREC=1 puts each effect vertex through four routes and compares the screen position with the PS2's kicked one (largest x / y difference, px):

| capture | vertices | today (24-bit matrix) | full-float matrix | recentred on the call's first vertex | clip position |
|---|---|---|---|---|---|
| opening (vb 8,770-8,777), fans | 1,030 (584 over 0.25 px) | 2.609 / 4.567 | 0.062 | 0.065 | 0.062 |
| opening, line strip ends | 6 | 0.77 / 1.31 | 0.056 / 0.061 | 0.099 / 0.098 | 0.056 / 0.061 |
| opening, clipped line ends | 66 | 8.211 / 3.409 | 0.095 / 0.163 | 0.354 / 0.141 | 0.062 / 0.062 |
| deck (24,050-24,057), fans | 43,075 | 0.127 / 0.096 | | | 0.0624 |
| aft deck (30,800-30,807), fans | 23,515 | 0.118 / 0.168 | | | 0.0624 |
| deck (24,664-24,671), fans | 35,575 | 0.084 / 0.095 | | | 0.0624 |

- **What 4x does** (OPT2_C4X): each vertex's clip position as the micro program computes it (x, y, with w in z) goes to the GE with the world the identity, as the sprites and quads do; the projection and fog are xc_xform's. A thinned strip's vertices are left out as before.
- **The untextured calls.** An untextured +25f0 call carries a stale UV q, and today's path refused such calls (6,720 calls on the deck route, vb 24,713-24,947, 48 a frame); with C4X they are drawn and the stale q is not read. On the deck capture (vb 24,664-24,671) gsref's picture with that entry's untextured triangles left out (GSREF_SKIPUNTEX) equals the whole one: their 21 candidate pixels all fail the depth test on the PS2 too.
- **Two oddities the checks found, both understood:** 2870 calls whose vertices all lie at one point (some with colour words above 255: the deck's unused particles; 240 in each of the two deck captures) draw nothing, and the live path skips them; one degenerate polygon in the 24,664 capture shifts gsref's pairing of fans by one, not a difference in what is drawn.
- **Effect on the route:** C4X changes the drawn digests of 1,013 of the deck route's 11,766 frames; the master shots in 4.6.

### 4.8 Memory [C]
- **The frame's data** (the batches, their vertices and transforms) has 1,432 KB (GL_PSP_ARENA 1,696 KB with NB 3,072 batches and NV 6,144 vertices; the arena lies in the 4 MB volatile tail with the sound RAM and cannot grow). The largest frame with C4 on is vb 22,978 at 1,406 KB: **26 KB to spare.** Of it df0 1,784 batches 955 KB, +25f0 / 2870 86 batches 152 KB, sprites 175 batches 239 KB, quads 40 / 12 KB, streaks 32 / 47 KB. C4 off: 1,107 KB (indoor route), 1,138 KB (deck route, vb 24,037).
- **The GE list:** the largest 62,127 words with C4 off, 69,882 on; the largest list and aux together 342,536 of 360,448 bytes either way.
- **Sharing transforms** (a census in the x86 twin, the deck route, KB saved over the route and the largest frame after): A, a step 4 batch reuses the last one's transform when equal; B, any effect batch does; C, every transform batch reuses the last one's camera part (100 bytes) when equal:

| setting | A | B | C |
|---|---|---|---|
| everything off | 0 (1,138 KB) | 7,352 (1,123 KB) | 118,959 (1,007 KB) |
| C4 | 14,123 (1,379 KB) | 17,893 (1,373 KB) | 127,634 (1,211 KB) |
| C4 and C4X | 14,123 (1,379 KB) | 21,855 (1,367 KB) | 128,151 (1,211 KB) |

  C frees 195 KB in the largest frame with the effects on (indoor route everything off: 1,107 -> 928 KB). Not designed yet.
- **Program memory:** with the shared work area the PSP model build's heap starts at gl16's 12,096 KB on the emulator and its lowest is 16 KB below gl16's (new code). On the PSP gl16's lowest headroom is estimated at 384 KB [I]; its run will say.

### 4.9 Whose effects they are [C: x86 census]
- **How:** the x86 twin with MGS2_FX_WHO (`ge/fxthin.c` fx_who_note, called by gelive's gl_call; x86 only, the PSP builds do not have it). Each effect call's records (XlCall.src) lie in a block of the game's allocator; the census finds the block (a header whose next header points back at it), then the actor in the 11 level lists that holds a pointer to it inside the actor's own block, and counts the call under the actor's file name (the name pointer at actor + 0x28, as the rain thinning reads it). `tools/dev/sb15/whoagg.py` sums the "fxwho:" lines by name (`-a`: every owner). Calls, not pixels: what each actor adds on screen would need a switch per actor.
- **Corrected 2026-11-03 12:46 (twin x11, MGS2_FX_WHO=3).** The first census (twin x8, written here at 12:00) scanned 0x1000 bytes from every actor, which runs past a small actor's block into the blocks after it, so a block could be counted for the actor before its owner: it named wind_man.c for the streaks, and brk_spotlgt.c, lodctrl.c and others for the sprites and quads. The census now reads only the actor's own block (to the next allocator header; 0x400 bytes when the actor's header does not look right), and with MGS2_FX_WHO=3 it counts the actors that point at each block: no block has more than one, on either route.
- **Indoor route (deck route in brackets):**
  - **streaks +2b60** 61,099 calls [37,848]: rain.c 55,200 [31,072], rain_slow.c 2,688 [2,688], spark.c 504, usp.c 57 [none]; block not found 2,650 [4,088]: records 0x400 apart outside the allocator's blocks, from vb 12,122 (the opening); owner not known. (whoagg reads rain.c 53,475 on the indoor route and lists 1,725 calls as "rows not read": lines the log's interleaved output broke; the thinning check of 4.11 counts all 57,888 of rain.c's and rain_slow.c's calls there.)
  - **sprites +3740** 230,742 [130,315]: body_sph.c 46,436 [19,426], charaspread.c 35,952 [23,280], fade_lgt.c 34,500 [19,154], bonbori_red.c 31,050 [17,478], line_sph.c 26,675 [23,483], bonbori_yw.c 10,350 [5,826], c_light.c 9,780, chim_smo.c 7,068 [4,052], set_t_dr.c 6,900 [3,884], item_box.c 5,138 [4,417], c_light_spot.c 4,296 [344], traffic.c 3,360 [3,360], gnrl_sprt.c 1,980 [4], rain_gas_pers.c 1,767 [1,013], steam_cm.c 262, splash.c 182, usp.c 114; on the deck route also weapon_sph2.c 1,952, rain_cm.c 508, weapon_sph.c 488; block not found 4,932 [1,646].
  - **quads +3a38** 84,642 [40,756]: fade_lgt.c 69,000 [38,308], george_bonbori.c 2,352 [2,352], smoke.c 290, mazzleflash_sub2.c 5; block not found 12,995 [96].
  - drawn since Stage A, for comparison: **fans +25f0** 552,833 [149,695]: d_splash_fall.c 117,300 [50,728], afterimage.c 67,440, blood_sp.c 52,210 [13,770], win_rain.c 42,966, sling.c 31,952, d_splash_floor.c 29,300 [19,420], sky_column.c 28,272 [16,208], blood_cm.c 21,184 [5,635], puddle.c 12,276, set_tex.c 11,820 [7,764], ripple.c 11,118 [10,170], d_splash_parts_slow.c 10,437 [511], foot_print.c 9,240, fog_wall.c 6,900 [3,884] and more; the drop sheets Ffm thinned (d_splash_camera.c, its object at +0x40) 9,172 [4,192] and the foot splash (ft_splsh.c) 330 [135]; block not found 85,631 [14,470]; **strips +2870** 471,712 [119,168]: blood_wl.c 334,144 [88,128]; no owner 137,568 [31,040].
- **What it means:** the streaks are the rain's. rain.c and rain_slow.c draw 95% of the indoor route's streak calls and 89% of the deck route's, and their blocks are found from the actor as fxthin finds the drop sheets' (task #114), so they are thinned the same way (4.11). The sprites and quads have many owners: by their file names lamps' and lights' glows (fade_lgt.c, bonbori_red.c, bonbori_yw.c, george_bonbori.c, c_light.c, c_light_spot.c), smoke and steam (chim_smo.c, smoke.c, steam_cm.c), body_sph.c, charaspread.c, line_sph.c and more, so thinning them as rain would thin more than the rain. Which actor draws the haze over the deck and at the door (31,663, 31,112) is not known yet.

### 4.10 Order
1. Converters and checks: done (4.4).
2. Batches, fx_draw, xconv, gelive behind OPT2_C4, then 4x behind OPT2_C4X; x86 and master checks: done (4.5-4.7).
3. PSP model: done (4.6).
4. The owners' census (4.9) and the master shots in Ffm's folder (`SHOTS\c4_effects`): done. Then a card to Ffm: drawing the effects and 4x are visual changes, Ffm's call.
5. Ffm chose "Test, streaks at 1/2" (card, 12:46): the rain's streaks thinned as the drop sheets are (4.11: x86 checks done; master shots to Ffm's folder next); then gl17 with both on and off in turns (settings 34 and 0) and `MGS2_GL_STREAKS=2`, the PGO profile and code order made again with them in the training, after the eDRAM probe's run (one build out at a time); its PSP turns give the cost with the GE's fill time.
6. Open: the frame's 26 KB margin (C, above, if the PSP shows a frame that does not fit); the 123 refused sprite calls; which actor draws the haze (4.9); the streak calls whose block the census does not find (4.9: from vb 12,122 in the opening, not thinned); P07's "paths not seen" (14,268 calls on the deck route, vb 24,467-24,711, 116-164 a frame, known since gl10, renderer_notes line 1490: the codec faces seen from the deck); the next kinds: P04+780 (1.5-1.9% on the deck), the sea (P09, up to 3.8% on the aft deck), the radar map's lines (stage 202008), P04+a70, the inside's stage programs.

### 4.11 The rain's streaks at 1/2 (task #155; Ffm's card, 12:46: "Test, streaks at 1/2") [C]
- **As built** (sb15, in `tools/dev/sb15/sb15.diff`):
  - `ge/fxthin.c`: a third kind, the streaks, for rain.c and rain_slow.c. Their actor holds a table from +0xb0 of 0x20-byte entries {float, 0, float, 0, the block's pointer, 0, 0, 0}; rain.c's actor block is 0x5b0 bytes (40 entries, 8-39 drawn), rain_slow.c's 0x8b0 (64). The number of entries comes from the actor's own block, at most 64 (the first form took 64 from every actor and turned rain.c's down as too short: 971 scans on the deck route, 1,725 indoors). Each entry's block is checked by its allocator header; the blocks are sorted and joined (one actor's lie 0x1290 apart, 4,736 bytes of data each), so each actor gives one range, marked as a line range. The scan reads the streak actors only when C4 is on (`fx_thin_scan(t, ram, lines)`); gelive scans for a line call too.
  - `ge/xconv.c` xc_line: a call whose records lie in a line range keeps strips 0, keep, 2 keep... (a strip starts where the PS2's ADC flags start one, class 0); the other strips' vertices get class 0, so the clipper and the pieces leave them out. xc_fpos's lookup skips line ranges.
  - `psp/xl_psp.c`: cfg `MGS2_GL_STREAKS` (1 or left out: whole; 2: half), passed to fx_thin_init with the sheets' and the splash's; the end-of-run line "picture: thinned streaks (1 in N): ...". `boot/xlive_chk.c` the same on x86, with the check below.
- **Checks** (twin x10, C4 and C4X on, streaks at 1/2, the drop sheets and the splash at 1/2 as gl16):
  - MGS2_THIN_CHK=1 (`thin_chk_l`): each thinned streak call converted again whole, and its thinned form built from that one: deck route 33,760 calls, indoor 57,888; none differs; segments kept 260,224 and 401,976, left out 262,844 and 403,317; calls with nothing left to draw 376 and 1,408. By the census's owner, every rain.c and rain_slow.c call is thinned and no other: indoors 561 other actors' calls (spark.c, usp.c) and 2,650 with no block stay whole, on the deck 4,088 with no block.
  - State hashes equal gl16's x86 runs on both routes (deck 252 lines, d228a016..; indoor 378, 63161e69..). With the setting left out, every frame's digests equal C4's runs before it (x7 with C4 on, x6 with it off; deck route). At 2, 1,013 of the deck route's 11,766 frames change against C4 with whole streaks: the frames with a rain actor (1,013 actors scanned).
  - The check runs' own frames: on the deck equal to the plain run's; indoors 8 frames (vb 24,032-24,502) differ, the first with one texture more. The check converts a call whose thinned form draws nothing again whole, and xc_line looks up the texture only when something is drawn (`if (!nsv && !npv) return 0;` before `c4_tex`), so the check run takes that texture into the frame and the VRAM places after it move. The plain runs are the reference.
- **Emulator masters** (the model build sb15m3/d, EBOOT 705fa578; `c4master.sh`; `mcmp.sh`), 2026-11-03 13:12:
  - s4u (C4 and C4X on, the setting left out) = x4a in everything: 434 hash and 311 vuh lines, 220,029,856 list words, the 150 digests, the largest frame (2,124 batches, 1,406 KB), all 16 shots; s4d (C4 off, the setting at 2) = x4d (gl16's picture) likewise (216,085,943 words, 16 shots). The lowest heap headroom 5,580 KB (x4a and x4d 5,576).
  - s4a (C4 and C4X on, streaks at 1/2): hashes and vuh equal; 220,007,699 list words (22,157 fewer); the 150 digests equal x4a's and x86's i_st2 line for line (no streaks in vb 31,561-31,959); the largest frame 2,124 batches, 1,387 KB (19 KB less); the end line "thinned streaks (1 in 2): actors 1767 ... calls thinned 57888", as on x86. 6 of the 16 shots change against x4a, 10 equal. Pixels moving by more than 8 levels / by any / the largest: 24784 0.45% / 21.0% / 25; 30500 1.57% / 19.4% / 26; 29500 1.59% / 21.6% / 28; 31112 0.14% / 2.4% / 15; 10516 0.11% / 0.5% / 15; 23263 0.00% / 2.7% / 5.
  - Shots: `pairshots.py` x4a s4a (whole, half) into Ffm's `SHOTS\c4_effects\streaks_half` (24 PNG, image chunks checked on the PC), the folder's README (0b6398bf) with the corrected owners and this section; staged copies deleted.
- **gl17's tree** (`/home/claude/sb17` = sb15 + the ME work's local-copy fix, `tools/dev/me14/patch_me14_lc.py` on ge/gsup2.c, 17f7b3c6; me_step2_notes.md): its x86 twin equals sb15's on both routes, effects off (= gl16) and on with the streaks at 1/2 (hashes, list, frame and content digests) [C].

### 4.12 What the effects cost, and the rain's options (task #158; Ffm 13:27: "how much ms it cost at 1/2? ... we need the best outcome, downgrade is not a problem") [C model; the GE's time is not in it]
- **The model** (`/home/claude/sb17m/d` = the gl17 tree sb17 + MGS2_GL_STREAKS=0 (`tools/dev/sb17/patch_none.py`), EBOOT 2fd88b9d, `mkpsp17.sh`, no PGO; PspProf on the indoor route's aft deck in the rain, vb 30,810-30,890, 80 steps; ms a step = (1.647 instructions + 68.2 I-misses + 85.9 D-misses) / 333 k; the splash at 1/2 in every run; `models17.sh`, `fit17.sh`):

  | effects | the rain's streaks | the drop sheets | ms a step | against effects off |
  |---|---|---|---|---|
  | off (gl16's picture) | - | 1/2 | 167.97 | 0 |
  | off | - | whole | 168.22 | +0.26 |
  | on | whole | 1/2 | 183.53 | +15.56 |
  | on | 1/2 by strips (Ffm's 12:46 pick) | 1/2 | 183.21 | +15.24 |
  | on | 1/3 by strips | 1/2 | 183.25 | +15.29 |
  | on | 1/3 by strips | whole | 183.30 | +15.33 |
  | on | none | 1/2 | 178.40 | +10.44 |
  | on | none | whole | 178.62 | +10.65 |

- **Where it goes:** the sprites and quads (lamp glows, smoke, haze; 4.9) 10.4 ms; the rain's streaks 5.1 ms whole (none -> whole: the converters +3.15 (vc_vu_clip +1.05, vc_line +0.69, xc_call +0.49, vc_cflags +0.37, the clipper +0.30, vc_xform +0.10), the translator's decode vm_all +0.91, memcpy +0.26, batches +0.21, textures +0.22, the GE list +0.08). Thinning by strips saves 0.3 ms at 1/2 and no more at 1/3: each call is still decoded and each of its vertices clipped before the left-out strips are dropped (xc_line thins after vc_line). The sheets whole cost 0.26 ms of CPU (11.30: their thinning saved no step time) and more GE fill [I: one sheet call is about 100,000 of the PS2 frame's pixels at vb 30,800, four calls a frame].
- **Streak calls 1 in N, whole calls by block** (`tools/dev/sb17/patch_calls.py`, cfg MGS2_GL_STREAK_CALLS; fxthin.h's comment):
  - *Why it can thin evenly* (x86 probe, `lprobe_xconv.diff` + `lprobe.py`, deck route, the streaks' clip positions before thinning): each rain call is 32 one-segment strips (64 vertices); the on-screen vertices of one call span a box of 1.72 x 1.76 (median) of the 2 x 2 clip square; in each of the 895 frames with rain calls, the box of the even calls covers 98-99% of all calls' box (1 in 3: 97-99%). The calls come in a different order each frame (the sort), so the choice goes by block: rain.c's range holds 32 blocks, each drawn once a frame (955-967 calls each on the route), rain_slow.c's 64 (40-42 each), one more range of 31 blocks once each; a call's data starts at 0x280 or 0x680 of its block's 0x1290 bytes (two halves taking turns), so (src - lo) / 0x1290 is the block's place and stays from frame to frame.
  - *x86 checks* (twin x4, C4 and C4X on, the sheets and the splash at 1/2): N = 3: the state hashes equal gl16's (deck 252 lines d228a016.., indoor 378 63161e69..); calls left out whole 22,155 of the deck route's 33,760 rain calls (65.6%) and 37,989 of the indoor route's 57,888; on the deck the frames that change against no streaks are the 1,013 that change between no streaks and 1/2 by strips, the same set; indoors 1,757 against 1,763, the 12 frames in only one of the two sets all in vb 24,024-24,494 (the deck where the streaks are mostly off screen; 4.11's check-run frames are there too), each of them a rain frame whose list differs as the kept streaks do. The setting left out: the deck route with streaks 1/2 by strips equals x1's frame for frame (11,766 frames, 0 differ).
  - *Model* (sb17m2/d: sb17m's build + patch_calls, EBOOT 00255284; its own effects-off run, since code placement moves totals by a few tenths of a ms between builds: there off 166.68, now 181.51, +14.83 against sb17m's +15.24): 1 in 3 with the sheets whole 178.94 (+12.26 against effects off, 2.57 less than now); 1 in 2 with the sheets at 1/2 179.49 (+12.81, 2.02 less than now). The saving is where the streaks' cost was (1/3 against now: vm_all -0.53, vc_vu_clip -0.69, vc_line -0.44, xc_call -0.18 ms). In the window, 33,474 of 51,008 rain calls left out at 1 in 3 and 25,504 at 1 in 2.
- **Emulator masters** (PPSSPP master, `c4master.sh` with gl16's delivery settings and shots): s17h (now: streaks 1/2 by strips, sheets 1/2; sb17m/d), s17t (1/3 by strips, sheets whole), s17n (no streaks, sheets whole): 434 hash and 311 vuh lines equal x4a's (no split), the 150 digests equal (no rain in vb 31,561-31,959); lists 220,007,699 / 220,011,967 / 219,129,614 words (x4a 220,029,856); the largest frame 1,387 / 1,387 / 1,366 KB. s17h's 16 shots equal s4a's. The heap: the break's highest point is 0b85c000 at vb 10,849 in sb17's builds against 0b846000 at vb 23,675 in sb15's and gl16's (+88 KB): newlib's realloc of the texture cache's table at the top of the heap (me_step2_notes.md), which me14p8 trims and sb17 did not (task #159: fixed, 4.14).
  - s17c3w (streak calls 1 in 3 by block, the sheets whole; the build sb17m2/d, EBOOT 00255284): 434 hash and 311 vuh lines equal x4a's, the 150 digests equal, 219,522,741 list words, the largest frame 2,103 batches, 1,382 KB; calls left out whole 37,989 = the x86 indoor route's; against s17t the six rain shots differ (10,516, 23,263, 24,784, 29,500, 30,500, 31,112) and the other ten are equal. s17n leaves out all 57,888 rain calls (= x86's count).
- **Shots** (`pairshots.py`; share of the 130,560 pixels that differ by more than 8 levels, largest difference): now against no streaks (sheets whole) 10,516 2.99% (19), 24,784 0.57% (18), 29,500 1.46% (28), 30,500 3.26% (29), 31,112 0.60% (27); now against calls 1/3 (sheets whole) 2.99%, 0.63%, 1.56%, 3.39%, 0.58%; no streaks against calls 1/3 (both with the sheets whole) 24,784 0.24% (17), 29,500 1.23% (28), 30,500 0.98% (27), 31,112 0.14% (15); calls 1/3 against strips 1/3 0.22%, 1.39%, 1.30%, 0.15% (other streaks, the same amount). Delivered 2026-11-03 14:35 to Ffm's `SHOTS\c4_effects\rain_options` (`1_sheets_whole_no_streaks`, `2_sheets_whole_streaks_third`, `3_no_streaks_vs_third`; 56 PNG equal by image chunks on the PC, list md5 a314dd9b; README 2c8b56ff with the cost table).
- **The GE:** gl16 on the PSP waited 0.00 ms a step for the GE in every deck window (vb 29,500-31,300, prof/hw_gl16/run1), so the sheets' extra fill is most likely hidden [I]; gl17's GE wait shows it with the effects on.
- **Ffm's call:** asked on a card 2026-11-03 14:38 (no streaks with the sheets whole, recommended; streaks 1/3 by calls with the sheets whole; or keep today's plan). **Ffm chose "No streaks" (card, 2026-11-03 17:38):** gl17's cfg MGS2_GL_STREAKS=0 and MGS2_GL_RAIN=1 (the close camera drop sheets whole), MGS2_GL_STREAK_CALLS unset; the foot splash stays 1/2 (MGS2_GL_SPLASH=2).
- **By owner (task #160, 2026-11-03 17:24; `deck_costs.md` §2.2, model build sb17km/d with the owner of each call bucketed):** on the rainy aft deck, 10.1 of the new effects' 14.0 ms (picture part, model) is the rain's own: the streaks 4.6 and sprites 5.5 (charaspread 2.27, line_sph 1.59, body_sph 1.44, rain_gas_pers 0.16; the rain's by their file names, the seed they share with rain.c and running only where it rains [I]; pictures with them left out to confirm). The lamps' glows are 2.3 (fade_lgt 1.35, bonbori 0.67, c_light 0.28), the rest 0.6 (item_box, chim_smo, set_t_dr). So "glows, smoke, haze" above holds for the whole routes' census; on the rainy deck the sprites are mostly the rain's. Recommended for gl17 (no card yet): the new effects without the rain's (a filter by owner), the drop sheets whole: about +4 ms in the model instead of +16.4 [I].

### 4.13 The rain's sprites left out whole (task #161; deck_costs.md §5's recommendation) [C, except where marked]

- **What:** with C4 on, the sprite calls (P04+3740) whose data lies in a block of one of the rain's four sprite actors are left out whole, before their decode (cfg `MGS2_GL_RAIN_SPRITES=0`; unset or 1: drawn, every path as before). The actors and where they keep their blocks, from the x86 census (sb15 `MGS2_FX_WHO=3`, both routes; no block two actors point at): body_sph.c (the block's pointer at +0x78), charaspread.c (+0xa8), rain_gas_pers.c (+0x184), line_sph.c (a table of 0x50-byte entries from +0xa0, the pointer first; up to +0xb40, 35 entries, read only inside the actor's own block).
- **As built** (`tools/dev/sb17/patch_rainspr.py` on /home/claude/sb17; fxthin.h's comment): fx_thin_scan takes their blocks each frame (kind 3), sorts them by address and joins neighbours (a block that starts at the last one's end + its 0x10-byte header), at most 512 a frame (FX_DROPS); gelive.c leaves out a sprite call whose `XlCall.src` lies in one (binary search), counted with the "other" calls. The log's end line: "picture: the rain's sprites left out: actors ...; blocks ... (not right, over); ranges; calls left out whole".
- **x86 checks** (`x5chk.sh`: twin x5 = sb17 + the patch; C4 and C4X on; `MGS2_THIN_CHK=1` counts the sprite and quad calls by the census's owner):
  - The state hashes equal x4's: the deck route 252 lines d228a016.., the indoor route 378 63161e69.., with the streaks none and the sheets whole (d_rs, i_rs) and with the streaks and sheets at 1/2 (d_rs2 against x4's d_st2).
  - Left out / drawn by the census's owner, deck route / indoor route: the rain's four 67,202 / 0 and 111,733 / 0; another actor's 0 / 102,127 and 0 / 186,627; no census block 613 / 1,129 and 613 / 16,411; no owner 0 / 0. Every call of the four is left out, no other actor's.
  - The 613 left out with no census block (`patch_rainspr2.py`, `patch_rainspr3.py`, `x7chk.sh`): all 613 on both routes rest on an allocator header whose third word is not 0 (0x017ef2d0, a pool word: the opening's own heap; the census's search skips such headers), in vb 10,340-15,505.
  - The filter's own counts: deck route 10,975 actors, 85,299 blocks (not right 0, over 0), 10,855 ranges, 67,815 calls left out whole; indoors 19,635, 151,731 (0, 0), 19,287, 112,346. (The census's four-actor count indoors, 111,733, is 903 above sb15's census of the same route: the census's owner look-up depends on its cache's history [I].)
  - By actor and part of the route (`patch_rainspr4.py`, `x8chk.sh`; indoor route; the opening before vb 16,000, the deck 16,000-31,499, inside from 31,500): body_sph 792 / 46,547 / 0 (vb 8,756-31,046); charaspread 0 / 29,520 / 6,432 (vb 23,261-32,197: the only one drawn inside, until about 10 s after the door); line_sph 160 / 26,515 / 0 (8,756-31,318); rain_gas_pers 42 / 1,725 / 0 (8,756-31,318). The deck route: 792 / 18,634, 0 / 23,280, 160 / 23,323, 42 / 971 (none inside).
- **Model** (sb17m3/d = sb17 + patch_rainspr, `mkpsp17.sh`, no PGO, EBOOT 8a41bee9; `models17r.sh`, the indoor route's aft deck window 30,810-30,890, 80 steps, the splash 1/2 throughout): C4 off with the sheets 1/2 (as gl16) **165.43** ms a step; the plan before Ffm's card (C4 + C4X, streaks 1/2 by strips, sheets 1/2) 180.32 (+14.89); Ffm's choice (no streaks, sheets whole) **175.68 (+10.25)**; with the rain's sprites left out **170.42 (+4.99)**. The filter saves **5.26 ms** (`fnsplit.py` by object: the effect converters 2.75 (vc_spr 1.32, vc_vu_clip 0.83), the translator's decode 1.29 (vm_all 0.88), lists, batches, textures, copies and the rest 1.34; fxthin's scan +0.11). PSP estimates [I: the translator's share x 0.73, the rest of ours x 0.83, deck_costs.md §2]: the plan before the card +12.0, Ffm's choice +8.3, with the filter +4.0; the filter saves about 4.3 ms a step on the PSP. In the window the filter left out 97,776 calls (r_rs log).
- **Emulator masters** (PPSSPP master, `c4master.sh` with gl16's delivery settings): s17r (`masters17r.sh`, sb17m3/d, C4 + C4X on, the sprites left out, streaks none, sheets whole): 434 hash and 311 vuh lines equal s17n's and x4a's; the 150 frame digests bbb4bf87.. against s17n's ab11425a.. (charaspread draws in vb 31,663-32,197, inside the check window: 22-24 PRIMs a frame fewer on x86); 216,750,082 list words (s17n 219,129,614); the largest frame 1,971 batches, 1,162 KB (s17n 2,092, 1,366); lowest heap headroom 5,476 KB (s17n 5,492); calls left out whole 112,346 = x86's indoor count. Shots against s17n: the six rain moments differ, the other ten equal byte for byte.
- **Shots** (`pairshots.py`, s17n against s17r; share of the 130,560 pixels that differ by more than 8 levels, largest difference): 10,516 0.27% (15), 23,263 0.00% (4), 24,784 37.38% (45), 29,500 15.94% (30), 30,500 31.05% (31), 31,112 8.88% (24). What they draw (from the pictures): a fine grey spray of drops over Snake's (and the guards') clothes, strongest close to the camera (24,784: the whole back of Snake's jacket), a faint speckle and haze on the far deck (29,500-31,112). More frames (`masters17r2.sh`: s17n2 and s17r2, the same two settings on sb17m3/d, shots at 12 moments picked from the count by actor and part of the route: 10,372, 10,872, 15,505, 22,978, 24,318, 26,007, 30,700, 31,701, 31,851, 32,001, 32,151, 32,195): the 434 hash and 311 vuh lines equal; six shots differ: the opening 10,872 14.39% (17) and 15,505 11.47% (20), a faint haze over the whole picture; the deck 26,007 0.09% (16), a faint speckle, and 30,700 29.63% (42), a haze over the whole deck; inside after the door 31,701 0.19% (47) and 31,851 0.02% (29), small drops of water on and around Snake (charaspread, the only one of the four drawn inside); the other six equal byte for byte (inside from 32,001 on, though charaspread's calls run to 32,197). Delivered 2026-11-03 18:12 to Ffm's `SHOTS\c4_effects\rain_sprites` (`1_rain_sprites_out`, `2_more_frames`; 48 PNG equal by image chunks on the PC, list md5 dfb00101; README 554edc1a with the cost table).
- **Ffm's call:** asked on a card 2026-11-03 18:14 (leave the rain's sprites out, recommended: about 4 ms a step on the rainy deck on the PSP [I], the look gl16 had; keep them; or pictures of each kind first). **Ffm chose "Leave them out" (card, 2026-11-03 18:19):** gl17's cfg and its PGO training take `MGS2_GL_RAIN_SPRITES=0`.

### 4.14 The heap's peak back to gl16's (task #159) [C]
- **What:** in sb17 the texture cache's entry table doubles at vb 10,849 (512 to 1,024 entries) while it ends at the top of newlib's heap; newlib's realloc then takes the whole new size from a top grown by sbrk, so the break's peak stood 88 KB above gl16's (4.12, emulator masters). The fix (`tools/dev/sb17/patch_trim159.py` on /home/claude/sb17; texcache.c `grow_table`, the PSP only): after a growth whose old block ended at the top chunk while the top was short, newlib's free top is given back at once (malloc_trim(0)), as me14p8's resize hook does (me_step2_notes.md). x86: no change. The log's new line: "picture: the texture cache's table: grown N times, M entries now; newlib's free top given back after K of the growths".
- **Emulator master** s17t159 (sb17m4/d = sb17 + patch_rainspr + patch_trim159, EBOOT e3427fc3; `master159.sh`, s17r's settings), 2026-11-03 18:09: 434 hash and 311 vuh lines, the 150 digests (bbb4bf87..), 216,750,082 list words and the 16 shots equal s17r's; the table grew 3 times to 1,024 entries, the top given back after 2 of the growths; the break's highest point 0b849000 at vb 23,675 (s17r 0b85f000 at vb 10,849: 88 KB lower; gl16 0b844000 at vb 23,675, the same moment, 20 KB lower as gl16's heap starts 21 KB lower: sb17's program is larger); the lowest headroom 5,564 KB (s17r 5,476, gl16 5,592). On the PSP gl16's lowest was about 388 KB, so gl17's about 360 KB [I: less the 28 KB found here]. In gl17's build.

### 4.15 gl17 delivered (task #156) [C, the PSP figures I]
- gl16 with the effects (C4 + C4X) off and on in turns of 100 vblanks, the rain as Ffm chose (no streaks, the drop sheets whole, the rain's own sprites left out), the heap fix of 4.14, PGO and code order made again; delivered 2026-11-03 20:31 to `Desktop\mg2 port\MGS2T_gl17` (`psp_harness.md` "Build gl17").
- On the PC every state hash equals gl16's (1.20.4 and the master); the master's 16 shots equal the effects-on masters in the effects-on turns and gl16's in the effects-off turns.
- Model (the deck window, the aft deck in the rain, and inside): the effects cost 4.14 ms a step on the deck (147.08 against 142.94), 0.22 inside; about 3.4 on the PSP plus the GE's fill [I]. The run decides.

## What the docs study changes (review 2026-11-01 21:40)

**Step 0 stays as designed.** Its two memos key on what our own map records. The study confirms the page facts they rest on: a page (TRI) is one CT32 transfer 64 pixels wide, texels then CLUTs [C ours, D]; page textures are 16- or 256-colour only [D], so a CLUT load spans at most 4 blocks, as 0b assumes. The TRI's per-texture tags matter only to the page lane (§0.6).

**For the later kinds** (each fact checked on our disc before code relies on it):
- **Codec faces (C1).** Each EVM vertex has 4 weight bytes summing to exactly 128 and 4 bone slot offsets; a packet has at most 32 vertices and 8 bone slots (all 1,280,665 vertices and 55,983 packets of the 123 stages, checked by the study). That is our own slot part F (`re/p07_builder.md` §3.4: UNPACK V4-8 unsigned, wl 2). The GE takes one weight per bone slot (8 bytes, 128 = 1.0), so the pairs are spread into slot order once per group from the kept streams, not per frame. The memory this costs is sized in the P07 design note against gl12's lowest headroom (about 580 KB).
- **All meshes (C2-C4).** The KMS layout (DG_DEF header, 0x40-byte units, 48-byte packets, separate streams; normal w = strip flag, 0x0fff draw, 0x8fff vertex only) holds in all 13,325 stage model files; 113 of them (0.85%) carry an odd packet (more than 64 vertices, unaligned streams, or normals outside the file). GE vertex data can be built once per model, each packet checked, odd ones left on today's path. The GE reads one interleaved vertex and the game keeps separate streams, so this is a copy in GE format, and its memory is sized first too.
- **Rooms (C4).** Pre-shaded models get their colours at set-up and keep them until a relight; fog is per vertex every frame [D]. DG_LightReshadeChanl = 0x128d90, our pending-light request [C, by the study]. The colours can stay in a stage-lifetime buffer refreshed when a relight runs, with an x86 check mode comparing them with the game's every frame before any build relies on it.
- **Effects (C5).** A closed list of shapes [D]: strips, culled strips, line strips (64 vertices a packet), free sprites (32), rotated squares (16), and 31 primitive kinds; one translucent texture each, sorted per packet.
- **Draw order.** Table A's 18 passes now have names (`re/dg_objects.md` §9), and our roles fit all 18. The native renderer follows them pass by pass. Shadows (pass 9) and spotlights (pass 10) draw into a texture and blend it onto receivers [D]: render to texture on the GE.
- **Depth rule.** Already ours [C, `re/mesh_packets.md` §1]: alpha test at 0x40 where Z is written, no Z write in the sort pass.

**Names.** our function-name lists label functions for the logic work's profile tables. Each is a lead matched by code shape. 2026-11-01 22:24: the docs thread's stricter check (function sizes must match) found wrong names in the first lists (two on texture-packet functions, others in the library area). Version 2 came 23:22: 901 and 983 names, each with its evidence; a change list gives every version-1 entry that moved, was renamed or was dropped. Still leads: each is checked against behaviour before use. Step 0 uses no names. `decomp_names_sles.md`'s six actor-system addresses were wrong and are marked there; nothing in our notes or code used them (grep, 2026-11-01).


## Next kinds (outline; order in native_draw_study.md §10)

1. Codec faces (P07, pass 5/8, DG_ChainEvmChanl): first exact with today's CPU skinning, then C1's GE skinning behind a switch for Ffm's OK, the weights spread once per group (above).
2. People (P06, builder 0x12e8b8 in pass 4/7, DG_Chain2Chanl, the multi-texture KMS), the same way. Its skinned entries blend two matrices by the position's w (renderer_notes §11.14). Census, GE mapping and plan: step 2 above (2026-11-02).
3. Rooms' meshes (P1214a0 / df0, pass 6, DG_ChainChanl): step 3 above (2026-11-02). The GE already transforms them; three exact trims of the CPU's work per call go to gl16; keeping each mesh's GE vertices between frames waits for an exact way to know its data is unchanged (3.7).
4. Effects (the sort pass 13, DG_SortChainChanl, and Prim2, pass 12), in the game's draw order. Step 4 above (2026-11-03): the sprites, quads and streaks the PSP leaves out; built behind OPT2_C4 / OPT2_C4X, off by default; Ffm chose to test them, the rain's streaks at 1/2 (4.11), in gl17.
