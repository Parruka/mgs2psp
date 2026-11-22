# MGS2 PSP, Phase 2: the native deck renderer on the Media Engine. Design (step 2.0)

Written 2026-11-20. Design only. Nothing here was built for the PSP.

Marks:
- **[C]** measured, or read in code or a log. The place is named.
- **[C model]** from the PSP cost model (ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000).
- **[I]** inferred. The basis is named.
- **UNKNOWN** not known. The way to find out is named.

Single estimates in this project have been wrong by 2x in both directions. Every [I] gain below is a guess until its PSP run.

Names used:
- **builder**: a game function that writes VIF1/VU1 packets (0x1214a0, 0x12e8b8, 0x12d910, 0x13bf78, the EVM builder).
- **stub**: the builder returns at entry and our hook writes a draw record instead.
- **record**: one builder call, written down by the stub (what would have been drawn, with which data).
- **snapshot**: one frame's records plus the small data copied with them.
- **walk**: today's path. The ME walks the PS2 DMA chains, unpacks VIF, runs the lanes and the translator.
- **w00a** is the deck's gameplay stage. **d00t / d01t** are demo (cutscene) stages.

---

## 0. Short answer

**Architecture.**
1. The game's own pass code stays the scene traversal. It keeps deciding what is visible, in which order, with which texture page.
2. Only the leaf packet builders are stubbed. Each stub appends one record to the frame's snapshot. This is today's reduced pipeline (0x1214a0 returns at entry on unshown steps) extended to every step and to the other builders.
3. The ME turns records into GE batches with the converters we already have (`ge/xconv.c`, `ge/vu1conv.c`), fed by pointers from the record instead of pointers parsed from a packet. The GE list writer (`ge/gelist.c`) is unchanged.
4. Kinds not native yet keep today's walk. Native and walked batches are merged in the game's order.
5. Small 2D packets (HUD, radar, the stage's post program) stay on the walk for good. They are cheap and they are GS packets, not objects.

**What I measured for this design (x86 twin, route i to vb 31,400, gl28's game settings) [C].**
- Stubbing the four leaf builders on every step keeps the IOP hash and the virtual time equal at all 314 checkpoints, and the game makes the same number of reads in every one of 31,399 vblanks. The game's control flow does not depend on the builders.
- The mesh builder (0x1214a0) and the people builder (0x12e8b8) are clean: with them stubbed the game reads the same values on the deck. Only leftover w lanes differ, in the demo stages.
- The effects builder (0x12d910) changes leftover lanes the deck's code reads (packet words left in the scratchpad). The sea builder (0x13bf78) changes a flag the game reads: it needs a real stub, not a plain return.
- Stubbing whole passes (the H1 set) is worse: the game then reads a renderer flag and scratchpad words that differ. So the seam is at the leaf builders, not at the passes.
- Details in section 1.2.

**Expected result after the full Phase 2 [I].**
- Rainy deck (w00a): main CPU 87 -> about 62-70 ms a step. ME 146 -> about 25-45 ms a picture. GE 27 -> about 28-31 ms a frame.
- The main CPU stays the wall. Its game logic alone is 49 ms a step in the rain [C gl28].
- Once every step runs the same reduced pipeline, a picture can be shown on every step at almost no main-CPU cost. That gives about **13-15 fps on the rainy deck at about 28-31% game speed** (7-8 fps if the two-step rhythm is kept). The basis and the GE's share of the bus are in 4.3. Today: 5.7 fps at 23% [C gl28].
- Quiet deck scenes (the d01t window, vb 26,300-28,700): about 19-21 ms a step, so about **23-25 fps at or near full speed**. Today: 11.8 fps [C gl28].

**Three corrections to the brief.**
- The ME's eDRAM on this PSP-3000 is **4 MB, not 2 MB** [C: gl16ep2 probe, `me_edram_probe_notes.md`; gl28 already uses 3,584 KB of it, `/home/claude/runs/gl28/log.txt:319`]. The 2 MB figure is the GE's VRAM.
- The "steady deck" window (vb 26,300-28,700) is a **demo stage (d01t)**, not gameplay [C: the game code reading there is `S_d01t_*`, my read log at vb 26,400]. It is face-heavy (96 P07 calls a step [C gl26]). The deck's gameplay load is the rain window (vb 29,300-31,300, stage w00a).
- The rain deck is **main-CPU-limited today** (87 ms a step; the ME idles 5-6 ms a step) [C gl28]. A faster picture side alone gives no fps there. The fps gain of Phase 2 in the rain comes from the main CPU's share (pipeline, copy plans) and from showing every step.

---

## 1. The seam

### 1.1 What the game reads back today [C]

From `speed_notes.md` section 31, `h1_notes.md`, `b_proof_notes.md`, `re/mesh_packets.md`:

| Fact | Number | Place |
|---|---|---|
| Renderer-only words written inside a pipeline call | 484,258 | speed_notes 31 |
| Of those, words game code reads | 1,716 | speed_notes 31 |
| Words a call reads from the call before | 24,473 | speed_notes 31 |
| Packet words kept and DMA-sent by later calls | 43,477 | speed_notes 31 |

Where the 1,716 read-back words are:
- scratchpad 0x3e70-0x3fdc and 0xcc0-0xcdc: view data, written by 0x121c30 (sort chain);
- 0x1b4700-0x1b47e0: renderer flags;
- 0x1b8b30-0x1b8be4 and 0x1b8f3c-0x1b8ff0: the packet-stream (chain) descriptors;
- 0x142e160-0x142e19c: the player's part 21 world matrix, written by 0x11fde8;
- 0x165d1f0-0x165d454: part 0 world, written by 0x11fa58;
- 0x1e50a0: the packet write pointer.

There is a second, hidden path [C b_proof_notes]: pipeline functions leave values in VU0 registers and in scratchpad quadwords. Game code later stores or copies whole quadwords of which it wrote only some lanes. The unused lane (mostly w) then carries the pipeline's leftover. This is why a stub changes the EE hash without changing what the game does.

### 1.2 The container experiment for this design [C]

Build: a copy of the x86 twin with one added switch, `MGS2_LITE_ALL=<vb>`: from that vblank every pipeline call is "reduced", so the functions of `MGS2_LITE_FN` return at entry on every step. Files: section 10. Base = the same binary without the switch; it equals `cfg/hashref_g22b.txt`.

Two checks:
- **Hash lines** every 100 vblanks: ee, iop, virtual time (`k_time`, the count of game loop back-edges).
- **Read hash** (`MGS2_READ_HASH`, the tool H1 used): every vblank, a hash and a count of every value game code loads outside the pipeline. Equal count = same path through the code. Equal hash = same values.

| Run | Returns at entry, every step from boot | rc | iop and time, 314 checkpoints | Read count | Read values |
|---|---|---|---|---|---|
| V1 | 0x1214a0 | 0 | equal | equal in all 31,399 vblanks | deck from vb 24,000: equal in 7,397 of 7,399 vblanks. Opening demos: differ in 3,785 vblanks |
| V3a | 0x1214a0, 0x12e8b8 (run to vb 29,403) | 0 | equal | equal in all 29,403 vblanks | **the same vblanks as V1, none added** |
| V3b | 0x1214a0, 0x13bf78 (run to vb 29,403) | 0 | equal | equal in all 29,403 vblanks | 753 vblanks more than V1, every rain vblank among them |
| V3 | 0x1214a0, 0x12e8b8, 0x13bf78 | 0 | equal | equal in all 31,399 vblanks | differ in 2,018 of 2,099 rain vblanks (the sea builder's, as V3b) |
| V4 | V3 + 0x12d910 returning 0 | 0 | equal | equal in all 31,399 vblanks | differ in 8,455 vblanks: V3's, plus about 2,000 demo vblanks and 22 in the talk |
| V2 | 0x1214a0 + whole passes 0x133cc0, 0x1324f0, 0x138ac8, 0x138ae0, 0x130390, 0x1303a8, 0x13d540, 0x122138 | 0 | equal | differs in 21,012 vblanks | differ in 21,299 vblanks |

What differs, by reader (read logs at vb 4,922-4,925, 23,844, 23,998, 25,944, 25,974, 26,400-26,401, 29,400-29,403):

- **V1.** Only leftovers.
  - Opening demos: w lanes read by d00t 0x323c38, 0x323b58, 0x2513a0 (the demo's particle code); a few y and zw lanes of scratchpad quadwords in the same code.
  - The four vblanks near stage changes (23,844, 23,998, 25,944, 25,974): w lanes only, reader d01t 0x329668. This is the copy path `b_proof_notes.md` already names.
  - Deck gameplay (w00a) and the d01t talk: nothing.
- **V2 (whole passes).** Not clean.
  - Frame code (0x123b28, 0x124180, 0x123f18) reads a different chain count: 3 reads fewer a vblank. Renderer code, harmless.
  - w00a 0x331160 reads the flag word 0x1b4818 with bit 22 different. V3b shows this one comes from the sea (0x13d540 is in the set).
  - w00a 0x3bebf0 reads scratchpad 0x70003ec0 (the view data area, written by the sort chain's 0x121c30) with other z and w lanes. d01t 0x276b38 reads scratchpad quadwords with other x, y, z lanes.
- **V3a (people builder).** Clean. With 0x12e8b8 stubbed as well, the differing vblanks are exactly V1's; the read logs at vb 26,400-26,401 and 29,400-29,403 show no differing read. Coverage: boot to vb 29,403 (the opening, the deck's first play, the talk, the first 103 rain vblanks). The rest of the rain window is covered only by V3, where the sea builder's flag hides it: run V3a to vb 31,400 before the people build (7 minutes).
- **V4 (effects builder added).** Leftovers at the sampled vblanks, but more of them. In the rain w00a 0x3bebf0 reads scratchpad 0x70003ec0 63 times a vblank: its x and y lanes (floats) are equal, its z and w lanes hold VIF command words (0x6c0880e8, 0x01000101) in the base and 0 in V4. Those are packet words the builder left in its scratchpad buffer. Plus 4 w lanes a vblank through unaligned loads. Nothing differs at vb 26,400-26,401. Only 6 vblanks were sampled: the effects build needs the whole-route classification (section 3.1, step 3).
- **V3b (sea builder).** Not clean. **w00a 0x331160 reads the flag word 0x1b4818 with bit 22 different** (0x00401041 against 0x00001041), once a vblank. That code writes the word itself, from something the sea's builder gives back (its caller reads v0 after the call). So 0x13bf78 cannot simply return at entry: its stub must return what the builder returns, and the builder's own visibility work may have to stay.

**Reading.**
- The game's control flow does not depend on the builders. Four builders off on every step for 31,400 vblanks: same IOP state, same virtual time, same number of reads in every vblank.
- The mesh builder and the people builder are clean stubs. The sea builder is not, as it stands.
- Stubs do change leftover lanes. So the EE hash of a build with a new stub set differs from the build before. This is the same class as today's reduced pipeline. The rule stays: **the twin, the emulator and the PSP run the same stub set and must give identical hashes**; the twin's hash file is made again for each stub set.
- **Pass-level cuts are out.** They change scratchpad values and the chain count the game's code reads. H1 found the same (98 reader keys).

### 1.3 What keeps running on the main CPU, what stops

Pipeline entry 0x11da90, end 0x11ee98. Names from `re/dg_objects.md` and the reference's list (names only).

| Function | Job | Verdict | Why |
|---|---|---|---|
| 0x12a850 | texture page chain sources | keeps running | cheap; feeds the page order |
| 0x128d90 -> 0x12cd78 | static vertex lighting into colour buffers | keeps running | the native path reads those colours |
| 0x12d560 -> 0x12d378 | dynamic point lights, colours into packet memory, mesh+0xD0 | keeps running | same |
| 0x12d770 -> 0x12d688 | stream restore | keeps running | 1,710 words read by the next call [C] |
| 0x11e5e0 DG_SortChanlSystem | channels and passes | keeps running | the traversal |
| 0x11f4d0 FrameChanl (pass 1) | screen set-up, clear | keeps running | small GS packets; one of the 12 read-back functions |
| 0x121060 ScreenChanl (pass 3): 0x120538, 0x11fa58, 0x11fde8 | transforms, culls, bone matrices | keeps running | the game reads the matrices; cutting it broke 1,993 frames [C h1_notes] |
| 0x120860 StoreMatrixObjs | the 384-byte constant block per visible mesh | keeps running | read by 0x122608 [C]; it is also the native renderer's matrix source |
| 0x120f00 | twin copy | keeps running | reads 0x11fde8's bones |
| 0x1226c8 -> 0x122608 (pass 6) | opaque mesh loop | keeps running | calls the builder per mesh |
| **0x1214a0** | per-group mesh packets | **stub** | V1 |
| 0x130390 / 0x1303a8 -> 0x12f130 (passes 4, 7) | people loop | keeps running | 0x12f130 is a read-back function |
| **0x12e8b8** | P06 packets (Snake, guards, crew) | **stub** | V3a |
| 0x138ac8 / 0x138ae0 -> 0x137730 (passes 5, 8) | faces (EVM) loop | keeps running | |
| **the EVM builder** inside 0x137730 | P07 packets | **stub**, later | address: UNKNOWN, read `re/mesh_packets.md` and the code when the kind is built |
| 0x133cc0 / 0x1324f0 (passes 9, 10) | shadow and spot projection | keeps running | they return at once on the deck [C dg_objects pass 9; b_proof row H] |
| 0x139180 hooks: 0x135338, 0x13d540, 0x1394a0, 0x1397a8 | plug-ins: prebuilt packets, the sea, others | keeps running | read-back functions |
| **0x13bf78** | sea patch packets | keeps running; **stub later, with its return value** | V3b: the game reads a flag that depends on it |
| 0x12dc90 Prim2 (pass 12): 0x12dbc0 keys, radix sort | effects' depth sort | keeps running | cutting it broke 5 frames [C h1]; its VU0 results reach the game [C b_proof] |
| 0x122138 -> 0x121c30 (pass 13) | sorted loop, view data to scratchpad | keeps running | the game reads the view data and a flag (V2) |
| **0x12d910** | effect primitive packets | **stub with its return value** | the caller uses v0 (0x121d5c) |
| 0x124d48 | texture page change packets | keeps running at first | tiny; its return value is the packet pointer. A stub candidate later: its packets make the walk parse the page uploads |
| 0x12a8c8 | clears page+0x344 | keeps running | |
| frame code: 0x124450/0x124460, 0x124380, 0x1238e8, 0x123e80 | packet pointer, chain list, DMA kick, buffer swap | keeps running | the chains still exist, they are just short |

What stops on the main CPU when every deck kind is native:
- the packet building in the builders and their scratchpad-to-packet-memory DMA flushes;
- almost all VIF1 traffic of the frame (the model's "DMA kicks and copies", 6.6 of 32.8 model ms a step [C model, deck_costs]);
- the walk's records and copy plans: 5.4 ms a step in the rain, 4.3 in the d01t window [C gl28, "GS walk" column];
- the wait for the ME's walk: 17 ms a step in the d01t window, about 0 in the rain [C gl28, "GE wait" column].

What stops on the ME: the DMA chain walk, the VIF unpack, the texture upload parsing, the GS front end for VU1 output, the lane recognisers. In gl26's split that is "rest" 31.45 of 65.08 ms a step in the rain, plus the lanes' own work [C gl26].

### 1.4 Stub rules

1. A stub is installed where `GEN_LITE` installs today's (`boot/gen_pre.h` FN_ENTER, `boot/dg_lite.h`). No new mechanism in generated code.
2. A stub writes **host memory only** (the snapshot). It never writes guest memory or guest registers, except a return value the builder's caller reads. So the game state depends on the stub set, never on the snapshot or on the ME.
3. A stub whose builder returns a value gets that value fixed by reading the builder. 0x12d910: V4 ran with 0 and the control flow held; whether 0 is what the builder returns when it writes nothing is UNKNOWN: read its exit paths before the build. 0x13bf78: a plain return changes a flag (V3b).
4. Each stub set gets its own x86 proof before any PSP build: hash lines against the twin, read-hash against the build before, differing readers classified (section 3.1).
5. A model the native code does not know falls through: the stub returns 0 and the original builder runs. The object then goes down today's walk. This is decided per call on the main CPU, from a per-model class byte (section 7).

### 1.5 The snapshot

A record is the builder's call written down. The lanes show what a call needs [C `ge/lw_lanes.h` lw_p04, lw_p06; `re/mesh_packets.md` section 3]: a 0x1214a0 group is five pointers (constant block at mesh+0xFC, material, streams A, B, C) and two header quadwords (count, fog word, clip, VU1 entry index). The lane's converter `xl_lane_p04` takes exactly those. So the record carries the same pointers and the same header values, and the ME calls the same converter.

Per frame, written by the main CPU inside the pipeline call:

| Part | Content | Size | Written at |
|---|---|---|---|
| Header | frame vb, counts, switch word, which channels are on | 64 B | entry of the pipeline call |
| Channels | the chanl struct of each enabled channel (VP, V, P, S, biased P', viewport, clear mode; 0x4A0 B each [C dg_objects]) | 2.4 KB for 2 channels | return of pass 3 (0x121060) for that channel |
| Records | one per builder call, 32 B: kind, pass, channel, order key (the packet write pointer at the call), mesh or prim pointer, group or count, header words | about 400-550 a picture in the rain = 13-18 KB [I from C: per step, vb 29,900-30,100, 147 calls of 0x1214a0, 44 of 0x12e8b8, 2 of 0x13bf78 (my count runs c1, c2); 0x12d910 170 a vblank on the route's average (V4), its rain figure UNKNOWN: same count run with 12d910 in the list] | each stub |
| Constant blocks | the 384-byte block 0x120860 wrote for the mesh, copied once per mesh and channel | 147 + 44 blocks = 73 KB in the rain [I from the counts] | first record of the mesh |
| Lights and bones | nothing extra: they are inside the constant block, where lanes 2 and 4 read them today [I: `ge/lw_lanes.h` lw_p06 takes them from the packet's blocks] | 0 | - |
| Page state | per record: page id and the page's one-frame palette pointer (page+0x344, cleared at the end of the call) | in the record | each stub |
| Effects | per 0x12d910 record: type, count, set index, the two vertex stream pointers, texture, blend | in the record (+16 B) | each stub |

Total: about **90-110 KB a picture** in the rain [I]. Two snapshot buffers of 128 KB (one being written, one being read).

In the d01t talk window the same count gives 0 calls of 0x1214a0 and 1 of 0x12e8b8 a step (vb 26,400-26,600 [C runs c3, c4]): that scene is faces only.

Not copied, read by the ME from game RAM:
- static model data: streams A, B, C, group descriptors, materials (they do not change between vblanks [C mesh_packets 3]);
- texture pages (the texture cache already reads them);
- effect vertex sets (the game double-buffers them, set index obj+180 [C pipeline_hot]);
- colour buffers (mesh+0xF0) and dynamic colour streams (mesh+0xD0 override).

Cost on the main CPU [I]: 90-110 KB copied from lines the pipeline just touched, 0.3-0.7 ms a picture (basis: 330-660 ns a 64-byte line, gl28 log line 107), plus about 500 appends. About **0.2-0.4 ms a step** at one picture per two steps.

Why copy the constant blocks instead of leaving them in packet memory: packet memory alternates per vblank, so a block lives until the pipeline call two steps later. Copying removes that deadline and costs about 0.2 ms a picture. If the PSP run shows the copy matters, the ME can read the blocks in place under the hold rule of section 4.3.

### 1.6 Order

Native and walked batches must come out in the game's order.
- Inside one builder's calls: the stubs append in call order. Nothing to do.
- Between chains: the frame has chains 0-10 in a fixed order [C mesh_packets 1]: 0, 1 screen; 2 people; 3 opaque meshes; 4 prebuilt (P01); 5 sea; 6 sorted prims and meshes; 7-9 small 2D; 10 the stage program. A record carries its pass, so its chain.
- Inside chain 6 (sorted prims and sorted meshes mixed back to front): the record's order key is its position in the chain's packet stream at the call: the packet write pointer plus the bytes the builders still hold in their scratchpad buffer (they flush it to packet memory by DMA [C mesh_packets 1]). The walk knows each tag's address. Records with a key at or below the next tag's address are emitted first.
  - UNKNOWN: that this position rises through chain 6 and matches the tags' addresses. Check: the order compare in shadow (section 3.2), one x86 run. If not, the key becomes a count of sorted items, kept by a hook in 0x121c30's loop and counted again by the walk.

---

## 2. Object kinds on the deck

### 2.1 Cost today, ranked [C unless marked]

Rain window, ME walking, per step. The ME's numbers are gl26's split and include about 6.2 us of timer cost per callback.

| # | Kind | ME today, ms a step | Main CPU today | Lane calls a step [C gl26] |
|---|---|---|---|---|
| 1 | Shared walk ("rest": chain walk, VIF unpack, upload parsing, translator) | 31.45 | records and plans 5.4 [C gl28]; DMA kicks and copies 6.6 model ms [C model] | - |
| 2 | People (P06: Snake, guards, crew) | 12.3 (lane 4.25 + held flushes 1.54 + calls that ran held calls 6.51) | builder 1.8 model ms [C model] | 143 |
| 3 | Effects (sorted prims: rain, splashes, glows, smoke) | 9.94 | part of the sort's 5.1 model ms; the builder's share UNKNOWN | 154 |
| 4 | Ship meshes (0x1214a0, opaque and sorted) | 8.95 | 3.8 model ms [C model] | 236 |
| 5 | The frame's own work (frame end, GE list) | 4.1 [C gl28] | - | - |
| 6 | Passes 2-3 of two-pass draws, GS prims (HUD, radar, post) | 1.75 + 0.53 | - | 138 + 37 |
| 7 | No lane (sea, prebuilt packets, others) | 0.16 + part of "rest" | - | 215 |
| 8 | Faces (P07) | about 0 in the rain; 3.25 in the d01t window | - | 96 in d01t |

The shared walk is the largest line and belongs to no kind. How it falls as kinds leave is **UNKNOWN**; step 1's PSP run measures it (arm C, section 6).

### 2.2 How each kind is drawn natively

**A. Ship meshes (0x1214a0; pre-lit +0xdf0 / +0x1088, lit +0x780 / +0xa70).**
- Record: mesh, group descriptor, the five pointers, the header words, the VU1 entry index (the rule of `re/mesh_packets.md` 3.1).
- GE: hardware transform, s16 positions and UVs, 8888 vertex colours, linear fog from the two VU constants, GEQUAL depth, the two-pass alpha rule where a texel is under 0x40. All of it exists as GERK_XFORM batches.
- Lit meshes (+0x780): today's converter handles them; unchanged.
- ME cost [I]: first build, no cache: about today's lane cost per vertex without the walk, 10-14 ms a picture. With the vertex cache (section 4.4): 2-5 ms a picture.
- Main CPU: stub 0.1-0.2 ms a step [I]; saves the builder on shown steps.
- GE: the same batches as today, so the same GE time.
- Verify: batch identity with lane 1 (section 3.2). Decals: section 9.

**B. People (0x12e8b8; P06, 1-3 texture passes).**
- Record: mesh, group, constant block, three materials, six streams (A-F), header. Copied: the constant block (bone and light data are in it).
- GE: the C2 path. Two-bone hardware skinning, hardware lighting (three directional lights plus ambient, mapping (b)), pass 2 as a second texture pass, pass 3 as an environment map from the skinned normal (TEXMAPMODE 3). It is built and exact on the real GE [C gl15].
- Why C2 saved no CPU in gl15 (+0.7-0.8 ms on the deck [C]): the packet path stayed. The cost is in the walk and the held calls, not in the vertex math. The native path removes them.
- ME cost [I]: 3-6 ms a picture with cached vertex buffers (about 50 calls, the bone and light words per call).
- GE cost [I]: +1.3-4.0 ms a frame (about 9,200 vertices a frame [C census vb 24,050] x 1-3 passes x 0.145 us [C gl22, C1's figure]).
- Verify: batch identity with lane 2 in its C2 mode on x86; then shots on the PSP (GE lighting is not the PS2's bit for bit; the picture rule of section 3.3 applies).

**C. Effects (0x12d910; DG_PRIM types 1-5: lines, quads, sprites, rotated quads, strips).**
- Covers: rain streaks, splashes, lamp glows, smoke, muzzle flashes, blood, ropes.
- Record: prim object, type, count, the two vertex streams of the current set, texture, blend, the channel's biased projection.
- GE: the C4 / C4X converters (GERK_XFORMF batches, sprites, line runs), hardware transform, no lighting, blending by the existing table. Triangles crossing the GE's 0..4096 window take the existing clipper path.
- The effect actors and the depth sort keep running on the main CPU (they share the stage's random seed with the guards).
- ME cost [I]: 8-14 ms a picture (about 300 calls; per-vertex reads of the float positions).
- GE: unchanged. Effects are 71% of the pixels written (fans 40%, rain sprites 31%) [C census vb 24,056, before the rain cuts].
- Verify: batch identity with lane 3; the destination-alpha blend checked on the PSP (section 5).

**D. Sorted meshes (0x1214a0 called from 0x121c30).** As A, placed by the order key.

**E. Faces (EVM, P07).** As B with up to 8 weights (C1 is on by default today [C]). Needed for the d01t window, not for gameplay.

**F. Sea (0x13bf78; two layers of 225 Hermite patches, LOD 0-4, crack mask).**
- On the deck at vb 24,056 the sea wrote 0 pixels [C census]. It still costs packets.
- Step 1 of the kind: a box test on the ME; skip the layer when its box is behind the hull or off screen. Step 2: patches as triangle strips tessellated on the ME (scalar float) or as GE Bezier patches.
- Its stub is not a plain return (V3b): the game reads a flag that depends on what the builder returns. Read 0x13bf78 first; the stub keeps the builder's visibility and LOD work and drops only the packet stores.
- Cost: UNKNOWN (no timer splits the sea out; 2 builder calls a step in the rain [C count runs]). Low priority; it stays on the walk until then.

**G. Prebuilt packets (P01, hook 0x135338).** Static packets, 0 pixels at vb 24,056 [C]. Stay on the walk. Later: converted once and cached by address and size.

**H. HUD, radar, font, the stage's post program (depth blur, trail, radar box).**
- These are GS packets written by game and stage code (chains 0, 1, 7-10). There is no object to draw.
- They stay on the walk for good. Cost today: GS prims 0.53 and passes 1.75 ms a step [C gl26]. Without VIF1 bulk the walk should cost 2-4 ms a picture [I].
- The depth blur stays as built (16-row tiles). The trail is off (G1).

**I. Shadows and decals.**
- Projected shadows (passes 9, 10) do nothing on the deck [C]. Not a deck kind.
- The deck's floor shadows are decal meshes: kind A with the rule of section 9.

**J. Glows that use the destination's alpha** ((Cs - 0) x Ad + Cd, about 600 fan triangles a vblank [C census]). Kind C. They need an 8888 target; see section 5.

### 2.3 Which kind first

By ME cost alone people come first (12.3), then effects (9.9), then ship meshes (9.0). I still put **ship meshes first**:
- Its stub is the one already shipping on unshown steps, and V1 shows the deck's reads are unchanged with it on every step. No new exactness risk in the first build.
- Its output can be made bit-identical to today's lane 1, so the first build tests the seam, the snapshot, the hand-off and the merge without any picture question.
- It carries the decal lesson.
- The gap to people is small (9.0 against 12.3), and the rain is main-CPU-limited, where meshes save more (3.8 against 1.8 model ms).

If the lead prefers strict cost order, people can go first. The seam proof for 0x12e8b8 is V3a.

---

## 3. Exactness and verification

### 3.1 Game state

Rule: state hashes identical on x86, the emulator and the PSP for the same switches.

For each new stub set, in the container, before a PSP build:
1. Twin run with the stub set and the build's turn schedule: this makes the hash file.
2. Read-hash run against the build before (recipe: `xt/rh.sh`, `tools/dev/rh_cmp.py`). Pass: equal read counts in every vblank.
3. Classify every differing read. Pass: a leftover lane of a quadword the reader did not fully write, or renderer frame code. Fail: a flag, a pointer or a value lane read on purpose (as the sea's 0x1b4818).
   - Quick form, used here: read logs at sampled vblanks (`xt/rl_lanes.py`, `xt/rl_lanes2.py`). It sampled 14 vblanks.
   - Complete form, for a build: the per-reader-lane and per-store-lane sums of `b_proof_notes.md` (rv, wv) over the whole route. One run lists every reader and lane that ever differs.
4. The emulator run and the PSP run must print the twin's hashes.

A stronger option if a leftover ever matters: the stub sets the VU0 registers the builder would leave (H1's "work owed"). Not planned.

### 3.2 The picture: identity first

Each kind has three states (a two-bit field per kind in one switch word, `MGS2_NAT`):
- **off**: today's path.
- **shadow** (x86 and emulator only): the builder runs, the hook also writes the record, the ME converts both. The native batch is compared with the lane's batch for the same call: state fields, matrices, vertices, strips. Nothing native is drawn.
- **on**: the builder is stubbed, the native batch is drawn.

Acceptance of a kind on x86: in shadow, **every native batch equals its lane batch** on route i and route w01a, and the merged batch order equals today's order. Then "on" draws the same picture as today by construction, and the frame digest (`dig_frame`) of an "on" frame equals the digest of the same frame "off" in a second run with the opposite turn phase.

This is possible because the record holds the same pointers the lane reads from the packet, and the converter is the same code.

### 3.3 The picture: where identity ends

Changes that are meant to differ from today's path get the picture rule (no missing or misplaced objects, the same look at a glance):
- the decal rule (section 9);
- depth range from boxes instead of vertex scans (section 4.4);
- anything in section 5 (5650, lower-resolution effects).

For those:
- **Shot**: the same frame from the same run in both turn phases, on PPSSPP's reference build and on the PSP (gl28's R-trigger "eye" shots).
- **Metric**: per frame, the share of pixels whose largest channel difference exceeds 16 levels, native against today's path, on the emulator's software renderer. Target: under 0.5% of the pixels, and no connected region over 64 pixels. Regions over the limit are looked at by eye and named in the build's notes.
- **Object census**: the count of batches per kind and their screen boxes must match between the two paths. A missing object shows here even when the metric is small.

### 3.4 Fallback

- Per kind: the switch word. A PSP build carries off and on and swaps them in turns of 100 vblanks.
- Per object: the class byte (section 7). Unknown models take the walk.
- Per run: `MGS2_NAT=0` is today's build.
- If the ME hangs: the heartbeat and the take-back already exist [C `ge/xme.h`]. After a take-back the main CPU runs the same loop entries itself, as it does today for the walk. The native converters are plain C, so the frame whose builders were already stubbed can still be drawn.

---

## 4. The two CPUs

### 4.1 Split

| Main CPU | ME |
|---|---|
| game logic, all actors | records -> batches (the converters) |
| the reduced pipeline: transforms, culls, lighting, sort | the vertex cache: conversion on a miss |
| stubs: records and the small copies | the texture cache |
| GE list submit, display, timers (system calls) | the GE list (`gx_frame`) |
| the hold at the hook (4.3) | the walk for the kinds still on it |

The GE does all per-vertex math: transform, skinning, lighting, fog. The ME has no VFPU.

### 4.2 ME rules the new code must keep [C: `me_handoff_design.md` 4.5-4.9, `ge/xme.h`, the gates]

- No system calls, no libc, no printing, no VFPU. Own memcpy and heap (`me_rt`). `-G0`, `-fno-jump-tables`, indirect calls through `ME_CALLP`.
- The four gates pass: `mesec_chk.py`, `xme_me_chk.py`, `me_call_chk.py`, `xme_rest_chk.py`.
- Floats: FCR31 0x01000001, results bit-identical to the main CPU's [C probe 1]. The x86 check build with `MGS2_FPU_RZ` gives the same words.
- Caches do not see each other. The main CPU writes its cache back before a post (`sceKernelDcacheWritebackAll`, as today [C me_step4_notes]). The ME invalidates what it is about to read and writes back what the GE or the main CPU will read. Shared words go through the uncached alias (`XS()`).
- An exception on the ME is a silent hang. So: every guest pointer in a record is range-checked against EE RAM and the scratchpad, every count is capped, before use. A bad record is dropped and counted.
- PPSSPP has no ME: the loop runs as a main-CPU thread there.

### 4.3 Hand-off and the two-step rhythm

New queue entry `XE_SNAP` beside today's (`ge/xme.h`). Per drawn step N:

1. **Pipeline call, main CPU.** Stubs fill snapshot buffer k. Records are posted to the ME in chunks of 32 so it starts while the pipeline still runs.
2. **End of the call.** The main CPU closes the snapshot, writes back its cache, posts the last chunk.
3. **RAM phase, ME.** For each record: cache lookup; on a miss, read the model's streams and convert. Read the effect vertex sets, the colour buffers, the one-frame palettes. When done, the ME sets `ram_done = N` (uncached word).
4. **Hook that ends step N, main CPU.** It waits until `ram_done = N`. Then step N+1 starts.
5. **List phase, ME.** No game RAM is read any more. Batches, `gx_frame`, the result slot. It overlaps step N+1.
6. **A later hook, main CPU.** Enqueues the list, flips. As today (two frames on the GE side, `ge2`).

Why the hold in step 4:
- Every byte the picture is made from is read at a defined time. The frame digest is then the same on x86, the emulator and the PSP.
- An object freed in step N+1 cannot be read after its memory is reused.

Expected hold [I]: 0-2 ms a picture once vertex buffers are cached (the ME's RAM phase overlaps the second half of the pipeline call and the IOP's 2.5 ms). In step 1, without the cache, the ME converts every mesh in the RAM phase (10-14 ms a picture [I]); the hold may then be 2-8 ms a picture. It has its own timer. If it exceeds 1 ms a step, the effect sets move to the weaker deadline they allow (the game leaves a set alone for one more step).

Unshown steps: stubs return at entry and write nothing. Same game state as a shown step.

**A picture on every step.** With all kinds native a shown step costs the main CPU only the snapshot (0.3-0.6 ms). The rhythm becomes a switch: draw every step when the ME's picture and the GE's frame each fit one step, else every second step. In the rain a step is 62-70 ms [I], the ME needs 25-45 [I], the GE 28-31 [I]: every step fits. At full speed (20 ms a step) it is every second step: 25 fps.

The cost of it: the GE then works 40-50% of the time instead of about 15% today (26.7 ms in 174). The main CPU kept 75-83% of its speed while the GE drew [C native_port_answer]. So expect the step to grow 5-9% [I]: 65-76 ms, 13-15 fps. Step 5's turn pair measures it.

### 4.4 The vertex cache (its own step)

- GE-format vertex buffers kept across frames, owned and written by the ME, read by the GE.
- Key: stream pointers, counts and the group descriptor; plus a generation number per object.
- Invalidation: hooks on the main CPU at object creation (0x129128), at the colour buffer's free (0x12cf28) and at stage load post "drop this range" records. A stage load drops everything.
- Proof of the rule, x86 only: a check mode converts every cached mesh again each frame and compares. Zero differences on both routes before a PSP build.
- Size [C stagec_notes step 3]: the deck window has 5,675 keys, 2.67 MB at 16 B a vertex; 512 KB serves 97.7% of the draws.
- With cached buffers the per-vertex scans go. Depth range, off-screen cut and the guard-band class come from the model's box (8 corners through the mesh's matrix). The depth map then differs slightly from today's: picture rule, not identity.
- Colours: pre-lit meshes whose colour buffer the game rewrites cannot stay cached as they are. How many deck meshes change colours per frame is UNKNOWN. Check: x86 census hashing each drawn mesh's colour stream per frame.

### 4.5 Memory [C gl28 log lines 245-247, 311, 317-319 unless marked]

| Area | Size | State today |
|---|---|---|
| Heap | 8,016 KB, 7,989 in use | 27 KB free |
| Outside the heap at start | 764 KB, largest block 512 KB | 304 KB taken by the plan arrays; about 460 KB left [I] |
| Frame memory 1 (in the volatile block) | 2,048 KB | arena 1,696 KB, list and aux 352 KB |
| Frame memory 2 (heap) | 2,048 KB | same |
| Texture pool (heap) | 2,688 KB | budget 2,352 KB |
| ME eDRAM | 4,096 KB [C probe 2] | copy arena 3,072 KB + 4 view buffers of 128 KB |
| VRAM | 2 MB | 688 KB free with 8888 buffers; 1,232 KB with 5650 [C plan 2.5] |

Facts that bind:
- The main CPU cannot reach the eDRAM [C]. The GE cannot read it either [I: it is on the ME's bus]. So lists, vertices and textures stay in main RAM or VRAM. The eDRAM holds ME-private data only.
- eDRAM is the faster place for what the ME reads often: one load a line 82 ns against 168.5 ns in main RAM [C probe 2].

Plan:
- Step 1 needs 2 x 128 KB of snapshot. From the memory outside the heap (460 KB left [I]; the log line of the build says the true figure).
- The ME copies each snapshot into eDRAM at the start of the RAM phase and works from there. The eDRAM has room: the copy arena can shrink as kinds leave the walk.
- The vertex cache (512 KB - 1 MB) comes from the frame memories: native frames carry no converted mesh and people vertices, so each arena can shrink by about that much. UNKNOWN until step 1 logs the arena's fill per frame with the mesh data gone.
- Second source: VRAM after 5650. Whether the ME can write VRAM is UNKNOWN; a one-word probe in step 1's build settles it (the ME writes a pattern at a free VRAM offset, the main CPU reads it back).
- When the copy plans stop: the plan arrays' 336 KB come back.

---

## 5. GE budget

Measured [C]:
- Rain, both CPUs busy: 26.7 ms a frame (1,310.0 ms for 49 lists, gl28 log line 2549).
- Deck, gl22: 21.8-27.0 ms a list.
- Before the rain was halved: 38.9 ms CPU idle, 41.8-45.1 CPU busy.
- Hardware transform costs the GE nothing over through mode (-0.28 ms for 1,439 calls, 32,118 vertices, gl22). Fog +0.47 ms. GE skinning and lighting +0.145 us a vertex-pass.

Budget: 40 ms a frame for 25 fps, 50 ms for 20 fps. The GE runs beside the CPUs, so its time is not added to theirs; it only has to fit the frame period.

After Phase 2 [I]: 26.7 + 1.3-4.0 (people lit and skinned by the GE) = **28-31 ms a frame**. It fits 40 ms with 9-12 ms to spare, and 50 ms easily.

Levers, in the order I would try them, each its own switch, timed in turns:

| Lever | Expected | Blocker or question |
|---|---|---|
| Vertex buffers and hot textures in VRAM | less traffic on the bus the main CPU shares [I; lead: psp-developer, texture fetch traffic] | VRAM free space; the ME's access to VRAM (UNKNOWN, 4.5) |
| 5650 colour buffers | frees 544 KB of VRAM [C plan]; less frame-buffer traffic [I] | the destination-alpha glows (kind J) lose their Ad factor; the depth blur and frame copies sample the buffer. Which effect uses Ad: UNKNOWN; find it with `tools/gsref.c` on the vb 24,056 capture. Dither speckles on additive glows [lead] |
| Depth blur at half width, or off | the post program is 13% of the pixels [C census] | a visual change: the lead's call, with shots |
| Fewer overdraw layers | effects are 71% of the pixels [C census, before the rain cuts] | A1-A3 already thin the rain |
| Index-free arrays | cached buffers are stored in strip order, one PRIM a strip, no index list | none; it comes with the cache |

The GE budget is not the wall in any window measured so far. It becomes the wall only at full speed, where 25 fps leaves it 40 ms.

---

## 6. Build order

Before any PSP build, step 0 in the container:
- the stub hooks in "shadow" state on the x86 twin (record, do not return);
- the native mesh front end calling `xl_lane_p04`'s converter from a record;
- the batch-identity check, the order check, the chain 6 address check;
- the read-hash proof of the build's exact stub set.

Each PSP step below is one build with turns of 100 vblanks. Each kind has its switch. Today's path is the fallback and the reference. Every expected gain is [I]; "rain" means the w00a rain window, today 87.2 ms a step main, 73.1 ms a step ME [C gl28].

| Step | Content | Turn arms | Expected, rain, per step [I] | What the run decides |
|---|---|---|---|---|
| 1 | Seam, snapshot, `XE_SNAP`, the hold. **Native ship meshes**, opaque and sorted together: that is exactly V1's stub pattern. (Opaque only would be a different pattern, 0x1214a0 stubbed by caller, and needs its own proof.) No cache. The ME-to-VRAM probe. | A today; B meshes native; C meshes stubbed and not drawn (timing only, the picture is incomplete in C) | main -3 to -7 (builder on shown steps, fewer records and plans); ME -6 to -14 | D1, D2 below |
| 2 | **People** (0x12e8b8) with the GE's skinning and lighting | A; B people native | main -2 to -4; ME -10 to -16; GE +1.3 to +4 ms a frame | D3 |
| 3 | **Effects** (0x12d910), after its whole-route classification and its return value | A; B effects native | main -2 to -5; ME -8 to -14 | D4 |
| 4 | **Vertex cache**; depth range from boxes; frame arenas shrunk | A no cache; B cache | ME -6 to -12 a picture; hold near 0 | D5 |
| 5 | **A picture every step**, as a switch | A two-step; B every step | fps x2 where the ME and the GE fit a step; main +5-9% from the GE's share of the bus | D6 |
| 6 | Faces (EVM); the sea with its real stub and a box test; prebuilt packets | per kind | d01t window: the ME from about 70 to 20-30 ms a picture | - |
| 7 | **Copy plans and VIF1 records off**, once no VIF1 kind is left on the walk | A plans; B none | main -4 to -5 | - |
| 8 | GE levers of section 5, only if a run shows the GE over 36 ms a frame | per lever | GE -2 to -8 ms a frame | - |

After step 7 [I]: rain main about 62-70 ms a step (logic 42-47, the remaining pipeline 17-20, IOP 2.5, snapshot 0.5). Basis:
- pipeline 29.5 ms [C gl28] minus the builders and the DMA share, 11-13 of 32.8 model ms [C model, deck_costs];
- plans and records 5.4 -> about 0;
- logic 49.3 -> 42-47 because the ME reads far less main RAM (gl23 measured 52.7 with the ME walking against 41.7 with the main CPU alone [C]).

### 6.1 Step 1 in detail

New code (all behind `MGS2_NAT`, off = today's build):

| Where | What |
|---|---|
| runtime, main CPU and x86 (`boot/`, beside `dg_lite.h`) | the 0x1214a0 stub: the builder's choices in C (which groups; nA, nB, nC from the descriptor bytes +0x0E, +0x0F, +0x10; the header words; the VU1 entry index by the rule of `re/mesh_packets.md` 3.1), the record append, the constant block copy, the channel copy at 0x121060's return |
| `ge/` header shared by both CPUs | the snapshot's layout: offsets only, no host pointers |
| `ge/xconv.c` | the lane 1 converter's core takes the five pointers and the header words as arguments; the lane keeps calling it with the packet's |
| `ge/` new file, ME-safe | records -> that converter; range checks; the page and texture look-up from the record |
| `ge/xme.c`, `ge/xme.h` | `XE_SNAP`; the RAM phase and the list phase; `ram_done` in the shared words |
| the walk (`ge/xlme.c`) | the merge: at chain 3's start emit pass 6's records; in chain 6 emit the records whose key is at or below the next tag's address |
| `psp/xl_psp.c` | chunks posted during the call, the write-back, the hold and its timer; per turn: records, snapshot bytes, hold ms, native ms on the ME, dropped records, arena fill; the one-word ME-to-VRAM probe |
| x86 check build | the shadow compare and the order compare |

One difference to keep in mind: the builder skips a group when the packet buffer would overflow [C mesh_packets 3]. A stub never overflows. If the shadow compare ever shows a skipped group on a route, the stub must apply the same rule from the packet pointer it sees.

Gates before delivery:
1. The read-hash proof of the build's stub pattern and turn schedule (3.1).
2. Shadow on routes i and w01a: 0 differing batches, 0 order differences.
3. Emulator: frame digests equal between two runs with opposite turn phases.
4. The four ME gates.
5. Hashes: twin = emulator.

Textures in step 1: unchanged. The record's material pointer gives TEX0 and CLAMP as the packet did; the texture cache works as today.

Decision points:
- **D1 (step 1, arm C against A).** How much the shared walk falls when the mesh chain is gone. If under 3 ms a step: the walk's cost is per transfer, not per kind, so only the end state (no VIF1 transfer at all) pays; then steps 2 and 3 ship as one build.
- **D2 (step 1, arm B against A, main CPU).** If the main CPU gains under 2 ms a step: the remaining pipeline (transforms, culls, sort) is the main cost; pull forward the VFPU box cull and native C for 0x11fa58 / 0x120538 (Phase 3 items) before more kinds.
- **D3 (step 2).** GE time with GE-lit people. Over 36 ms a frame: step 8's first lever before step 3.
- **D4 (step 3).** The hold at the hook. Over 1 ms a step: the weaker deadline for effect sets, and the cache next.
- **D5 (step 4).** Cache memory. Under 512 KB available: VRAM (5650 first), or a smaller cache with eviction.
- **D6 (step 5).** If the ME's picture does not fit one rain step: every step only where it fits (a per-frame choice, the picture side only; the game state does not depend on it).
- **gl29's result** (decal arms on the real GE) fixes section 9's rule before step 1 ships.

---

## 7. Beyond the deck

Generic (the DG library is the same code in every stage):
- ship meshes -> any DG_OBJ model (0x1214a0);
- people -> any multi-texture model (0x12e8b8);
- faces -> any EVM model;
- effects -> any DG_PRIM;
- prebuilt packets, the 2D walk, the texture cache, the snapshot, the hand-off.

Deck-only or stage-bound:
- the sea plug-in (the Tanker's deck and the Plant's outdoor struts);
- each stage's post program (GS packets from the stage image: handled by the walk, no per-stage code);
- decal pairing tables (per model, built at load by the same rule everywhere).

Not on the deck, needed later:
- projected shadows and spotlights (passes 9, 10: render to texture). Whether today's translator draws them is UNKNOWN; they have never run on the PSP. Check: a twin run of an indoor Tanker area with the GS reference tool.
- optical camouflage (object flag 0x4000080), mirrors (channel order swap).

How the design carries without a rewrite:
- **The traversal is the game's.** New stages bring new objects, not new traversal code.
- **Per-object fallback.** At object creation (hook at 0x129128) the model gets a class byte: vertex formats, flags and VU1 entries known to the native code, or not. A stub of an unknown class returns 0 and the original builder runs; the walk draws that object. Coverage grows by adding classes.
- **A census per stage in the container** lists the classes and VU1 entries a route uses before any PSP run of that stage.
- The walk stays in every build as the complete fallback.

---

## 8. Risks and UNKNOWNs

| # | Risk or UNKNOWN | Cheapest way to settle |
|---|---|---|
| 1 | **The main CPU stays the wall.** Phase 2 gives the rain 62-70 ms a step [I]; the logic is 49 [C]. | Step 1's arm C gives the true main-CPU saving of one builder. Phase 3 is the answer, not more renderer. |
| 2 | **Leftover lanes** make each stub set a new game state; a stub could change a value read on purpose. | Section 3.1 per stub set, container only. The mesh and people builders are clean (V1, V3a). The sea builder is not (V3b). 0x12d910 and the EVM builder still need their classification. |
| 3 | **Return values of 0x12d910 and 0x13bf78.** V4 used 0 for the first and the control flow held; the second changes a flag when it just returns. | Read the two builders' exits (an hour each); then the read-hash run of the corrected stubs. |
| 4 | **Real-GE-only faults**: coplanar layers, the 0..4096 window, depth on big triangles. The emulator does not show them. | Identity with today's batches first (same faults as today, no new ones). gl29's arms for decals. Eye shots in turns on the PSP. |
| 5 | **Races between the cores**: stale cache lines, an object freed while the ME reads it. | The hold rule (4.3). x86 check: the list phase asserts it reads no guest memory; a debug mode fills freed model memory with a pattern. |
| 6 | **ME hang on a bad pointer.** | Range checks on every record; the hang test and take-back exist [C]. |
| 7 | **Memory**: 27 KB of heap free; where the cache lives. | Step 1 logs the frame arenas' fill and the outside-heap block. The ME-to-VRAM probe. |
| 8 | **The shared walk's share per kind** (31.45 ms a step). | Step 1, arm C. |
| 9 | **Chain 6 address order** for the merge key. | x86 census, one run. |
| 10 | **How often colour buffers change** (cache layout). | x86 census, one run. |
| 11 | **Which effect uses destination alpha**; what 5650 does to it. | `tools/gsref.c` on the existing capture; then a shot. |
| 12 | **The GE's share of the bus with a picture every step** (4.3: the step may grow 5-9% [I]). | Step 5's turn pair on the PSP. |
| 13 | **Estimates.** All [I] figures can be off by 2x. | Each step's PSP run. |
| 14 | **Shadow passes and other stages' kinds.** | Section 7's census. |

---

## 9. Coplanar layers (the decal lesson, designed in)

What is known:
- The deck's floor shadows are copies of floor meshes lifted 5 units, blended Cd - Cs x As, no Z write, depth test GEQUAL. On the real GE patches of them vanish on large steep triangles; the emulator shows nothing [C gl29_plan 1.1].
- The fitted error: about 1e-3 of the triangle's z span plus 0.3 depth codes [C model / I, gl29_plan].
- A lead that explains it: the GE's depth is one affine plane per triangle, with floored gradients, anchored at one vertex [lead: psp-developer ge-arithmetic, "Depth"; not our measurement]. Two triangles with different vertices get different planes, so a lift smaller than the error loses pixels.

Rules for the native renderer:
1. **A decal that is a copy of a mesh is drawn with its base's triangles.** Same position bytes, same matrix, same strip order; only UVs and colours are the decal's. Its depth plane is then the base's, bit for bit, and GEQUAL passes everywhere. The native path knows which mesh is which, so the pairing is made once per model (each decal vertex = a base vertex + (0, 5, 0)), not by looking back through batches. This is gl29's arm X done at the source.
2. **A layer with its own geometry on a surface** (splash rings, foot splashes, blood) gets a depth bias through the viewport's z offset, sized from the error rule, per effect class. gl29's arms B1-B8 give the size.
3. **No per-frame jumps in the depth map** for layered draws: the bias is set in the PS2's Z units and scaled with the frame's map.
4. Kept as an option: split floor triangles over a size limit when the cache converts them, so no triangle's z span is large.
5. Until gl29's PSP run is read, rule 1 is the plan and rules 2-4 are candidates.

Check: the census counts decal groups and how many found a base (target: all on the deck). On the PSP: the shots gl29 uses.

---

## 10. Files of this study (container only)

All under `/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/native/`.

| File | What |
|---|---|
| `x86e/boot_x86e.c`, `x86e/build.sh`, `x86e/run.sh`, `x86e/cmp.py` | the twin with `MGS2_LITE_ALL`; hash-line runs and their comparison |
| `x86e/boot_x86e4.c`, `x86e/run4.sh`, `xt/rh4.sh` | adds `MGS2_LITE_RET=<fn>:<v0>` (a stub's return value); binaries `x86e/boot4`, `xt/boot4` |
| `x86e/out_base.log`, `out_v1.log` ... `out_v4.log` | hash lines of the runs in 1.2 |
| `xt/mkt.sh` | the read-hash build of the same twin |
| `xt/rh.sh` | one read-hash run (gl28's game settings) |
| `xt/rh_stat.py` | differing vblanks per route window |
| `xt/rl_lanes.py`, `xt/rl_diff.py`, `xt/rl_lanes2.py` | differing reads by reader function and lane (rl_lanes2: two runs of two binaries) |
| `xt/out_*.log` | read-hash and read-log runs (large; game-derived, they stay here) |

To repeat a proof for a new stub set: `xt/rh.sh <tag> 31400 MGS2_LITE_ALL=0 MGS2_LITE_FN=<list>`, then `python3 -I xt/rh_stat.py xt/out_base.log xt/out_<tag>.log`, then a second run with `MGS2_READ_LOG=<vb>:<vb>` and `xt/rl_lanes.py`.

Nothing in `/home/claude/t28`, `/home/claude/t29`, `/home/claude/main` or `/mnt/project-files` was changed.
