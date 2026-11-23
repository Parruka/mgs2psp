# MGS2 PSP: the native GE renderer. Design

2026-11-23, for the lead's course change of 15:58 (`plan_deck_25fps.md`). This is a plan with container experiments only. Nothing has gone to the PSP.
Marks: [C] measured or read in code; [C model] from the fitted cost model; [C twin] the x86 twin; [C dump] read from a RAM image of the twin; [I] inferred, with its basis named. Every [I] can be off by 2x.
Working notes: `main/attic/ge_native/state.md`.

This design replaces the ME-translation design of `native_renderer_design.md`. Its seam findings (§1.2-1.4), its coplanar-layer rules (§9) and its census tools still apply.

---

## 0. Short answer

**Seam.**
- The game's own pass code keeps running. It decides what is visible, in which order and with which texture page. It also computes the values the game reads back.
- Only the leaf packet builders are replaced. In a builder's place, the main CPU writes GE commands: the matrices, the lights and a material bind, then a CALL into geometry that was cooked once per model.
- The GE does the transform, lighting and skinning. The ME stops translating lists, kind by kind.

**First two kinds are clean [C twin, run V3a today].** I stubbed the ship builder 0x1214a0 and the people builder 0x12e8b8 together and ran the route to vb 31,400:
- **Reads:** the game reads the same values in every vblank of the rain window (2,099 vblanks) and of the talk. Read counts are equal everywhere.
- **IOP and frame digests:** equal at all 314 checkpoints.
- **Strict EE hash:** it moves, because packet bytes are no longer written. So each step gets its own hashref (x86 = emulator = PSP).
- **Steps 1 and 2 keep the strict standard.** Later kinds need the E5 reader standard (§5.1).

**The checkpoint as told to Ffm will fail even if the renderer succeeds [I].**

| State | ms a step | fps |
|---|---|---|
| Today (gl39 rain wait) [C] | 78-80 | 6.3-6.4 |
| After step 2 (ship + people native) [I] | 64-72, about 68 | about 7.4 |
| Full deck renderer [I] | 54-63, about 58 | about 8.6 |
| Floor with drawing costing the main CPU nothing [I] | 52-55 | |

The floor is the game logic (34.4 [C, K7]) plus the pass code that must stay for exactness (about 14 [C model]).
- Under 50 ms needs the renderer **and** logic work: the rain deletes, gl41/gl42 and G1/G2. With all of it: 39-54 ms, center about 45 (about 11 fps) [I].
- Ship meshes alone save little. The ship builder is already a stub with records (since gl31), and its chain holds only page uploads [C dump].

**Proposed checkpoint.** Measure it in one PSP run after step 2, with turns:
- K-A: today's path.
- K-N2: ship and people native.
- K-N2R: the same, with the other chains not translated (a bound).
- K7: nothing drawn.

From the K-N2R turn's own per-function profile:
- Project the full renderer's main CPU: K-N2R minus the remaining builders and walk, plus their list writing.
- Add the logic levers as measured in their own runs.

The decision rule is in §4.3. **The lead should restate the checkpoint to Ffm before step 2's run.** As worded ("ship and people under 50 ms"), it measures a state the plan never stops at.

**Biggest risks:**
1. The checkpoint framing above.
2. The GE's share of the RAM bus once a picture is drawn every step. The CPU ran at 75-83% speed while the GE drew [C MGS2GE].
3. Memory: about 1.2-1.4 MB cooked against about 1 MB freed [I].
4. Blended people normals need a per-frame d (§2.2), or joints come out brighter.

---

## 1. The seam

### 1.1 What the game reads back, and so what keeps running

Facts that bind:

1. **Model reads.** The game reads only obj->world from the screen pass. Exceptions: the EVM world copy into the codec face object, the curved patch's world, flags and LOD, and page+0x34C. Page+0x34C has no game load [C grep, R1]. (Source: the HD study §7.1 as a lead, checked here against our code and runs.)
2. **Rounding leaks [C b_proof].** Several pass functions put VU0 rounding into values that game code reads:
   - the box-cull micro programs (VF10 leftovers);
   - OnePieceObjs, CalcSortZ and DG_Prim2Chanl;
   - MakeMaxBound, 0x139940, ChainObj and PluginActor.
   Some of these are real logic: the x of GM_ActControl, HZX_GetLevelHeight and the block bounds. These functions must keep running exactly, toward zero.
3. **Clean functions [C b_proof].** StoreMatrixObjs and DG_SetObjsMatrix pass the poison test. So does the shadow chain.
4. **Pass cuts [C twin, H1 round 2026-11-23].** Cutting whole passes moves the strict hash. Every differing read is ORDER, LEFTOVER or GARBAGE-IN (R1-R8). The lead decided that pass cuts belong to the renderer under the E5 standard, not to H1 on the old path.
5. **Leaf stubs [C twin, V3a today].**
   - With 0x1214a0 and 0x12e8b8 stubbed, the read hash is equal over the rain window, the talk and the stretch between.
   - It differs in 2 vblanks of the deck's first play (25,944 and 25,974, not yet classified).
   - It differs in 3,785 vblanks of the opening's demos: w-lane leftovers, as in the old V1/V3a.
   - IOP and vfb are equal at all 314 checkpoints. The EE hash differs at 272 of 314 checkpoints, from vb 4,300 on.

**Rule: native kinds are armed per stage.** Cooked data is per stage anyway. The demo stages keep the builders, so their reads stay those of the old path. Step 1 arms w00a only.

| Function (pipeline 0x11da90-0x11ee98) | Native renderer | Why |
|---|---|---|
| 0x128d90 → 0x12cd78 static lighting into obj+0x6C | keeps running | its colours are cooked into the ship's VBs (§2.1); hook point for re-cooking |
| 0x12d560 → 0x12d378 dynamic point lights (mesh+0xD0) | keeps running | none on the rain deck [C dump]; a mesh with an override uses the builder that frame |
| 0x12d770 → 0x12d688 stream restore | keeps running | read by the next call [C] |
| 0x11e5e0 DG_SortChanlSystem, 0x11f4d0 FrameChanl | keeps running | the traversal; screen set-up |
| 0x121060 ScreenChanl: 0x120538 DG_SetObjsMatrix, 0x11fa58 OnePieceObjs, 0x11fde8 bones | keeps running | writes obj->world (read); OnePieceObjs leaks rounding (b_proof) |
| box cull micro programs | keep running, toward zero | leak into game logic (b_proof) |
| 0x120860 StoreMatrixObjs (384-byte constant block per visible mesh) | keeps running | **the native renderer's source** of matrices, lights and bones |
| 0x1226c8 → 0x122608 opaque mesh loop | keeps running | calls the stub per mesh, in order |
| **0x1214a0** P04 groups | **stub (step 1)** | V1, V3a clean |
| 0x130390 / 0x1303a8 → 0x12f130 people loops | keep running | read-back function (page flag R1; order) |
| **0x12e8b8** P06 | **stub (step 2)** | V3a clean |
| 0x138ac8 / 0x138ae0 → 0x137730 faces | keep running | EVM world copy is read |
| **EVM builder inside 0x137730** | stub later (step 5) | address UNKNOWN; EVM drove two always-differing keys in the H1 round (d00t P272fd0, d01t P276b38) → E5 |
| 0x133cc0 / 0x1324f0 shadow, spot | keep running | return at once on the deck [C]; write no object fields |
| 0x139180 plug-ins: 0x135338, 0x13d540 PluginActor, 0x1394a0, 0x1397a8 | keep running | leak rounding (b_proof O) |
| **0x13bf78** sea patches | stub later (step 4), with its exit state | a plain return flips 0x1b4818 bit 22, which w00a P331160 loads; R5 says P331160 masks it with 0x3330, so it is likely LEFTOVER; check on its own twin |
| 0x12dc90 Prim2, 0x12dbc0 CalcSortZ, radix sort | keep running | leak rounding; the native list takes its order |
| 0x122138 → 0x121c30 sorted loop | keeps running | the game reads its view data and a flag (V2) |
| **0x12d910** effect prims | stub later (step 3), with its return value | caller uses v0; the scratchpad words it leaves are read by w00a 0x3bebf0 (R5 LEFTOVER) |
| 0x124d48 page-change packets | keeps running first; stub in step 6 | tiny; the returned pointer is a packet pointer (R3) |
| frame code 0x124450/60, 0x124380, 0x1238e8, 0x123e80, DMA start | keeps running until step 6 | R2-R4: frame code and DMA timing words; cut under E5 |

### 1.2 What the renderer gets, and from where

A stub is entered from the game's own loop, with the object and mesh in registers. It never scans for them. Each picture, it reads the following (layouts: `re/dg_objects.md`, `re/mesh_packets.md`):

| Data | Source | Written by |
|---|---|---|
| Local → view matrix (the matrix today's batches use) | mesh+0x40, and the constant block at [mesh+0xFC] | ScreenChanl, StoreMatrixObjs (this step) |
| Bones | P06: mesh+0x80 / constant block q12-q15. P07: slots abs 32 + 4s | 0x11fde8, StoreMatrixObjs |
| Lights, ambient | constant block q4-q11 | StoreMatrixObjs |
| Fog | constant block (fog word); frame constants | frame |
| Model | obj+0x44 → def; mesh desc [mesh+0xF4], 0x20 a group; streams [mesh+0xDC..0xF0] | load |
| Visibility | obj+0x4C/0x50/0x5C, mesh+0xC0 (bit 0x1000 hidden, bit 1 pre-lit), +0xC8 (cull, clip), +0xCE (sorted), +0xD0 (stream override) | passes, actors |
| Pre-lit colours | obj+0x6C (per object) | 0x12cd78 |
| Material and texture | desc+0x14/0x18/0x1C → page: +0x33C texels, +0x340 CLUT, +0x344 this frame's CLUT swap | load, passes |
| Projection, viewport, screen squeeze (B1) | frame camera, as today's PROJ | frame |
| Order key (sorted meshes) | gl30n key: SPR 0xc70 + 16 × SPR 0xc64 | sorted loop |

**Stub rules.** These are today's `boot/nat_stub.h` rules, kept as they are.
- A stub writes host memory only, plus the return value its caller reads.
- A fallback (the original builder runs) is decided from game state only. Examples: a +0xD0 override, a C2 check that fails on game data, a model the cooker rejects. Host state never decides it: not cache fullness, not timing.
- So the guest state depends on the stub set and never on the cooked data, the GE or the ME. Cooking misses are filled on the main CPU in the stub itself, which is host-only work.

### 1.3 How the list is put together

- **Per kind, a native sublist.** In frame memory, the main CPU writes one sublist per chain: people (chain 2), opaque ship (chain 3), sorted meshes (chain 6). Each ends in RET.
  - Per mesh: WORLD (12 words, putf as today), lights if lit, BONE matrices if skinned.
  - Per group: a material bind (memo: only words that changed), then CALL into the cooked group list.
  - The cooked group list holds VTYPE, BASE/VADDR, one PRIM per strip, and RET.
- **Patch table.**
  - The main CPU does no texture work. A bind writes TBP/CBP placeholders and appends (list word, page, region, CLUT swap value) to a small per-frame table.
  - Before the kick, the ME resolves the table through `texcache` (where it runs today [I]) and patches the words.
  - The ME also patches the depth map (VIEWPORT z scale and offset, today's per-frame Z24 → 0..65535 rule). That map is known only once every batch of the frame is seen.
  - It also writes the per-frame d of blended people vertices (§2.2).
- **Order.** The walked chains stay in the ME's output in chain order. Where a native chain sits, the walk emits CALL to its sublist.
  - Chain 6 interleaves sorted meshes and prims by key. The ME's chain-6 output CALLs main-written blocks by key.
  - Depth: top → native block → cooked list is 2 levels, within the GE's limit of 2.
  - In the end state there is no walk, and the main CPU writes the top list itself at frame end.
- **Double-buffered.** The lists live in the two frame memories, as today's frames do. The main CPU writes back the dcache range of a finished sublist. The ME writes uncached and flushes before the kick.

---

## 2. Per draw kind on the rain deck

Census at vb 29,900 (twin, the rain deck, `gen/census.py`, `drawn.py`, `cooksize.py`) [C dump]:

| Kind | Drawn this picture | In the queue (cooking scope) |
|---|---|---|
| Ship and props (P04) | 18 objects; 146 meshes; 587 groups; 11,303 vertices: 10,447 pre-lit, 856 lit (284 of them on a 2-matrix selector); no sorted mesh, no +0xD0 override | 37 objects with pre-lit meshes (40,013 vertices; the ship object 30,017); lit 6,283 vertices |
| People (P06) | 2 objects × 22 meshes; 282 groups; 6,816 vertices; 2,188 blended; passes 2-3 on 3,916 | 13,406 vertices, 1,008 groups |
| Faces (EVM, P07) | 1 queued | |
| Effect prims | 287 queued; chain 6: 2,197 tags, 56,113 REF qw | |
| Sea | chain 5: 441 tags, 3,680 qw | |
| Stage program (radar lines, chain 10) | 1,332 tags, 59,568 REF qw (415 calls, S_w00a_P3a4138) | |
| Materials, pages | 161 materials, 15 models, 32 pages | |

### 2.1 Ship meshes (P04; chains 3 and 6). Step 1

**Today [C].** The stub 0x1214a0 writes records (gl31+). The ME converts the streams per frame into GE vertices. The GE draws them with hardware transform: WORLD = the batch's matrix (putf, top 24 bits), PROJ and VIEWPORT per frame, fog, one PRIM per strip. So the ship is already GE-transformed. What moves is where and when vertices are made.

**Cooked format.** The PS2 streams are s16 (`mesh_packets` §3):
- A: x, y, z, w;
- B: normals or colours;
- C: u, v with 4096 = the full texture.

The GE takes s16 positions, normals and UVs as value/32768. The scale ×8 goes into WORLD and the texture scale as a power of 2, so **cooked positions and UVs are bit-exact copies** [I: exponent-only scaling].

| Group kind | Vertex | Bytes | Scope |
|---|---|---|---|
| pre-lit | UV s16×2, colour 8888 (from obj+0x6C, today's channel mapping), pos s16×3 + pad | 16 | per (object, mesh): colours live per object |
| lit | UV s16×2, normal s16×3, pos s16×3 | 16 | per model |
| selector groups (desc0 bit 0: w = 0 or 4096 picks one of two matrices) | + 2 u8 weights (128/0 or 0/128) | 20 | per model; GE skinning with weight 1.0 is the matrix itself |

**GE features.**
- Hardware transform, with WORLD = today's matrix. With view in the matrix, float24 errors scale with distance from the camera: about 2^-16 relative, under 1/100 px [I].
- Pre-lit: lighting off, vertex colour.
- Lit: GE directional lights from the constant block. Material diffuse comes from vertex colour (d, d, d, a), where d is the PS2 normal's length baked in. This is the C2 trick: the PS2 does not normalise normals and the GE divides by |N|. The selector's matrices are rigid, so d stays static.
- 2 bones a draw, within the GE's 8.
- Fog: today's FOG1/FOG2/FOGC.
- Textures: T4/T8 pages through texcache into GE CLUT4/CLUT8 with 8888 palettes and regions extracted (REGION_CLAMP), as today.

**Invalidation.**
- Object creation 0x129128: first cook of the model.
- 0x12cd78: re-cook that object's colour bytes. How often it runs is UNKNOWN; this is the old risk #10, settled with one census run.
- Colour free 0x12cf28.
- Model memory reuse between stages: the model is checked by def pointer plus a header word sum at each use.

**Picture.** Step 1 aims at identity. Same vertex bytes, same matrices, same words: the x86 twin compares the GE list words of P04 batches with today's. Shots must equal. Lit meshes leave identity (GE lighting); they are 856 vertices, judged by shots.

**What stays.** Meshes with a +0xD0 override this frame use the builder and go down the walk. So do sorted meshes until the chain-6 CALL exists (the walk handles them as today).

### 2.2 People (P06; chain 2). Step 2

**Today [C].** C2 (`ge/vu1conv.c` p6_c2loop, `gelist.c`): WORLD identity, VIEW = the call's world, BONE0 = 32768·I and BONE1 = B. Vertex colour (d, d, d, a), emissive = the PS2's ambient, MATUPD 3. Pass 2 uses a UV offset or its own UVs; pass 3 is an env map through the texture matrix. These were proven on the real GE: gl15 shots byte-equal the master.

**Cooked format.** 20 B a vertex (UV, colour, normal s16×3, pos s16×3); skinned 24 B (two u16 weights, w·8 and 32768 - w·8).
- Pass 2 with its own UVs needs a second VB (the GE has one UV set). That adds about 80-160 KB [I].
- Cooked per model: one model has 5 people objects.

**Per frame.**
- d of the **2,188 blended vertices** depends on this frame's bones: today the converter recomputes it each frame [C code].
- The ME writes those groups' colour bytes into a double-buffered copy, using today's code: 2 × 52 KB, about 0.2-0.4 ms of ME [I].
- Without it, joints come out up to 90 levels brighter [C, earlier study]. That is a picture risk, but only an option to drop.

**Fallbacks.** Today's C2 checks (colour max, alpha ranges; `C2m.r = -9`):
- static ones at cook time;
- dynamic ones in the stub from game data.

A failing mesh uses the builder, which is game-state-only and walked as today.

**GE features.** 2 bones a group of the GE's 8. Lighting is the GE's with d in the colour (C2), and the env map is TEXMAPMODE as today.

**Picture.** Identity with today's C2 batches: list words compared on the twin, then shots.

### 2.3 Faces (EVM, P07; C1). Step 5

- C1 has been on by default since gl15 [C]: up to 8 bone slots, weights ×128, 1-4 bones a vertex.
- Cooked per model, in today's C1 vertex format; bones per frame from the slots.
- The rain wait has 1 queued. Faces matter in demo stages (96 a step in d01t [C gl26]).
- The EVM builder address is UNKNOWN; read 0x137730 when the step comes.
- E5: EVM caused two always-differing keys in the H1 round.

### 2.4 Effect prims, including the drop sheets Ffm kept (chain 6). Step 3

**What is drawn [C].**
- The close camera's drop sheets (d_splash_camera.c, fans +25f0, whole since gl17: A4).
- The foot splash at 1/2 (A3).
- Glows, smoke and quads (C4/C4X from the PS2's clip positions).
- The streaks (A1) and the rain's own sprites (A2) stay out.

**No cooking.** Prims are written by the game each frame: float positions (V4-32) and V4-16 attributes.

**Native form.**
- Stub 0x12d910. It sets its return value and the scratchpad words its caller and w00a 0x3bebf0 see (R5 LEFTOVER, E5).
- The stub records (prim, matrix, key, type).
- The ME converts vertices with today's C4/C4X and fxthin code into GE vertex arrays in frame memory, keeping today's exact screen positions (through mode). The picture then stays today's.
- The main CPU's chain-6 list CALLs them in Prim2's order.
- Later option: GE transform of the float positions, judged by shots.

### 2.5 Sea (chain 5). Step 4

- Stub 0x13bf78 with its exit state (flag bit 22; return value).
- ChainObj and PluginActor keep computing world, flags and LOD (read).
- The ME tessellates as today's lanes do. Up to 3.8% of the aft deck's pixels [C stagec].
- Option: GE Bezier patches, by shots.

### 2.6 Shadows and spot lights

- Passes 9 and 10 return at once on the deck [C], so nothing is drawn in the rain wait.
- Floor shadows are copies of floor meshes lifted 5 units (Cd - Cs×As, GEQUAL, no Z write) [C gl29]. The native path draws each decal with its base's position bytes and strip order (old design §9, rule 1). This is a property of cooking.

### 2.7 Fog

- PS2 fog is per vertex from the constant block. The GE's is per vertex from eye z: FOG1 = far, FOG2 = 1/(far - near), colour FOGC.
- Today's mapping [C gelist] is reused, with WORLD = local → view and VIEW = identity. GE cost +0.47 ms a frame [C].

### 2.8 HUD, radar, 2D, screen set-up (chains 0-1, 7-10). Step 6

- These stay on the walk until step 6.
- Chain 10 (the radar lines, stage actor S_w00a_P3a4138) is the largest walked chain after step 2: 59,568 REF qw [C dump]. Its vertices are static offset-mode V4-16 data (0xf09xxx) plus a 16-byte record per item [C mesh_packets §10]. It is cookable per stage, with a translation per item.
- Chains 0-1 and 7-9: native from FrameChanl's state (clear, scissor) and the 2D packets, or walked for good if the census finds them too cheap to matter.

---

## 3. Memory

Budget [C]:
- 51 MB user, of which the 32 MB PS2 image stays.
- Heap low point about 64 KB free (gl36).
- Frame memory 2 × 2 MB: one in the volatile 4 MB, with SPU2 RAM; arena 1,696 KB + 352 KB.
- Texture pool 2,688 KB in the heap.
- Native step 1's arrays: 2 × 108 KB outside the heap.
- Plan arrays 336 KB.
- VRAM: 688 KB free with 8888 buffers.
- ME eDRAM 4 MB, which neither the GE nor the main CPU can reach.

| Item | Size | Mark |
|---|---|---|
| Ship and props, pre-lit, per object (queue at vb 29,900) | 640 KB | C dump + format |
| Ship lit, per model | 102 KB | C dump + format |
| People, per model | 302 KB (+80-160 KB second UV VBs) | C dump + format; I |
| Cooked group lists (about 4,000 groups × about 40 B) | about 160 KB | I |
| Per-frame native lists (2 buffers) | 2 × 90-150 KB, inside frame memory | I |
| Blended d copies | 2 × 52 KB | I |
| **Deck total** | **about 1.2-1.4 MB** | I |
| Whole w00a (models that appear later) | UNKNOWN | census at several points of the stage (x86, one run) |

**Where it comes from [I].**
1. The frame arenas shrink: ship and people vertices no longer pass through them. That is about 440 KB a frame [I: 11,303 × 16 B + 6,816 × 24 B × 1.6 passes], so about 0.9 MB for both. Step 1's build logs the arena fill per frame, which settles it.
2. Native step 1's record arrays: 216 KB [C].
3. VRAM: the hottest VBs (drawn people 161 KB, drawn ship 167 KB [C dump]) fit in the 688 KB. This also cuts bus traffic (§4.4).
4. The plan arrays (336 KB) come back only when the walk ends (step 6).

**Gap.** About 0.1-0.4 MB at step 2 [I]. If the arena log shows less room, pre-lit meshes are cooked per object only for objects that are drawn, with eviction:
- A miss is cooked in the stub. That is host-only work and never changes guest writes.
- Cooking per model and copying colours each frame is not an option. The GE reads one interleaved stream, so it would mean per-frame VB copies.

---

## 4. Costs and the checkpoint

### 4.1 What the main CPU loses and keeps

Rain wait, K7 (nothing drawn, no copy plans) = **67.06 ms a step** [C gl39]: logic 34.43, pipe 25.77, gsw 4.01. K0 is 80.2 [C], of which plans and hand-over are about 7.9 [C model] and ME bus coupling about 2.4 [C model].

K7 per-function model (gl38 k38b.6, 70.3 model ≈ 67.1 PSP) [C model]:

| Goes with the full renderer | model ms | Stays (game reads it, b_proof / R list) | model ms |
|---|---|---|---|
| io_dma_start (exact VIF1 walk, SPR copies) | 6.25 (most of it) | box cull vupv_96936c0f_0 | 4.04 |
| rt_qcopy_nov (builders' copies) | 3.20 (most) | OnePieceObjs | 1.66 |
| people builder 12e8b8 | 1.14 | nat_dbc0 (sort keys) | 1.58 |
| sea ChainObj 13bf78 | 0.95 | StoreMatrixObjs (renderer's source) | 1.20 |
| WritePrimPacks 12d910 | 0.59 | SortChainObjs, ScreenObjs, Prim2Chanl, SetObjsMatrix | 2.96 |
| rt_nat_p04 (ship stub) | 0.53 | 12d688 restore, BoundCheck, CalcSortZ | 1.36 |
| rt_store (part) | about 0.6 | | |
| **Sum** | **about 11-12** | **Sum** | **about 13-14** |

Native list writing adds:
- per kind about +0.5-1 [I: about 16k GE words a picture for ship and people, about 10-20 instructions a word];
- about +2-3 for the full set [I].
The vup_33268338 slerp (1.58) is w00a game logic, not drawing.

### 4.2 Estimates for the rain wait [I unless marked; each can be off by 2x]

| State | Main CPU ms a step | fps | ME ms a step | GE ms a picture |
|---|---|---|---|---|
| Today, gl39 [C] | 78.1-80.2 | 6.2-6.4 | 70.6 busy | about 27 [C gl28: 26.7] |
| K7 floor of today's architecture [C] | 67.06 | | | 0 |
| After step 1 (ship) | 76-79 | 6.3-6.6 | 60-66 | about 27 |
| After step 2 (ship + people) | 64-72 (about 68) | 6.9-7.8 | 35-45 + d pass 0.3 | 28-30 |
| Full deck set (steps 1-6) | 54-63 (about 58) | 7.9-9.3 | 10-20 + sound 7.5 | 28-31 |
| Full set + rain deletes (dice seed jump about -3) + gl41/gl42 (-2.3..-3.9 PSP) + G1/G2 (-3.5..-5.5 model) | 39-54 (about 45) | 9.3-12.8 | same | same |
| Ceiling: drawing free on the main CPU | 52-55 | 9.1-9.6 | | |

**Basis.**

*Step 1.*
- It saves the ship's copy-plan share (about 1.3 of plans' 6.3) and part of the ME coupling (about 1).
- It adds list writing (+0.5).
- The builder is already a stub [C gl31: -3.5 then].

*Step 2.*
- The builder: -1.14.
- Chain 2's share of the walk: 2,924 of about 6,900 walked tags, 42%, so about -2.6.
- Its copies: about -1.2.
- Plans: about -1.7, by chain 2's 27% of the walked REF qw.
- Hand-over and coupling: about -1.
- List writing: +0.5-1.

*Full set.*
- K7 - 11..12 + 2..3.
- Plus the GE's bus share once a picture is drawn every step: 0 to +5. K7 draws nothing; K0 already pays it. The CPU ran at 75-83% speed while the GE drew [C].

*GE.*
- Hardware transform costs the GE nothing [C gl22].
- Skinning and lighting cost +0.145 µs a vertex-pass [C], so people add about +1.3-4.
- The GE is not the wall: 31 ms is about 32 fps.

### 4.3 Measuring the checkpoint on the PSP

**One run after step 2,** turns cycling every N steps (the `MGS2_NAT_CYCLE` pattern). The hashref comes from the x86 twin with the same cycle words, as in gl18's runs.

| Turn | Ship | People | Other chains | Picture |
|---|---|---|---|---|
| K-A | today's records + ME | builder + walk | walk | full |
| K-N1 | native GE | builder + walk | walk | full |
| K-N2 | native GE | native GE | walk | full |
| K-N2R | native GE | native GE | built by the game but not translated; ME rests | ship and people only |
| K7 | not drawn | not drawn | not drawn | none |

Ffm must be told before the run that K-N2R and K7 turns show incomplete pictures.

**Read from the log** (prof39 per function, divided by 80 vblanks: per PSP step):
- (a) K-N2 - K-A: the measured gain of two kinds [C after the run].
- (b) **P = K-N2R - (remaining builders 12d910, 13bf78, the EVM builder, the walk's io_dma_start and qcopy, all read from K-N2R's own profile) + their list writing [model about 1.5-2].** This is the full renderer's main CPU, mostly measured.
- (c) P minus the logic levers, each as measured in its own run (gl41, gl42, the rain deletes).

**Decision rule (proposed; the lead's call):**
- P ≥ about 62: the renderer cannot deliver 8 fps. Stop or rethink.
- (c) < 50: go on.
- In between: go on only with the logic plan funded.

**K-A vs K-N1** gives step 1's gain. That needs a run of its own after step 1, or is read from this run.

### 4.4 Bus and placement levers (each a switch, timed in turns)

- VBs in VRAM against VBs in RAM: affects the GE time and the main CPU's speed while the GE draws.
- Hot textures in VRAM.
- 5650 colour buffers: frees VRAM; the destination-alpha caveat in old §5 applies.
- The new list-writing code goes into the PGO training and code placement. It is new hot code, and the I-cache is the usual wall.

---

## 5. Build order

**Each step:**
- Its own switch, with the old path as its twin.
- An x86 proof before any PSP build.
- One build out at a time; the next ships only after the last log is read.

### 5.1 Proof standards

- **Strict.** The step's own hashref: x86 = emulator = PSP. The read hash is equal to the old path's on every armed window. Read counts are equal on the whole route, and IOP and frame digests are equal.
- **E5 reader.** A new hashref; the read hash may differ. Every differing read site is classified ORDER, LEFTOVER or GARBAGE-IN, with R1-R8 as the starting list. The two d01t GARBAGE-IN sites (R7, R8) are checked again on the step's own twin.

### 5.2 Steps

| Step | What | Switch (proposed) | Exactness proof | Picture check |
|---|---|---|---|---|
| 0 (container) | Cooker and list writer on x86 for P04; list-word compare with today's P04 batches on the deck route; census of w00a's whole model set and colour rewrites | none | none (host only) | list words equal |
| 1 | Ship meshes native on w00a (0x1214a0 stub → GE calls); hardware probes P1-P5 ride along | `MGS2_NAT=5` turn kind | **strict**: V1 [C]; new hashref | x86 list words equal today's; PSP shots side by side |
| 2 | People native (0x12e8b8 stub, C2 cooked, ME d pass); **checkpoint run** (§4.3) | turn kind 6 | **strict**: V3a [C today: rain read-equal]; classify vb 25,944 / 25,974 first (one READ_LOG run) | list words equal today's C2; shots |
| 3 | Effect prims (0x12d910 stub + ME conversion, chain-6 CALL order) | kind 7 | **E5**: scratchpad leftovers (R5, w00a 0x3bebf0) | identical to today's C4/C4X (same screen positions); shots |
| 4 | Sea (0x13bf78 stub with exit state) | kind 8 | **E5** (flag bit 22; R5 P331160 mask) | shots |
| 5 | Faces (EVM builder stub, C1 cooked) | kind 9 | **E5** (EVM keys of the H1 round) | shots; codec scenes |
| 6 | Chain 10 cooked; 2D native or kept; page-change stub; frame code stops building chains (R2-R4) | kind 10 | **E5** (R1-R4) | shots, HUD and radar |
| 7 | Pass-level native traversal for the passes that only build: sort chain, people loops, PluginActor's drawing part (H1's c7 set: 7.0 model ms inclusive [C model]) | kind 11 | **E5** with R1-R8 | shots |
| 8 | ME: the sound mix | own switch | sound hash / IOP equal | listening |

Steps 3-6 can be reordered by the checkpoint run's profile: take the largest measured main-CPU share first. Chains 6 and 10 each have a walk share as large as chain 2's [C dump: REF qw].

---

## 6. Risks, unknowns and the first hardware probes

| # | Risk or UNKNOWN | How to settle |
|---|---|---|
| 1 | **The checkpoint framing.** Ship + people give about 68 ms [I]; the renderer's ceiling alone is 52-55 [I]. | The lead restates the checkpoint; §4.3's run measures P. |
| 2 | **GE bus share** with a picture every step. | Probe P1. VBs and textures in VRAM as levers. |
| 3 | **Memory**: 1.2-1.4 MB cooked [I] against about 1.1 MB freed [I]; heap 64 KB. | Step 1 logs the arena fill; step 0's census sizes the whole stage. |
| 4 | **Blended normals**: identity needs the per-frame d pass. | Built into step 2; a turn without it shows the brightening. |
| 5 | **Determinism of fallbacks.** Any host-state decision that changes guest writes breaks the hash across platforms. | Stub rule §1.2; the x86 twin with a small cook cache vs a large one must give equal hashes. |
| 6 | **Stale cooked data**: 0x12cd78 rewrites, stage changes, the CLUT swap, +0xD0 overrides on other stages. | Hooks plus the header check; a debug mode re-cooks every frame and compares. |
| 7 | **The GE's guard band and near clip** on large close ship triangles. Today's batches share this, so no new faults are expected. | Shots of close camera views; probe P4. |
| 8 | **CALL depth 2** for chain-6 interleave. | Design keeps it at 2; probe P3. |
| 9 | Read differences at vb 25,944 / 25,974 (deck, first play) with both stubs. | One READ_LOG run before step 2. |
| 10 | **ME patching races**: the bind table, depth map and d must land before the kick, and frames must not overlap. | Double-buffered lists; today's hand-over gates. |
| 11 | **Texture pool at budget**: native binds go through the ME's texcache. | Unchanged from today; watch the pool log. |
| 12 | **I-cache**: new hot code on the main CPU. | PGO training includes it; per-function profile in the run. |
| 13 | **Estimates** off by 2x. | Each step's PSP run. |

**Probes for step 1's build** (off by default, each its own switch):
- **P1.** GE time and main-CPU step time with cooked VBs in RAM against VRAM, in turns.
- **P2.** List-writing time and dcache writeback per frame (timer around the stub writes).
- **P3.** A CALL two levels deep into ME-patched words: proves cache coherence between the ME, the main CPU and the GE.
- **P4.** Close ship views: shots against today's.
- **P5.** Frame-arena fill and the outside-heap block, as log lines.

---

## 7. Files

- `main/attic/ge_native/state.md`: dated working notes.
- Container, ephemeral: `/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gen/`:
  - `census.py`, `drawn.py`, `cooksize.py`, `chains.py`;
  - `ram.29900`, the RAM image of the twin at vb 29,900;
  - `out_v3a.log`, the V3a run; compare with `native/xt/out_base.log` using `native/xt/rh_stat.py`;
  - `prof_ka39_1.txt`.
