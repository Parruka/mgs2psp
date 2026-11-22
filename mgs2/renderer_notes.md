# MGS2 GE renderer: what the draw pipeline does (measured, 2026-10-19)

Goal: replace the PS2 draw pipeline (the call of `DG_ActLast` 0x11da90, about 60 ms of a 133 ms deck step on Ffm's PSP in v6) and the VIF1 / GS walk (11 ms) with a native GE renderer, keeping every word the game itself reads back. Sources: x86 traces of the v7 reduced code (boot.c `MGS2_DG_TRACE`, `tools/dgt_report.py`, `tools/dgt_inputs.py`), the GS capture (`tools/gscap.py`, deck vb 25000-25005), the PSP cost model (PspProf, build v7), and the R5900 disassembly of SLES_503.83. Numbers only: no game data here.

## 1. What reaches the GS per deck vblank (gscap)

- VIF1: about 11.9 k DMA tags, **5.1 MB per vblank**.
- **2,107 VU1 micro calls**. The programs, named by the ELF's `.DVP` overlay, are P04 (11 entry points; +df0 785 calls, +3740 192, +780 102, +2870 96 ...), P06 (+0 147, +358 75), P09 (146) and P01 (3). There are also 415 calls at entry 0x90 of an unnamed program.
- 9,714 UNPACKs per vblank, 1.35 MB (V4-32 595 KB, V4-16 640 KB, V2-16 115 KB). About 4.6 per VU1 call. 54% of the UNPACK data is identical (address and bytes) to the previous vblank.
- **Textures:** 34 IMAGE uploads per vblank through PATH2, sent as CT32 transfers, **3.67 MB, identical every vblank**.
  - They are sampled as T4 / T4HL (4-bit CLUT). Also TEX0 CT32 1024x1024 ×10 and T4 128x1024 ×4.
  - The GE reads CLUT4 textures straight from RAM. Converting them once per stage load removes the whole per-vblank upload.
- GIF: PATH2 only (174 tags), no PATH3.

## 2. Pipeline structure (disassembly)

- **Entry.** `DG_ActLast` 0x11da90 tail-calls 0x11ee98, which makes these calls in order:
  1. 0x12a850(buf)
  2. 0x128d90(0x1df950, buf)
  3. 0x12d560(0x1df950, buf)
  4. 0x12d770(0x1df950, buf)
  5. 0x11e5e0(buf)
  6. Then it jumps to 0x12a8c8.
- **buf** = word [0x18dc30], the frame buffer index. 0x1df950 is the channel ("chanl") structure.
- **0x12a850 / 0x12a8c8.** They walk the [0x18e8f0] objects listed at 0x1e6820 (fields +0x334, +0x340, +0x344), per buffer: setup and reset.
- **Pass tables.** Words at 0x18dbc0 (16 entries) and 0x18dc00 (10 entries) hold the per-channel passes, in the MGS1 `DG_*Chanl` style:
  - 0x18dbc0: 0x139160, 0x11f4d0, 0x139180, 0x121060, 0x130390, 0x138ac8, **0x1226c8**, 0x1303a8, 0x138ae0, 0x133cc0, 0x1324f0, 0x139180, **0x12dc90**, **0x122138**, 0x139180, 0x139180.
  - **0x139180** walks a hook list at [0x1b9b54] (sentinel 0x1b9b70). It calls the entries whose flag matches [0x1b9b90] and the channel's bit (16 << chanl[+1172]). Other systems use these hooks to draw between passes (`sprite_dma_Act` 0x15b288 runs inside the pipeline).
  - 0x139160 sets [0x1b9b90] to 1 and 0x139170 sets it to 0.

## 3. Who builds the VIF1 packets (x86 trace)

Writers inside the pipeline of the words VIF1 reads, with scratchpad copies followed back to the code that wrote the scratchpad:

| Function | Share of VIF1 reads | Path |
|---|---|---|
| P1214a0 | 34.0% | pass 0x1226c8 → P122608 → P1214a0 |
| P120860 | 20.0% | pass 0x121060 → P120860 |
| P121c30 | 15.5% | pass 0x122138 → P121c30 |
| Plain scratchpad copies | 17.0% | |
| P12e8b8 | 8.8% | P12f130 → P12e8b8 |
| P13bf78 | 3.8% | P13d540 → P13bf78, which runs the VU0 micro programs |

All other writers are 0.2% or less each.

- **VU0 / VIF0.** The VIF0 packets come from P136798 (70%), P13bf78 and P121c30.
- **By reference.** VIF1 also reads 5.3 MB of data the pipeline did not write (1.39 M words per vblank, 224 ranges): textures and model data, sent by reference.
  - Most of it was last written by P16cff8 (the 1 MB at 0xdbca00 and 0.3-0.5 MB ranges at 0xb77600, 0xae8e00, 0xd00000, 0xd66a00 ...), probably the stage loader.
  - About 1 MB of it is per-vblank vertex data written outside the pipeline by stage code: the qcopy `S_w00a_P3b7c88` and `S_w00a_P319078` / `P31f188` / `P332180`.

## 4. What the pipeline reads (its input)

311 ranges, 7.6 M reads per 100 vblanks (`dgt_inputs.py`). The largest:

| Range | Size | Read by | Last written by |
|---|---|---|---|
| 0x1396500 | 133 KB | P1214a0 80% | P129128, 96% |
| 0x1b4600-0x1b4900 | — | P120860, P11fa58 | actors (`S_w00a_P219d90`, `P370688`, P165f60) |
| 0x18db00 | — | P120860 | not written by guest code: static |
| 0x15d1a00 | 74 KB | — | — |
| 0x1611d00 | 52 KB | — | — |
| 0x1479e00 | 22 KB | P12e8b8 | — |
| 0x14c5900 | 67 KB | P12dbc0 85% | stage code: `S_w00a_P319078`, `P3b7c88` qcopy |

- P129128 is probably the per-object setup.
- 0x1b4600-0x1b4900 is the actors' requests.
- P12dbc0 is the costliest pipeline function (8.1 ms per step on the PSP model). It is not a packet builder itself; it reads the vertex data the stage copies in each step.

## 5. What the game reads back (must stay exact)

About 150 words. They come from 12 pipeline functions, at most 10.6 ms of self cost per step: see speed_notes.md §12.

- The scratchpad words 0x3e70-0x3fdc / 0xcc0-0xcdc, read 221 k times in 300 vblanks by about 10 stage functions, come from P121c30 (probably the view matrices).
- 0x1b4700-0x1b47e0 come from P120860, P122138 and P135338.

## 6. Cost of the pieces on the PSP (v7 model, self ms per full deck step)

**Pipeline EE code:**

| Function | ms |
|---|---|
| P12dbc0 | 8.14 |
| P11fa58 | 4.84 |
| P1214a0 | 4.62 |
| P120538 | 2.58 |
| P120860 | 2.11 |
| P12dc90 | 1.84 |
| P13bf78 | 1.61 |
| P121c30 | 1.48 |
| P12e8b8 | 1.07 |
| P12d910 | 0.92 |

**Compiled VU0 micro programs:**

| Program | ms |
|---|---|
| vup_8baafbc3 | 7.1 + 0.9 + 0.4 |
| vup_33268338 | 1.2 |
| vup_43da1f40 | 1.0 + 0.3 |

**DMA / VIF1 walk:** io_flush 12.1, vif1_word 4.2, dma_feed 3.7, dma_vif1_block 2.6, dma_run 1.8.

## 7. Detailed layouts (2026-10-19 studies, `re/`)

- `re/dg_objects.md`: the object struct DG_OBJS (0xC0 header + 0x130 per mesh), the model definition, materials, lights, texture pages (0x1e6820), the channels (5 × 0x4A0 at 0x1df950, object queues at chanl+0x28C) and the pass tables with roles.
  - Table 0x18dbc0, 18 entries: screen 0x11f4d0, transform+cull+shade 0x121060, opaque meshes by page 0x1226c8, prims 0x12dc90, depth-sorted meshes 0x122138, hooks 0x139180 in phases 1/2/4/8.
  - Table 0x18dc08: channel 4, hooks only.
- `re/mesh_packets.md`: every builder decoded qword by qword, the VU1 memory map, vertex formats (V4-16 position, V4-16 second stream, V2-16 UV), the material block (TEX0 T4/T8, CLAMP, ALPHA, TEX2) and the VU1 entry chosen per mesh.
- `re/vu1_programs.md`: all 14 VU1 entries, with the math, clipping, lighting (3 directional + ambient), fog and the GE mapping.
  - Its §1.1 "matrix defect" was a fault in the study's own replay: the capture holds complete matrices.
- `re/pipeline_hot.md`: what the costly pieces are.
  - The sort pass: 491 effect meshes, 15.4 k vertices per step, built by effect actors that must keep running.
  - Rigid and skinned objects; the VU0 box cull: about 1,270 calls and most of the 8.4 ms of compiled VU0.
  - The sea: 2 × 225 Hermite patches, which map to GE Bézier patches.

## 8. GE renderer design (first version; inferred from §1-7, each point to be measured)

**Where it cuts.** The pass functions that compute game-visible state keep running:
- 0x120538: matrices, cull status, and the part and bone matrices the game reads back.
- The sort-key and radix-sort logic.
- The hook bookkeeping.
- The light requests (0x128d90 / 0x12d560).

The functions that only build VIF / GS packets are replaced by GE emitters: 0x120860, 0x1214a0, 0x12d910, 0x12e8b8, 0x13bf78, the page uploads (0x124d48) and the pass headers (0x1244a8 / 0x124630). So are the VIF1 DMA, the GS walk and the VU1 work. The VU0 box-cull calls get a native box test with the same result.

| Draw kind (VU1 entry) | Per vblank | GE path |
|---|---|---|
| Pre-lit meshes (P04+df0, +1088, +2870, +25f0) | ~950 calls | Hardware transform with the mesh's local-to-clip matrix (mesh+0x40), vertex colour, GE fog. Vertex streams converted once per model to a GE format (s16 position, colour, s16 UV) and cached. Strips split at the restart flag. |
| Lit meshes (P04+780, P06+0) | ~250 | GE lighting, 3 directional lights + ambient, from the mesh's light block. P06's 2nd and 3rd texture passes are extra draws of the same vertices. |
| Skinned (P04+a70, P06+358) | ~90 | GE 2-matrix skinning. |
| Effect meshes in the sort pass (prims types 1-5) | 491 meshes, 15.4 k vertices | Converted on the CPU each step from the actors' current buffers, drawn in the game's sorted order. |
| Sprites, rotated quads, AA lines (P04+3740, +3a38, +2b60) | ~260 | CPU-built quads and line strips (lines without AA). |
| Sea (P09) | 146 | `sceGuDrawBezier` from the game's Hermite corners and tangents. The game's cull and level of detail stay; the crack mask needs handling. |
| Instanced mesh (P01) | 3 | One cached vertex buffer, a matrix per instance. |
| Stage program 2D lines (0x201f78+0x90) | 415 | Through-mode lines (radar). |

**Textures.** The 32 pages at 0x1e6820 are uploaded every frame through PATH2 as CT32 into the same GS area and sampled as T4 / T8 with a CLUT. Each page is converted once into a GE CLUT4 / CLUT8 texture in RAM or VRAM, cached by page pointer. The cache is dropped when the stage loader rewrites the page.

**Exactness.** The renderer changes which RAM words are written: packet buffers, the per-mesh VU1 blocks and the scratchpad packet area. The full-RAM state hash will therefore differ. The check becomes a game-state hash that leaves out renderer-owned memory. The DG traces must show none of that memory is read by game code. That list is the ~150 words of §5, plus anything a new trace finds.

**Cost (inferred, to measure).** About 1,800 GE draw calls per vblank, from direct display-list writes (TT lesson: vertices in VRAM). About 15 k effect vertices converted per step. Kept CPU passes with a native cull. Target: the 51 ms draw pipeline and the 11 ms GS walk become about 10 ms in total.

## 9. Reference renderer and the deck's GS census (`tools/gsref.c`, deck vb 24050-24057)

**Tool.** `tools/gsref.c` replays a GS capture through VIF1, a VU1 interpreter (the float, flag and pipeline model of `runtime/vu0.h` with VU1 sizes, XGKICK, XTOP/XITOP and the EFU) and a software GS. It writes every frame buffer and the display circuit as images and prints per-vblank counts. 8 vblanks take 9.5 s. `tools/gsref_entries.py` names the VU1 entry points (P04+df0 ...). Saved numbers: `prof/gsref_deck_vb24056.txt`, `prof/gsref_entries_vb24056.txt`. The images are game data and stay in the container.
- The capture (`runtime/ee_io.h`, dev builds only) now also records the GS display registers (PMODE, SMODE2, DISPFB1/2, DISPLAY1/2) after each vblank record. PSP builds are unchanged.
- Check: the rendered deck frame shows the expected scene (Snake, deck machinery, rain, the radar, the codec subtitle and the weapon box). Not modelled: dithering, AA1 coverage, the GS's exact interpolation rounding (edge pixels can differ), interlace offsets.
- Debug switches: `GSREF_VIFLOG=1|2` (VIF commands, 2 adds the data and its guest address), `GSREF_TRACE=<call>` (VU1 trace of one call), `GSREF_VDUMP=<n>` (vertices as the GS gets them), `GSREF_SPRITES=1`.

**Frame.**
- The game draws a full frame every vblank (50 per second). The buffers are CT32 512×512 at page 0 and page 0x80 alternately, with Z24 at page 0x180. The display shows the buffer drawn in the previous vblank (DISPLAY 512×512, MAGH 5).
- Per vblank (vb 24056):
  - 2,867 VU1 calls and 7,353 XGKICKs;
  - 32,526 primitives: 27,427 strip triangles, 1,205 fan triangles, 2,308 sprites and 1,586 lines, plus 2,599 zero-area strip restarts;
  - 5.6 M pixels written at 512×512 (3.9 M at vb 24051; it depends on the rain).

**Where the pixels go (vb 24056).**

| Entry | Share of pixels | What it draws |
|---|---|---|
| P04+25f0 | 40% (2.2 M) | Effect fans: additive or alpha blend, no Z write |
| P04+3740 | 31% | Sprites (rain) |
| P04+df0 | 13% | Pre-lit meshes |
| Stage image's post program (0x202390) | 13% | Post passes |
| Every other entry | under 2% each | |

Four entries write 0 pixels in this view:
- P09 (the sea): all of its pixels fail the depth test from this camera.
- P04+2870: its position buffers are all zero, so every triangle is culled as degenerate.
- P01 and P04+1088: their pixels fail the depth test.

**State census (all draws use context 1).**
- **Depth:** Z24 with test GEQUAL (larger is nearer), except UI and post (ALWAYS).
- **Blend.** GE mappings are inferred, to verify in PPSSPP and on the PSP. PS2 alpha 0x80 means 1.0, so the GE path doubles alpha (vertex alpha ×2 or the DOUBLE_*_ALPHA factors).

| PS2 equation | Used for | GE mapping |
|---|---|---|
| (Cs−Cd)·As+Cd | Normal blend | SRC_ALPHA / ONE_MINUS_SRC_ALPHA |
| (Cs−0)·As+Cd | Additive | SRC_ALPHA, FIX white |
| (0−Cs)·As+Cd | Subtractive | GU_REVERSE_SUBTRACT |
| (Cs−0)·Ad+Cd | Additive scaled by destination alpha; about 600 fan triangles | DST_ALPHA. The GE keeps destination alpha in the stencil byte of an 8888 buffer, so it must be written as the PS2 writes it. |
| (Cs−Cd)·FIX+Cd, FIX 0x40 | One full-screen post sprite | GU_FIX |

- **Alpha test.** GEQUAL 0x40 with AFAIL = FB_ONLY on nearly every 3D draw.
  - It only matters where Z is written. About 2,300 Z-writing primitives per vblank have failing pixels: colour is written, Z is not.
  - The GE has no AFAIL modes, so those draws need two passes: alpha ≥ 0x40 with Z write, then alpha < 0x40 without Z write.
  - A draw needs only one pass if its texture has no texel below 0x40. The converter knows this per texture and CLUT.
- **Fog** on all 3D draws. The per-vertex F is linear in clip w: F ≈ 241 − w/100 on pre-lit meshes, from the VU constants abs2.xy.
  - It maps to the GE's linear hardware fog, with start and end taken from the same two constants.
  - A smaller set of vertices uses another line (F down to 14).
- **Textures.**
  - Formats: T8 and T4 with a CT32 CLUT (CSM1); T4HL / T4HH for the font; the frame buffer read as CT32 for post.
  - Filtering: all bilinear (MMAG = MMIN = linear), no mipmaps, LCM = 1.
  - 1,780 CLUT loads per vblank. They use CLD 4, so a load happens only when the CLUT address changes.
- **Wrap.**
  - REGION_CLAMP on almost every draw: sub-rectangles of the 1024-wide atlas pages. Most sizes are not a power of two, and origins are aligned to anything from 4 to 1024 texels.
  - REGION_REPEAT on about 2,000 primitives: 128×128 at 1024-aligned offsets.
  - The GE has neither. The converter cuts each region into its own texture, padded to a power of two with its edge texels repeated, which reproduces the clamp exactly, bilinear included. REGION_REPEAT becomes a 128×128 repeat texture.

**Post chain.** The stage image's program at 0x202390 runs once per vblank:
1. **Depth blur.** 32 strips, each 32 px wide, copy the frame onto itself twice: once at u = x (bilinear, so the average of 2 texels) and once shifted by 1 pixel. Both copies use Z GEQUAL against the sprites' Z and write no Z, so only distant pixels are blurred horizontally (depth of field).
2. **Motion trail.** A full-screen sprite samples the other buffer (the previous frame) and blends it in at 50% with FIX 0x40. Ffm allowed removing motion blur if it costs too much.
3. **Radar.** The radar's background box.

On the GE (inferred):
- Step 1 draws the current frame as a texture in strips through the depth test. It needs a copy first, because the GE must never sample the buffer it is drawing to.
- Step 2 is one blended full-screen sprite, since the previous frame is the other display buffer.
- **The trail at 25 fps (2026-10-21, speed_notes §34):** the PSP draws and shows every other frame, so the trail can only sample the previous *shown* frame. On the PS2 the blend is (Cs - Cd) * 0x40 / 128 + Cd with Cs the other buffer: out(n) = 0.5 s(n) + 0.5 out(n-1) = 0.5 s(n) + 0.25 s(n-1) + 0.25 out(n-2), where s is the frame before the trail. The previous shown frame blended at 25% (FIX 0x20) gives 0.75 s(n) + 0.25 out(n-2): the same weights, with the skipped frame's quarter taken from s(n). At 50% as drawn it gives 0.5 s(n) + 0.5 out(n-2): a trail twice as long in time. Recommended: 25%, compared on pictures against the PS2's every other frame when the renderer is built; a visual choice, so Ffm decides.

**Fill (to measure).** TT's GE was not fill-bound. The deck draws 5.6 M pixels per vblank at 512×512, mostly blended, which is about 2.8 M at 480×272. A blended full-screen fill test in the harness will give the real cost.

**Textures in detail (texture converter, `ge/texconv.c`).**
- **Uploads.** Each vblank uploads 35 blocks, 3.76 MB in all, from 29 distinct EE ranges (3.08 MB). The data and the GS addresses are the same in all 8 vblanks.
  - They are the texture pages of `re/dg_objects.md` §4. Each page uploads its texels, then its CLUTs, to the same GS area (DBP 0x2000 on), as CT32 rows of 64 pixels.
  - Page fields, corrected in `re/dg_objects.md`: +0x33C is the texel data, +0x340 the CLUT data, +0x344 an alternate CLUT for one frame (not used in this window).
  - On the PSP none of this transfer happens. A texture is identified by its page and material, not by GS address.
- **T4 palettes.** Every T4 draw (20,373 per vblank) reads entries CSA×16 to CSA×16+15 of a 256-entry palette stored at CBP in CSM1 order: the material's TEX2 loads 256 entries and CSA selects the row (checked against the live CLUT state on every draw; gsref "clut check").
  - On the GE: load the 256-entry palette once, and select the row with the CLUT offset of sceGuClutMode (I).
- **Converter.** `ge/texconv.c` is plain C, shared by the PSP renderer and the container tools.
  - Input: the page's own upload packet (`tc_page`) and the material's TEX0 / CLAMP.
  - Output: a swizzled CLUT8 / CLUT4 texture. The GE size is a power of two; only the region plus one repeated edge column and row is stored. REGION_REPEAT with a power-of-two mask becomes a GE repeat texture.
  - Palettes become 8888 with PS2 alpha 0x80 mapped to 0xff (`tc_palette`).
  - Why one edge texel is enough: on every deck draw, bilinear taps stay within one texel of the region (gsref "texuse" extents). A new scene should be checked the same way.
- **Check.** `GSREF_RAM=<EE RAM dump> gsref <capture> <dir>` converts each texture of the capture from the dump's page records and compares it with what the GS samples, including one texel beyond each edge (or a period, for repeat).
  - Deck vb 24050-24057: all 318 page textures per vblank match, **0 of 16.3 M texels differ**.
  - Not page textures (4 per vblank, still to do): the font (T4HL / T4HH in the top byte of the Z buffer at block 0x3000, its CLUT uploaded on its own) and the two frame-buffer textures of the post chain.
- **Whole stage at load** (`tools/texconv_pages.c`, every material of the 32 deck pages):
  - 836 textures (222 T8, 614 T4), 4.34 M texels, **3.53 MB** of GE textures. Padding to powers of two would need 4.49 MB.
  - The deck view itself uses 320 textures.
  - 88 ms on this PC with the simple per-texel code. Converting per GS block (one upload lookup per 256 bytes) should cut the PSP time several-fold (I).
  - 329 materials have a used texel below alpha 0x40, so their Z-writing draws need the second alpha-test pass; 17 have every texel below.
- **Memory is the open question.**
  - v2 measured the EBOOT at about 48 MB of the 56 MB large-memory partition, with IOP RAM (2 MB) and the host stacks (1.2 MB) coming out of the rest.
  - The 4 MB volatile block holds the 2 MB SPU RAM, so 2 MB of it is free.
  - Options: textures in the free volatile 2 MB plus the heap; or a converted-texture cache with a budget (the deck view needs about 1.5 MB with edges).
  - Before choosing, measure the real heap headroom on the PSP. The v7 log printed 65,520 KB, which is the search limit, not a real value.

**VU0 side note.** `runtime/vu0.h` ISW / ISWR store only the first selected field; the VU stores all selected fields. No MGS2 VU0 program seen so far (20 images) and no VISWR / VILWR in the ELF uses a multi-field form, so nothing changes today. Fix it if a stage's program ever uses one.

## 9b. GE replay of one deck frame (`psp/ge_replay/`, MGS2GE; 2026-10-19)

**What it is.** `tools/gsref.c` with `GSREF_GEOUT=<file>:<vb>` writes every GS primitive of one vblank as GE batches (format `ge/replay_fmt.h`: screen-space vertices, through mode, textures converted by `ge/texconv.c`). The EBOOT draws that frame with the GE, per mode (one change each): full, no textures, no blending, alpha test in one pass, nearest filtering, without each of the 4 entries that write the most pixels, full with the CPU copying RAM meanwhile (bus sharing, both directions), and merged draws. It reports GE time per frame (list run alone) and the CPU time to build the list.

**Deck vb 24056 in numbers (measured in the file).** 12,250 batches, 59,312 vertices, 322 textures, 1.68 MB of texture data, 331 KB GE list.
- Entry 0x1991d8 (textured, fogged; 226 textures, so the scene meshes) is 7,269 of the 12,250 batches: mostly 4-vertex strips, many of them sub-pixel streaks with one texel (u, v 0.5) that write 0 PS2 pixels (inferred: the rain). 6,063 of its neighbouring batches have the same state as the one before.
- Merging neighbouring same-state batches (strips and fans as triangle lists, line strips as lines, order kept) gives 1,850 draws and 93,684 vertices; the list falls to 128 KB. PPSSPP's software GE draws the merged frame identical to the unmerged one except 22 pixels off by 1 to 3.
- PPSSPP's CPU timing for building the list (indicative only): 10.4 ms unmerged, 2.3 ms merged; without entry 0x1991d8, 5.0 ms. The GE times from PPSSPP mean nothing (0.02 ms); the hardware run gives them.

**Picture check (PPSSPP headless against gsref).** Same scene, subtitle and radar. The one visible difference is fog: through mode has no fog, so distant geometry keeps its contrast. Rendering gsref with `GSREF_NOFOG=1` confirmed it: the "lighter radar background" seen first is the unfogged scene behind a correctly blended radar (sprite, (Cs-Cd)As+Cd, PS2 alpha 0x60 -> GE 0xC0, both 0.75). The real renderer uses hardware transform, which has fog.

**pspgu quirks met (this pspsdk build).**
- `sceGuScissor(x, y, w, h)` takes sizes, and also sets the drawing region (REGION1/2). Without a call, the region is one pixel and nothing appears.
- `sceGuClear` takes its size from the current context's draw buffer. In a `GU_CALL` list that is empty, so the clear goes in the direct list.
- `sceGuGetMemory` after `sceGuFinish` corrupts the stall address; `sceGuCheckList` gives the list size.
- `sceGeDrawSync(1)` (peek) returns 0 once the list is done (PPSSPP `GPUCommon::DrawSync`).

**PPSSPP headless.** `sceIoDevctl("emulator:", 3, ...)` answers "is emulator"; devctl 0x20 writes a screenshot. `--screenshot=<bmp>` needs a 32-bit BMP of 512x272; the frame lands in `__testfailure.bmp`.

**Heap probe fix.** psp-gcc removed the `malloc`/`free` pair in `heap_headroom()` (`psp/main_psp.c`), so the 65,520 KB "headroom" in v5-v7 was not real. The probe now goes through a `volatile` pointer; the next harness build reports the real figure.

**Hardware run (PSP-3000, 333/166 MHz, MGS2GE v1, 2026-10-19; Ffm's log).** GE time per frame, mean of 39 frames x 3 rounds (min within 0.15 ms):

| Mode | GE ms | CPU build ms |
|---|---|---|
| full | 75.11 | 14.84 |
| no textures | 27.10 | 12.54 |
| no blending | 75.06 | 13.92 |
| alpha test in one pass | 73.57 | 13.29 |
| nearest filtering | 66.19 | 14.84 |
| without entry 0x19a9f0 (P04+25f0, effect fans) | 67.35 | 14.41 |
| without entry 0x19bb50 (P04+3740 and what follows it) | 36.21 | 14.71 |
| without entry 0x1991d8 (P04+df0, scene meshes) | 59.54 | 7.47 |
| without entry 0x202390 (clears, HUD) | 72.55 | 14.82 |
| full + CPU copying | 95.98 | 14.84 |
| merged draws | 76.62 | 3.32 |

CPU copy rate 54.1 MB/s while the GE draws, 69.8 MB/s with it idle. Largest list 331 KB. Every blend mapped.

What it says:
- The GE is the wall for drawing: 75 ms against a 40 ms budget (one drawn frame per 2 logic steps at 25 fps), and that is with the CPU idle.
- Textures cost 48 ms; blending nothing; the second alpha-test pass 1.5 ms; bilinear 9 ms over nearest.
- Merging same-state draws adds 1.51 ms (2.0%) of GE time and cuts the CPU build from 14.8 to 3.3 ms: the renderer merges. (Wording corrected 2026-10-23, audit L9: it said "leaves GE time unchanged".)
- Bus sharing both ways: +21 ms GE, -22% CPU copy rate. Vertices and hot textures belong in VRAM (TT lesson).

**Per entry, from the file (`tools/ger_stats.py <file>`, `tools/ger_entry.py <file> <entry hex>`; GE area = rasterised pixels before depth and alpha rejection).** The entry is the last VU1 call's entry, so direct GIF packets (PATH2/3, such as the post passes) inherit the VU1 entry that ran before them.

| Entry | GE area (screens) | Cost | ns per GE pixel | Contents |
|---|---|---|---|---|
| 0x19a9f0 | 2.07 M (15.9) | 7.8 ms | 3.8 | Effect fans, T4 64x64 magnified (0.4 texel/px), 20 texture changes |
| 0x1991d8 | 1.79 M (13.7) | 15.6 ms | 8.7 | Scene meshes, 226 textures, 1,204 changes (170 KB of CLUT loads), minified: T4 3.3, T8 8.1 texels/px |
| 0x19bb50 | 1.45 M (11.1) | 38.9 ms | 26.9 | See below |
| 0x202390 | 0.40 M (3.1) | 2.6 ms | 6.4 | Two full-screen clears, subtitle text, radar |

Entry 0x19bb50 holds four different things:
- Mist: one batch of 15 sprites averaging 326x193 px, T4 32x32 magnified 100x, additive, alpha test, Z test, no Z write: 0.93 M px.
- Depth blur: 32 sprites of 30x272 sampling the frame buffer being drawn (8888, linear), Z GEQUAL, no Z write: 0.26 M px, of which the PS2 writes only 684 (the rest fail Z).
- Motion trail: one full-screen sprite sampling the other frame buffer, FIX 0x40: 0.13 M px.
- About 1,100 small particle sprites (T8 32x32, minified 5-250x): about 0.12 M px; and the radar box.
At the rate of the effect fans these would cost about 5 ms; they cost 39. One part has a slow path (not yet known which; the frame-buffer reads are the first suspect). MGS2GE v2 times each part.

**v1 bug, fixed in v2: the alpha test of AFAIL FB_ONLY draws without Z write.** On the PS2 a pixel that fails such a test still writes its colour, so the test changes nothing. v1 ran it on the GE anyway (GEQUAL 0x80 after doubling), which dropped the mist (vertex alpha 0x04-0x10) and most particles: 1,415 batches, 3.8 M PS2 pixels. v1's GE times therefore lack those blends. Rule for the renderer: alpha test only where Z is written (one pass if no texel or vertex alpha is below AREF, else two). Against gsref's no-fog frame (scaled to 480x272) the PPSSPP picture's mean absolute difference fell from 13.8 to 8.6. The 3D area is still about 5-15 levels darker than gsref; suspect: PS2 alpha above 0x80 (factor above 1.0), which the converter clamps to 0xff (to check).

**MGS2GE v2 modes** (one change each against "full", which now has the fix): no textures; without the mist; without the depth blur; without the motion trail; without the small sprites; depth blur from a copy of the frame (sceGuCopyImage to VRAM 0x154000, then sample the copy); motion trail in 64-pixel columns; textures in VRAM (175 textures, 688 KB, 99% of the palette-textured GE pixels, chosen by GE pixels per byte); 16-bit 5650 frame buffers (measured only). PPSSPP pictures per mode: the "without" modes change exactly their part; blur copy differs from full in 19 pixels (the PS2's second blur pass reads the first one's pixels; one copy per frame does not); slices and VRAM textures are identical to full.

## 9c. MGS2GE v2b on the PSP and v3 (2026-10-19, `prof/hw_ge_v2b/log.txt`)

v2b (3 rounds x 39 frames, GE ms per frame, spread under 1%): full 74.35; no textures 26.84; without the mist 71.11 (mist 3.2); **without the depth blur 47.42 (blur 26.9)**; without the motion trail 66.96 (trail 7.4); without small sprites 73.44 (0.9); depth blur from a copy 75.78 (copy +1.4, sampling the copy costs the same as sampling the live buffer); **motion trail in 64-pixel columns 68.43 (trail 1.5: -5.9)**; **textures in VRAM 68.73 (-5.6)**; 16-bit frame buffers 57.34 (-17.0, a visual change: measured only). CPU build 16.3 ms (the replay's builder, not the renderer's).

Rates: trail full-width 57 ns per GE pixel, in 64-pixel columns 11 (texture-cache thrash across a 1,920-byte row of a linear 8888 texture). Depth blur: 103 ns per pixel of its 0.26 M (143 per pixel inside its scissor, rows 18-214), although its strips are already 30 pixels wide, so the column fix alone does not explain it. Batch dump: 32 sprites, each **bottom-right corner first** (480.94,272.53 -> 450.94,0.53), u = x + 0.94, v = y + 0.53, Z GEQUAL at z 46 (only 684 PS2 pixels pass), no blend, no alpha test on the GE, scissor y 18-214, sampling the 8888 buffer being drawn, bilinear. Suspects: the reversed corner order, the texture fetch of pixels that then fail Z, bilinear on a linear 8888 texture.

Sums (estimates from v2b's one-change modes, not measured together): trail columns + VRAM textures = about 63 ms; if the blur fell to the trail's 11 ns per pixel (about 3 ms), about 39 ms.

**MGS2GE v3** (`psp/ge_replay/main.c`, delivered as MGS2GE_v3): part 1 times each costly class alone (the frame drawn up to the blur untimed, then the class 10 times, 3 rounds): blur as drawn, top-left corner first, 15-pixel columns, 16-row tiles, top-left + 16-row tiles, from a copy, and (picture-changing, measured only) without texture, nearest filter, no depth test, 16-bit buffer; mist and small sprites with and without VRAM textures; trail as drawn, in 64-, 32-, 16-pixel columns, 16-bit. Part 2: the full frame as v2b, "best fixes together" (VRAM textures + the fastest blur cut + the fastest trail cut among as drawn / 32 / 16, chosen from part 1 on the PSP), and best without the trail, without the blur, with 16-bit buffers, without textures. PPSSPP pictures (`/home/claude/ge_v3m`): trail in 32 or 16 columns = full exactly; in 64 columns 11 pixels differ by 1 (timed, not chosen). Every blur cut changes 205-251 of the blur's pixels (x 429-479, y 201-218, up to 86 levels), the same whether it reads the live buffer or a copy: the GE places texels a texel apart in places for a reversed or cut sprite. The renderer sets its own texel mapping for the scaled frame, so this is a mapping detail, not a reason to keep the slow form.

**MGS2GE v3 on the PSP** (2026-10-19 22:21, `prof/hw_ge_v3/log.txt`; spread under 1%). Parts alone (ms per draw over the frame drawn up to the blur): blur as drawn 27.20; **top-left corner first 2.65**; 15-pixel columns 25.94; **16-row tiles 2.66**; top-left + 16-row tiles 2.65; from a copy 28.64 (copy included); no texture 1.28; nearest 19.84; no depth test 26.88; 16-bit buffer 22.48. Mist 3.28 (VRAM textures 3.28); small sprites 0.83 (VRAM 0.72). Trail as drawn 7.48; 64-pixel columns 1.49; 32: 1.53; **16: 1.36**; 16-bit 3.66.
- **Cause of the blur's cost: its sprites are given bottom-right corner first, and are 272 rows tall.** Either putting the top-left corner first or cutting each strip into 16-row pieces (still bottom-right first) brings it from 27.2 to 2.65 ms, and narrower columns do not. Inference, not measured: the GE walks a sprite given that way in an order whose texture reads (linear 8888) do not fit its 8 KB texture cache when the sprite is tall, e.g. column by column. Rule for the renderer: emit every sprite top-left corner first; cut full-screen texture reads into columns of 16-64 pixels.
- Full frame: as v2b 74.34; **best fixes together (VRAM textures, blur top-left + 16-row tiles, trail 16-pixel columns) 38.32**; best without the trail 36.98 (trail 1.34); best without the blur 35.72 (blur 2.6); best + 16-bit buffers 26.90; best without textures 26.35. **The deck frame fits the 40 ms GE budget for 25 fps without any visual change; the motion trail stays.** The replay's own list build was 18.2 ms of CPU (not the renderer's; it does no transform or fog).

## 10. Next steps

1. ~~A GS reference image~~: done (§9).
2. ~~Texture converter~~: done for page materials (§9, Textures in detail). Still to do: the font, and a fast per-block version for the PSP.
3. **First GE path.** Planned in §11 (2026-10-21): the P04+df0 converter in gsref, MGS2GE v4 with hardware transform, then Stage A live in the harness.
4. ~~Fill test on the PSP~~: done as the GE replay (§9b, §9c).
5. **Game-state hash.** Stage A keeps every existing hash (the renderer only reads). Stage B is checked by GE-list equality with Stage A and the from-boot read check (§11.3).
6. **Vertices in RAM or VRAM (Ffm, 2026-10-21).** TT measured VRAM vertices faster (no GE / CPU bus sharing); Peace Walker keeps them in RAM. Re-measure for MGS2 as a GE replay mode when the VRAM budget is laid out (2 MB: frame and depth buffers, the 688 KB of hot textures, the blur copy), since it may not all fit.

## 11. The GE renderer plan (2026-10-21)

Two stages. **Stage A** translates the game's own packets into GE draws: the PS2 pipeline keeps running unchanged and the renderer only reads, so the game state stays exact by construction and every draw kind is covered from the first build. **Stage B** moves the costly draw kinds upstream: a native emitter builds the same GE draws from the game's records, so their packet building, VU1 work and walk disappear; each emitter is checked word for word against Stage A's GE list. After that, the steps that are never shown drop all render-only work. Labels as in `re/`: [C] measured or read in the code, [I] inferred.

### 11.1 What the draw pipeline and the walk cost now

Model, v23 code (build g2, v22's code order), deck vb 24001-24100, mean of full and reduced steps: everything 76.66 ms per step, logic only (`prof` without `all`) 37.13, so **the draw pipeline and the walk are 39.53 ms per step** [C]. Hardware v22 for comparison: draw 31.56 + GS walk 8.60 = 40.16.

| Part | ms per step | Largest | Fate in the plan |
|---|---|---|---|
| Walk of the VIF1 chain and scratchpad DMA (io_flush, gif_qw, interrupts) | 9.60 | io_flush 8.92 | Stage A walks again on shown calls (read only); Stage B leaves only the packets nobody replaced |
| Packet builders | 8.41 | P1214a0 2.42, P120860 1.63, P13bf78 + P13bd00 1.67 (sea), P12e8b8 1.12 | Stage B emitters |
| Sort pass | 6.77 | nat_dbc0 1.94, P121c30 1.38, P12dc90 1.25, P12d910 1.08 | Kept on shown steps (draw order); on the others only what the game reads (P121c30's view matrices); keys and links have no game reader (`re/pipeline_hot.md` §1.6) |
| Compiled VU0 (box cull, bone steps) | 5.32 | vupv_660a267e_0 4.06 (box cull) | Kept (the game reads the results); a native box test with the VU's operation order, as nat_dbc0 |
| Runtime helpers | 5.12 | rt_qcopy_nov 1.74, rt_store 0.82, GSDU 0.65 | Shrink with what they serve |
| Object and part transforms and culls | 3.72 | P11fa58 1.66, P120538 0.74 | Kept (part+0 words, cull status and depth are read by game code, §2.6 there) |
| Other pipeline code | 0.58 | | |

### 11.2 Stage A: the packet translator

**Where it runs.** Portable C in `ge/` (translator, converters, a GE command writer), so the same code runs in x86 check builds; GE setup, VRAM, list kicks and pacing in `psp/`. On a shown call only. The walk at vblank v walks the packets of call v-1 (`tools/dev/steps_percall.py`), so when the walked chain comes from a shown call, a second walk over the same chain feeds the translator. It reads guest RAM and the scratchpad and writes only its own buffers; the exact walk (`vif1_chain_fast`) is not touched, so the interrupts and every hash stay as they are.

**What it models.**
- VIF1 (and the rare GIF-channel transfers): every command, with UNPACK into a 16 KB VU1 data memory model (STCYCL, STMASK, STROW, STMOD, OFFSET / BASE, the double buffer). This is gsref's code (`tools/gsref.c`), whose replay of the deck capture draws the expected frame (§9).
- At each MSCAL / MSCNT, the entry selects a **converter**. A converter reads the VU1 memory as the micro program would (header, matrix, light block, fog constants, GIF templates, the A+D material, the vertex streams) and emits GE commands. The GE does what it can in hardware: transform, fog, lights, two-matrix skinning. The CPU does the rest: strip restarts (ADC) split into strips, winding cull bits, sprites built from points, Hermite patches turned into Bézier control points.
- No VU1 micro code runs on the PSP. gsref's VU1 interpreter stays the reference on x86.
- A GS register model for A+D writes (from XGKICK packets and DIRECT): TEX0 / TEX1 / TEX2, CLAMP, ALPHA, TEST, FBA, ZBUF, FRAME, SCISSOR, FOGCOL, TEXA, PRMODE. It turns into GE state, written only when it changes. Neighbouring draws with the same state merge (the replay went from 12,250 batches to 1,850 draws, §9b).
- DIRECT primitives (clears, the post chain, HUD, font, radar box) are already in screen space: GE 2D draws, scaled to the GE frame.
- Textures: the walk sees each IMAGE upload with its EE source. The converted texture (`ge/texconv.c`) is cached under that source range plus a hash of 64 words spread over it. Hashing 3.7 MB of uploads per frame in full is not affordable on the PSP [I]. The x86 check build hashes every upload in full and reports any content change under an unchanged key (research_review M6).
- An entry with no converter is counted and logged and its primitives are skipped: never a crash, never silent. An x86 census of VU1 entries over the routes (as the VU0 census, speed_notes §37) finds them before a stage is played.

**Converters needed for the deck** (census §9, entries in `re/vu1_programs.md` §3): P04+df0 (785 calls), stage program +0x90 2D lines (415), +3740 sprites (192), P06+0 lit (147), P09 sea (146), +780 lit (102), +2870 / +25f0 effect fans (96 + 64), P06+358 and +a70 skinned (75 + 18), +3a38 quads (40), +2b60 lines (32), +1088 (4), P01 instanced (3), the stage post entry and DIRECT packets. The title, menus, codec, crew deck and cutscenes on the routes come next, from the census.

**Checks.**
1. Game state: every hash equal with the translator on and off, on x86, the emulator and the PSP. In the x86 check build the translator gets only const pointers, and the RAM hash before and after each translated walk must be equal.
2. Geometry, per converter (x86): beside gsref's VU1 interpreter, the converter's vertices are put through a model of the GE's transform and compared on every call of that entry over many deck vblanks: screen x / y, depth, fog, colour, UV, strip splits. Differences are expected only from float24 matrices (research_review M4) and are reported as a maximum in pixels.
3. Pictures: the PSP build in headless PPSSPP writes screenshots at chosen vblanks. gsref renders the same vblanks from an x86 GS capture; the game state is the same, so the scene is the same. Compare at the GE size, per entry (masks on both sides) and whole, by mean difference and the share of pixels more than 16 levels off. PPSSPP master's bit-exact software GE (M2) replaces 1.20.4 here once we have it. **Done 2026-10-30:** master 83e616ea with `tools/ppsspp_master_vfpu_rn.diff` and `tools/ppsspp_master_fpu_fs.diff` (`tools/dev/ppsspp_master_run.sh`) gives the PSP's own shots byte for byte (gl4c 6 / 6 codec shots, gl4b 6 / 6 indoor 3D shots; 1.20.4 differs from them in 0.51-0.57% of pixels) and every checked log line (research2_port_plan.md "Results"), so a PSP shot that differs from master's is a real difference on the device.
4. Hardware: GE ms per frame, translator CPU ms per shown step, the picture on the PSP.

### 11.3 Stage B: native emitters for the costly kinds

- **How.** An emitter assembles the converter's input block from the game's records instead of from VU1 memory and calls the same converter. Order, by CPU saved:
  1. P1214a0 (opaque meshes by page, 34% of what VIF1 reads, §3; 4.8 ms per full step);
  2. the sort pass emission P12d910 from P121c30 (491 effect meshes);
  3. P120860 (transform pass packets, 20%);
  4. the sea P13bf78 / P13bd00 (Hermite to Bézier);
  5. P12e8b8.
- **Check:** on every frame of both routes (x86), Stage B's GE list must equal Stage A's word for word. Then the from-boot read check (speed_notes §34), since the packet bytes are no longer written.
- **Return values.** P12d910 returns the packet pointer that its caller compares against the buffer end (P121c30 at 0x121d5c) [C]. So it cannot simply return at entry the way P1214a0 does on unshown steps; the other builders' call sites are to be checked the same way. An emitter returns the pointer the builder would have returned. It writes one DMA tag that jumps over the unwritten space, so the chain stays walkable and its FINISH / SIGNAL stay in place [I; to check with the GS line and the read trace].
- **Unshown steps.** The reduced pipeline grows by the render-only work: sort keys and radix sort, the packet builders, and the effect vertex conversion. What stays: culls and their status, the matrices the game reads (part+0, 0x142e160 ...), the view matrices in the scratchpad (0x3e70-0x3fdc, P121c30), the light requests, and the hooks (sprite_dma_Act ...).
- **The effects (the deck's largest cost).** Rain, splashes and the sea are 14 ms of the deck's logic step (speed_notes §38). Their native forms (as line_sph) could write GE vertices directly on shown steps and skip the stores only the renderer reads on the others. That is the "render-only work elision" of speed_notes §38, checked the same way.

### 11.4 GE side: facts and rules that bind the design

- **GE budget:** 40 ms per shown frame at 25 fps. The deck replay frame takes 38.3 ms with the exact fixes (§9c), but in through mode. **Hardware transform with fog adds vertex work that is not measured yet: the first measurement of this plan.**
- **VRAM, 2 MB:** two 8888 frame buffers at stride 512 x 272 rows (1,088 KB) plus 16-bit depth (272 KB) leave 688 KB. That is exactly v2's hot-texture set, with no room for a blur copy or vertex buffers. Options: the 4 MB eDRAM switch (research_review M1: one probe with a small kernel module, which needs Ffm's OK first), or a smaller hot set.
- **Rules already measured:**
  - sprites top-left corner first;
  - full-screen texture reads in 16-pixel columns;
  - textures in VRAM;
  - colour doubling for MODULATE, PS2 alpha 0x80 = GE 0xff;
  - the alpha test only where Z is written (two passes);
  - merged draws;
  - the GE never reads guest RAM, only renderer buffers double-buffered per frame, because the CPU runs the next step while the GE draws.
- **Rules from research_review**, to apply and check:
  - float24 matrices: keep them camera-relative. Mesh+0x40 is already relative; the float world positions of effect fans are made relative on the CPU (M4).
  - The guard band drops a whole primitive with any vertex outside 0..4096. The game's own clip status (0 / 1 / 2 per object and part) marks the meshes that need CPU clipping [I].
  - 16-bit depth for the PS2's Z24 GEQUAL: fit the 16 bits to the frame's own depth range (§11.8; M3's "push the near plane out", done in the depth mapping so no geometry is clipped).
  - The GE's doubled inverse blend factors are unclamped, a possible exact form of PS2 alpha above 0x80 (M5).
  - Exact sprite sampling rules (M7).
  - The texture cache, and 512 x 512 as the largest texture (M8).
  - List safety and batched kicks (M9).
- **Open, settled by pictures on the hardware:** the depth blur reading the buffer being drawn (as the PS2 does; the GE texture cache may return pixels from before the first pass) or reading a copy (+1.4 ms; 19 pixels differ in PPSSPP, §9b); fog with a negative start (`re/vu1_programs.md` §4.3).

### 11.5 First steps, in order (one change per measurement)

1. **P04+df0 converter in gsref (x86)** (done: §11.8); then the effect meshes P04+25f0 with the PS2's clipper (done: §11.9; MGS2GE v5 waits for v4's run). It runs beside the VU1 interpreter on every df0 call and is compared vertex by vertex (check 2). gsref writes the converted frame as a new replay kind for MGS2GE: model-space vertices, the matrix and the fog constants.
2. **MGS2GE v4 on the hardware.** The deck frame of v3's best run, with the df0 meshes drawn by hardware transform and fog instead of through mode. It gives the GE cost per transformed vertex and says whether the 40 ms holds. PPSSPP pictures against gsref.
3. **Stage A live in the harness, deck route** (the translator core passes its x86 check against the interpreter: §11.10): the walk feeds the translator on shown calls; df0 first, then each entry above. The PSP shows the game and every hash stays equal.
4. The remaining deck draws (post chain, HUD, font), then the other screens on the routes.
5. Stage B emitters in the order of 11.3, then the larger reduced pipeline on unshown steps.

### 11.6 Estimates (all inferred, to be replaced by measurements)

- **CPU per shown frame** (two steps) after Stage B, with a native box cull:
  - work that must stay on both steps (culls, the matrices the game reads, hooks): about 2 x 6-8 ms;
  - shown-only work: sort and emission about 4-5, effect vertex conversion 1-2, GE list build about 3.3 (merged replay, §9b);
  - total about 20-25 ms, against about 80 ms of draw and walk per frame today (2 x 39.5).
  - The 3.3 ms list build is v1's merged list, whose GE time was 1.51 ms longer (§9b). The GE times used now (v3-v5, 38.3-38.9 ms) are for unmerged lists, which the test player builds in 20-22 ms of CPU; that player rebuilds every state per batch and is not the port's builder. The port's list build is measured when Stage A runs on the PSP; until then both figures stand (audit M4).
- **IOP and sound** run on the main CPU in every step and are not in the harness's logic column (logic = step - IOP - GS - draw): deck 2.83 ms per step (v23, mean of 1,200 steps), crew deck 1.90 (v18). speed_notes §38's levers include moving this slot to the Media Engine (up to 2.7 ms).
- **Deck:** (logic 33.44 + IOP 2.83) x 2 + about 22 gives about 94.5 ms per shown frame (10.6 fps). With the logic levers of speed_notes §38 (10-14 ms per step, the IOP slot among them): 66.5-74.5 ms (13.4-15.0 fps).
- **Deck in free play (measured 2026-10-23, v25f, speed_notes §44):** the deck figures above are Otacon's scripted tutorial. Playing, the step is 92.42 ms on the PSP (logic 51.69, IOP 2.54): (51.69 + 2.54) x 2 + about 22-25 gives about 130-134 ms per shown frame (7.5-7.7 fps) [I]. For 25 fps the logic and IOP must come to about 9 ms per step, against 54. v26 (2026-10-24, speed_notes §47): 88.72 ms per step, logic 47.68, IOP 2.53: 122-125 ms per shown frame, 8.0-8.2 fps [I]; still about 9 ms needed against 50.2.
- **Crew deck (w01a):** (logic 15.20 + IOP 1.90) x 2 (hardware v18) + about 10 gives about 44.2 ms (22.6 fps). w01a has not run on the PSP since v18; harness v24 runs it.
- *Corrected 2026-10-23 after the audit (H3): the first version left out IOP and sound and gave the deck 89 ms, 64 ms with the levers (15-16 fps), and the crew deck about 40 ms. Ffm was told on 2026-10-21 "deck 15-17 fps, crew deck close to 25"; the corrected range was given on 2026-10-23.*
- **Not in these numbers yet** (each can only lower them):
  - audio output: the mixer and reverb are not built (runtime/iop_host.h runs the SPU2 state only). Planned off the main CPU: on the Media Engine (a kernel module, told to Ffm first) or through sceSas (a research item; any audible difference needs Ffm's consent). Mixed on the main CPU it would cost of the order of PS2RC's 5.8 ms per frame on GTA [I, not MGS2's number] (audit H4);
  - the GE and the CPU on one bus: the GE times were measured with the CPU idle. v1's test with a CPU copy loop added 20.9 ms to the GE frame (§9b); the logic steps' misses may slow down too while the GE reads RAM [I]. MGS2GE v6 measures both (audit H2);
  - hitches: the vb 24667 step (145 ms) and 12 IOP / sound spikes of 21-51 ms on the deck (audit M3).
- **Code size:** every PSP build so far links coverage-reduced code (functions the test routes never enter are left out). Measured 2026-10-23 (audit H1, feasibility.md §9): the full code is ELF 4.20 MB + w00a 16.33 MB (w11a 21.88 MB) at -O2, against 6.4 MB reduced; it does not fit next to the 32 MB guest RAM. Plan there: compiled hot set per stage + an exact interpreter for the rest.
- **Memory** (the figure used so far, about 4.8 MB free plus 2 MB of the volatile block, is the start-of-run largest heap block plus 764 KB outside the heap. The start reading is 4,096 KB in all five runs v19-v23, and at the end of v23's run the largest block is 2,960 KB. v24 prints mallinfo and the kernel's total free at the start and the end; size against the end figure (audit M2)):
  - converted textures: the deck view needs about 1.5 MB with edges, the whole stage 3.53 MB;
  - two GE lists of about 130 KB;
  - two effect vertex buffers of about 0.4 MB (15.4 k vertices);
  - Stage B mesh caches, to measure.
  - A texture budget with eviction is likely needed.

### 11.7 Decisions for Ffm, later, with pictures and numbers

- **Aspect:** the 4:3 picture in 362 x 272 with side bars (as on a TV; the faithful default, recommended) or stretched to 480 x 272. A wider view is not offered: the game's own culls would leave the extra sides empty and pop objects in [I].
- **Motion trail at 25 fps:** blend the previous shown frame at 25% (recommended, same weights as the PS2) or at 50% (§9).
- **Pacing:** 25 frames per second on a 59.94 Hz LCD (2-3-2-3 refreshes per frame).
- **The 4 MB eDRAM probe:** a small kernel module, new for this port.
- **16-bit frame buffers:** only if a scene goes over the GE budget (a visual change).

### 11.8 Step 1 results: the df0 converter, replay pictures and 16-bit depth (2026-10-21)

**The converter (`ge/vu1conv.c`; gsref `GSREF_DF0CHK=1`, capture deck_fd8, vb 24050-24057).**
- 11,696 df0 calls, 258,544 vertices. A model of the GE (float24 matrices and viewport, near plane only, guard band 0..4096) against what the micro program kicked, max difference: x 0.072 px, y 0.078 px, depth 0.54 of a Z24/256 step, fog 1.0 level, UV 6e-5 texels. Colours, UVs and strip flags: 0 differences.
- 98.4% of the calls take the program's no-clip path. Of the 140,880 triangles: 87,623 inside, 37,987 (27%) trivially off-screen, 15,270 cut by the screen edge only (the GE scissors them); 0 near the near or far plane, 0 over the guard band. So on the deck the GE draws df0 without any CPU clipping.
- GE strips: 58,832 for the 258,544 vertices (4.4 per strip); the ADC bit only marks strip restarts. UVs are 2..4094 (u) and 2..2762 (v): no negative UV, so whether the GE reads 16-bit texture coordinates as signed is still open (the converter refuses a negative one, -3).

**Replay v4 file (`GSREF_GEOUT` + `GSREF_ZSCALE`, GER2):** vb 24056 has 1,439 df0 calls (32,118 vertices, 7,305 strips, 43,850 joined indices) as transform batches beside their PS2 primitives; the player draws one or the other.

**PPSSPP pictures (software renderer) against gsref, vb 24056.** Each PSP pixel is compared with the closest of the 3 x 3 PS2 pixels it covers (the 512 x 512 to 480 x 272 squeeze alone puts 18% of pixels over 16 levels with a one-pixel comparison). Mean error in levels, share of pixels over 16 and over 32:

| Mode | Compared with | depth Z24 / 256 | depth Z24 x 0.2 |
|---|---|---|---|
| v3 best (through mode) | PS2, no fog | 4.82, 5.00%, 0.59% | 4.77, 4.84%, 0.52% |
| df0 transformed | PS2, no fog | 4.84, 5.03%, 0.56% | 4.77, 4.85%, 0.48% |
| v3 best (through mode) | PS2, fog on df0 | 11.08, 20.5%, 9.8% | 11.04, 20.4%, 9.8% |
| df0 transformed + fog | PS2, fog on df0 | 3.57, 1.39%, 0.27% | 3.51, 1.24%, 0.19% |
| the same, strips joined | PS2, fog on df0 | 3.57, 1.39%, 0.27% | 3.51, 1.24%, 0.19% |

- Hardware transform is as close to the PS2 as through mode, and with fog it is far closer: through mode cannot fog. Joined strips change 59 pixels by 16 levels at most (rounding).
- The transformed and through pictures still differ by more than 16 levels on about 1,090 pixels, mostly along geometry edges: one-pixel coverage changes, from sub-pixel rounding (inferred: the PS2 truncates positions to 1/16 pixel, the GE rounds). Both are equally far from the PS2 on average.

**16-bit depth.**
- **What showed it:** with only df0 transformed, the deck's window panes lost triangles. The panes (P04+780, the lit-mesh program, through mode in that test) are drawn GEQUAL 98 Z24 units in front of their walls (gsref `GSREF_PICK`): 0.38 of a Z24/256 step. Through mode floors each vertex's Z24/256, the GE computes its own, and the two differ by a fraction of a step, so a 0.38-step gap flips.
- **Census** (`GSREF_ZCENSUS`, 8 frames): a 16-bit buffer that truncates Z24/256 never turns a passing GEQUAL into a failure, but it passes 204 k fragments the PS2 rejects (25.5 k per frame; |Z24 difference| 1..255, 89% of them 16..255): surfaces just behind others show through. Most are df0 against df0 (127 k) and P04+780 against itself (62 k).
- **Picture cost** (`GSREF_ZQ`: gsref with a depth buffer of fewer bits, against its own 24-bit picture, 8 frames of 512 x 512 PS2 pixels):

| GE depth | Pixels changed per frame | Over 16 levels |
|---|---|---|
| Z24 / 256 (the whole range) | 2,524-3,939 | 778-1,186 |
| min(Z24, 8M) / 128 | 1,510-2,586 | 483-760 |
| min(Z24, 4M) / 64 | 706-1,566 | 230-370 |
| min(Z24, 2M) / 32 | 282-773 | 71-124 |
| min(Z24, 1M) / 16 | 136-408 | 33-68 |
| min(Z24, 512k) / 8 | 70-222 | 17-40 |

- **Why a smaller range is safe on the deck:** in these frames every draw that writes depth stays below Z24 = 292 k (largest per frame 291,366-292,004). Fragments above 1 M come only from draws that test without writing (11.7 M fragments at 8-16 M, such as P04+3740's view-aligned sprites (rain) at Z24 = 16.46 M, which pass everywhere either way). Only 2 depth-tested primitives cross 2 M, none writing depth.
- **The GE's side (PPSSPP's model, `GPU/Software/TransformUnit.cpp`, `Clipper.cpp`, said there to match hardware tests):** screen depth = z / w x the viewport's z scale + z centre, any affine map of the PS2's Z24, since both are affine in z / w. Through-mode z is the float truncated per vertex, then interpolated. GU_CLIP_PLANES (command 0x1C) is the depth clamp: on, a vertex's depth is clamped to 0..65535 and a primitive is dropped only when all its vertices are out on one side; off, one vertex outside 0..65535 (or z / w beyond 1.00003) drops the whole primitive.
- **Other scenes** (new captures on the boot route, 4 vblanks each: vb 5000 bridge traffic, 9000 Snake on the bridge rail, 13000 Snake in the rain with credits, 17000 codec): the written Z24 range of the main depth buffer is 4.3 k-71 k, 4.2 k-704 k, 4.6 k-532 k and 0-1.32 M. At vb 13000 a second target with its own depth buffer (fbp 0x2080, zbp 0x2000, fans of P05+788) writes 13.9 M-15.9 M: the range belongs to each depth buffer, not to the frame.
- **Design rule:** per depth buffer and frame, the GE's 16 bits cover the Z24 range that depth-tested draws write, [lo, hi], widened by m = (hi - lo) / 16 on each side: GE depth = clamp((Z24 - lo + m) x 65535 / (hi - lo + 2m), 0, 65535), for the viewport's z terms and for through-mode z alike. Outside the range the order against written depth is kept: a fragment nearer than every write passes GEQUAL either way, one farther than lo - m clamps to 0 and passes only where the buffer is still clear (0), as on the PS2. Draws that only test may lie beyond the range; GU_CLIP_PLANES on clamps their vertices (a primitive crossing the clamp gets a bent depth, rare: 2 on the deck cross 2 M). The range is known from the previous shown frame (on the deck, hi moves about 100 Z24 units per frame against a margin of 18 k); the translator also measures the current frame's range as it walks (Allegrex min / max on the converted positions, box corners through the matrix; Stage B: the game's object boxes) and re-emits the list in the rare frame that leaves the range (a camera cut).
- **Check of that rule** (`GSREF_ZMAP`: gsref with each frame's measured range per depth buffer, per-vertex depth truncated to 16 bits and interpolated, as PPSSPP's GE does; against the same frames drawn with Z24). Pixels changed, and over 16 levels, in all frames of each capture:

| Capture | Z24 / 256 | Range per depth buffer |
|---|---|---|
| deck vb 24050-24057 (8 frames, no fog) | 27,989 / 8,839 | 849 / 108 |
| vb 5000-5003 bridge traffic | 112,112 / 861 | 1,624 / 0 |
| vb 9000-9003 Snake on the rail | 11,309 / 46 | 770 / 9 |
| vb 13000-13003 rain, credits | 18,335 / 148 | 1,540 / 0 |
| vb 17000-17003 codec | 170 / 47 | 17 / 2 |

- **Replay check:** with Z24 x 0.2 (range 327 k), the window panes draw right and every mode moves closer to the PS2 (table above). The v4 replay file carries the scale in its header.

**MGS2GE v4 on the PSP** (delivered 2026-10-23 10:27, EBOOT.PBP b13699d0, deck_ge.bin 9fb5e4b6; run by Ffm, log `prof/hw_ge_v4/log.txt`) [C]. Vb 24056, 39 frames x 3 rounds per mode; the numbers are means over all rounds (the player's round lines are running means).

| Mode | GE ms (mean, min) | CPU list build ms |
|---|---|---|
| best (as v3: df0 in through mode) | 38.70, 38.62 | 20.02 |
| df0 transformed | 38.42, 38.33 | 21.22 |
| df0 transformed, fog | 38.90, 38.81 | 21.35 |
| df0 transformed, fog, strips joined | 43.53, 43.42 | 19.54 |

- df0 alone (10 draws per sample, 9 samples): through 11.71 ms, transformed 11.43, + fog 11.90, + fog joined 16.55.
- Hardware transform costs the GE nothing over through mode (-0.28 ms for 1,439 calls, 32,118 vertices, 7,305 strips); fog adds 0.47 ms. The fogged frame takes 38.90 ms, under the 40 ms of 25 fps.
- Joined strips: +4.65 ms on the GE for 11,732 extra indices (two per join) and 23,464 zero-area triangles, and -1.8 ms of CPU list building. One draw per strip is the better form. Whether the GE's cost is the indexed fetch or the zero-area triangles is not separated (v5 adds that part).
- The list build with transform takes 1.2 ms more CPU (matrix, viewport and texture-scale commands per call) [I].
- Pictures: the player's own shot after the 39th frame of each mode in round 1, against gsref (the 3 x 3 rule; PPSSPP = the same build and frame count, `/home/claude/ge_run4/s40`, container only):

| Mode | Compared with | PSP | PPSSPP |
|---|---|---|---|
| best (through) | PS2, no fog | 2.80, 2.07%, 0.34% | 2.85, 2.22%, 0.40% |
| df0 transformed | PS2, no fog | 2.79, 2.07%, 0.37% | 2.81, 2.11%, 0.38% |
| best (through) | PS2, fog on df0 | 14.87, 26.61%, 15.31% | 15.20, 27.28%, 15.86% |
| df0 transformed, fog | PS2, fog on df0 | 2.38, 1.24%, 0.19% | 2.44, 1.33%, 0.20% |
| the same, strips joined | PS2, fog on df0 | the same picture (identical file) | 2.44, 1.33%, 0.20% (61 pixels differ) |

- These errors are lower than the PPSSPP table above, which used single-frame screenshots of one-mode runs: after 39 frames the motion trail has settled. PSP against PPSSPP, same mode: mean 1.34-1.78 levels, 0.41-0.88% of pixels over 16 levels.
- So the GE's hardware transform with fog draws the deck's main mesh kind as the PS2 does, at no GE cost. The frame has 1.1 ms left of 40; the effects (v5) and the other mesh kinds decide whether the deck fits [I].

### 11.9 Step 1b: the effect meshes (P04+25f0) and the PS2's clipper (2026-10-21)

**The entry (`re/vu1_programs.md` §2.5).** P04+25f0 draws the rain, spray and splash sheets: strips of float world positions (V4-32 at T+0x100, stride 3), a colour per vertex (V4-16, alpha at most 0x80) and UV 4.12 with a flag word (V4-16); the world-to-clip matrix at T+0x10..0x13; fog from w. Every vertex is clip-tested. A triangle with a clip flag on any of its 3 vertices gets ADC plus bit 14 on its last vertex: the GS skips it in the strip, and after the strip's XGKICK the clipper at micro address 0x1710 draws it again as a fan. P04+2870 is the same with a back-face test in its main loop; it draws no primitive in the replay frame (vb 24056).

**The converter** (`ge/vu1conv.c` `vc_fpos`; gsref `GSREF_DF0CHK=1`, capture deck_fd8, vb 24050-24057).
- 1,976 calls (768 of +2870), 76,480 vertices. The 51,181 drawn ones, put through a model of the GE, against what the micro program kicked: x 0.116 px, y 0.087 px, depth 1.05 Z24/256 steps, fog 1.0 level, UV 6e-5 texels at most. Colours, UVs and clip flags: 0 differences.
- UVs u 4..4092, v 1026..2390 (4.12: none negative, all below 1.0); largest alpha 0x80; at most 64 vertices per call.
- 240 calls of +2870 hold zero positions and colours above 255 and draw nothing (the converter returns -4 for them).
- Triangles: 38,272 asked for by the data. 24,387 the strip draws, 11,096 trivially rejected, 2,789 go to the clipper (122 come out empty). Of those 2,789, the GE can draw 2,532 whole and let its scissor cut them (every vertex inside its 0..4096 range); 257 have a vertex outside that range and need the clipped polygon from the CPU. None crosses the near plane or lies past the far plane.

**The clipper (`vc_clip_tri`, a port of 0x1710).**
- Order: the near plane first (colours interpolated in world space), then the colours times w, then -z, +x, -x, +y, -y.
- Each plane is Sutherland-Hodgman from the first vertex, with the VU's CLIP test. A new vertex = (a d_b - b d_a) / (d_b - d_a), and its coordinate is set to +w or -w.
- The far and side planes are tested on the first 4 vertices left after the near plane only (the program clip-tests vf04..vf07 again).
- Then the polygon's back-face test (2870's cull bits), and emission as the PS2 does it: Q = 1/w, RGBA = ftoi0(col Q + 0.0005).
- On all 2,789 triangles, 2,667 of the PS2's 2,668 fans match: x and y 0.062 px, depth 0.5 Z24 units, fog below 1 level, UV 2e-4 texels, colours exact.
- The other fan is a PS2 quirk: when a plane empties the polygon, the later plane routines clip stale data left in the buffer and the PS2 kicks a zero-area fan, which draws no pixel.

**Depth (§11.8 rule).** The effects test depth (GEQUAL) and never write it, so the GE's depth clamp (GU_CLIP_PLANES) is on for them.
- With each frame's measured window (`GSREF_ZMAP`, 8 frames): 26,527 drawn triangles inside it, 151 all nearer, none all farther, 299 across its edge.
- With the replay file's fixed window (Z24 x 0.2, vb 24056): 3,320 inside, 8 all nearer, 43 across.
- A triangle across the edge gets a bent depth inside it, since the clamp works per vertex. Where that matters, the exact form splits it at the window plane on the CPU: the part beyond the window is nearer than every write and passes either way. The pictures below do not show it yet; to check with a gsref count of flipped depth tests.

**Replay v5 file** (`GSREF_GEOUT`, vb 24056).
- The 49 effect calls are GERK_XFORMF batches (`ge/replay_fmt.h`): 2,444 vertices and 1,266 clipped-polygon vertices (351 polygons).
- Triangle classes: 372 drawn by the strip, 485 rejected, 278 the GE can cut, 89 that need the CPU's polygon.
- Share of the frame's 6.33 M PS2 pixels by entry: 25f0 35.3%, +3740 sprites 27.0%, df0 23.2%, the stage post entry 0x202390 11.3%, +780 1.3%, +3a38 1.2%, the rest 0.9%. With df0 and the effects converted, 58.5% of the frame's pixels come from hardware transform.

**MGS2GE v5** (`psp/ge_replay`; delivered 2026-10-23 10:45 to `mg2 port\MGS2GE_v5` after v4's run: EBOOT.PBP 8202d90e, deck_ge.bin 3c33b098). Changed after v4's run (§11.8: joined strips cost the GE 4.65 ms):
- the effects' strip triangles are drawn one run per draw (196 runs of 764 vertices), no longer joined into one indexed strip;
- the last frame mode draws df0 with one draw per strip (xf 2, not the joined xf 3);
- two df0 parts: transformed with fog (v4's 11.90 ms, to tie the runs together), and the same with each strip drawn indexed through an identity index list, no joins (xf 4): it tells the indexed fetch's cost from the joins' zero-area triangles.
- PPSSPP, the same frames as the PSP run (`/home/claude/ge_run5/s40`, container only): against v5 as first built, mode 1 is identical and modes 2-4 differ on 33, 1 and 70 pixels (rounding at the former joins). Against gsref (3 x 3 rule): best 2.85, 2.22%, 0.40% (PS2 no fog); effects transformed, both ways, 2.58, 2.01%, 0.38%; df0 and effects transformed with fog 2.09, 1.04%, 0.20% (PS2 with fog on df0 and the effects). The largest list is now 3,379 KB of 4,096 (the df0 parts, 10 draws each).
- The effects with hardware transform, two ways. GE cuts: the class 3 triangles drawn whole, only class 4 as the CPU's polygons. CPU cuts: every clipped triangle as its polygon, as the PS2 does.
- Both keep the PS2's order (the strip's triangles, then the clipper's in triangle order), since the effects blend: 23 of the 49 calls alpha-blend, 22 add, 4 subtract.
- Parts test: the effects alone, 10 times over the frame, in through mode and both ways. Frame modes: v3's best; with the effects either way; df0 transformed with fog plus the effects (GE cuts).

**PPSSPP pictures** (vb 24056, the 3 x 3 rule of §11.8, `tools/dev/piccmp.py`):

| Mode | Compared with | Mean, over 16, over 32 |
|---|---|---|
| v3 best (through mode) | PS2, no fog | 4.77, 4.83%, 0.52% |
| effects transformed, GE cuts | PS2, no fog | 4.48, 4.52%, 0.47% |
| effects transformed, CPU cuts | PS2, no fog | 4.48, 4.52%, 0.47% |
| v4: df0 transformed, fog | PS2, fog on df0 | 3.51, 1.24%, 0.19% |
| df0 and effects transformed, fog | PS2, fog on df0 and the effects | 3.16, 1.14%, 0.19% |

- The two ways differ on 0.6% of the pixels, by 0.01 level on average: the same picture.
- Where the transformed effects change a pixel by more than 8 levels (5,720 pixels), the error against the PS2 (one-pixel comparison) drops from 11.29 to 7.89 levels. Through mode interpolates UVs linearly on the screen, while the PS2 and the GE's transform do it perspective-correctly. It shows on the large spray sheets near the camera.
- Fog on the effects changes no PS2 pixel in this frame (2 of the 49 calls have it on).
- Emulator note: v4 and v5 build the same GE list for the best mode, except data addresses (VADDR, TEXADDR, CLUTADDR, CALL). Yet PPSSPP's pictures differ on 193 pixels in one 40 x 14 area. The same binary always gives the same picture, so PPSSPP's result depends on where the data lies [I].

**MGS2GE v5 on the PSP** (run by Ffm 2026-10-23, log `prof/hw_ge_v5/log.txt`) [C]. Vb 24056; parts 10 draws x 9 samples; frame modes 39 frames x 3 rounds; means over all rounds.

| Part alone | GE ms (mean, min) |
|---|---|
| effects as the PS2 draws them (through) | 7.72, 7.72 |
| effects transformed, GE cuts | 7.73, 7.73 |
| effects transformed, CPU cuts | 7.81, 7.80 |
| df0 transformed, fog (v4: 11.90) | 11.92, 11.91 |
| df0 transformed, fog, each strip drawn indexed, no joins | 16.48, 16.46 |

| Frame mode | GE ms (mean, min) | CPU list build ms |
|---|---|---|
| best (as v3) | 38.73, 38.65 | 21.09 |
| best, effects transformed, GE cuts | 38.75, 38.66 | 21.24 |
| best, effects transformed, CPU cuts | 38.83, 38.74 | 21.24 |
| best, df0 and effects transformed, fog | 38.93, 38.84 | 22.35 |

- **Why v4's joined strips cost 4.65 ms: the index list, not the joins.** The same 7,305 draws of 32,118 vertices, each strip drawn through an identity index list, cost 4.56 ms more than the strips drawn straight from the array (16.48 against 11.92). v4's joins added 11,732 indices and 23,464 zero-area triangles on top of that for 0.07 ms more (16.55). That is about 0.14 µs per indexed vertex, with the vertices in main RAM. Why the GE's indexed fetch is that slow is not measured; fetching each vertex on its own instead of in bursts is a guess [I]. **Rule: draw vertex arrays in order, without index lists.**
- Effects: hardware transform costs the GE what through mode costs (7.73 against 7.72). GE cuts are 0.08 ms faster than CPU cuts and draw the same picture (946 pixels differ, by at most 7 levels): GE cuts is the form to keep. Its clipped triangles still go as one indexed list per call (420 triangles in the frame); unindexed they should cost a little less [I].
- The frame with df0 and the effects transformed and fogged: 38.93 ms on the GE, 1.07 ms under 40. Its CPU list build is 22.35 ms against 21.09 for through mode.
- The same mode (best, as v3) builds its list in 21.09 ms in v5 against 20.02 in v4. The file differs (49 more batches, which that mode skips); the cause is not traced [I].
- Pictures: the player's shot after the 39th frame of round 1, 3 x 3 rule (PPSSPP: the same build and frame count, `/home/claude/ge_run5/s40`, container only):

| Mode | Compared with | PSP | PPSSPP |
|---|---|---|---|
| best (through) | PS2, no fog | 2.79, 2.06%, 0.34% | 2.85, 2.22%, 0.40% |
| effects transformed, GE cuts (CPU cuts the same) | PS2, no fog | 2.50, 1.87%, 0.35% | 2.58, 2.01%, 0.38% |
| df0 and effects transformed, fog | PS2, fog on df0 and the effects | **2.00, 0.99%, 0.20%** | 2.09, 1.04%, 0.20% |
| the same | PS2 as drawn (fog on everything) | 2.62, 2.30%, 1.00% | 2.73, 2.40%, 1.03% |

- The closest picture so far. Against the PS2 as drawn, the rest of the gap is the fog of the kinds still drawn in through mode. PSP against PPSSPP, same mode: mean 1.36-1.75 levels, 0.43-0.67% of pixels over 16.
- **Open, small: PSP v4 and v5 differ in the same mode.** Mode 1 (best, through) draws the same primitives from both files (it skips the XFORMF batches and draws the effects' THROUGH batches, `main.c` line 398); only the data addresses differ (v5's file has the 49 XFORMF batches, 103,360 data bytes more). On the PSP 1,275 pixels differ, by at most 14 levels, all inside x 11-58, y 152-213, on Snake. PPSSPP's 39-frame shots of v4 and v5 are identical (`/home/claude/ge_run4/s40`, `ge_run5/s40`; the 193-pixel emulator difference above came from single-frame screenshots [I]). Against the PS2 both PSP shots score the same (2.796 and 2.793 mean; 3.91 and 3.69 on those pixels). So the GE's output there depends on something PPSSPP does not model: where the data lies, or timing [I]. Candidates: the texture cache while the depth blur and trail sample the frame being drawn, or where the 1% of texels left in RAM lie. To test later: the same build run twice (is a PSP run repeatable?), then that mode without the depth blur and without the trail.

**Next:** draw everything in order without index lists (df0 does already; the effects' clipped triangles as runs). Then Stage A live in the harness (§11.5 step 3). Paused 2026-10-23 10:51 at Ffm's request until the audit is done.

### 11.10 Stage A: the translator, the GS front end, the textures (2026-10-21 to 2026-10-24; steps 1-5 core and capture checks, 6 live on both routes, 7 the PSP texture path)

**Code.** `ge/xlate.h`, `ge/xlate.c` (portable C, no micro code).
- Input: the VIF1 words in stream order, each with its guest address, and the PATH3 quadwords.
- It models the VU1 data memory from the uploads only (gsref's VIF code: UNPACK with STCYCL, STMASK, STROW, STMOD, the double buffer), the three GIF paths, and the GS registers in A+D numbering (TEX2 merges into TEX0 as on the GS).
- Entries are named as gsref names them: the guest address the code at the start address was uploaded from. P04 entry X is 0x1983e0 + X + 8·(X >> 11), one 8-byte MPG command per 2 KB chunk [C].
- At each MSCAL / MSCALF / MSCNT it sets TOP and ITOP as VIF1 does and looks up the entry's rule: where the program's first A+D packet is, and the GIF tag template whose PRIM its geometry starts with. It applies both, then hands the call to the caller's converter with the VU memory and the GS registers.

| Entries | A+D packet | Tag template | Also |
|---|---|---|---|
| P04 (780, a70, df0, 1088, 25f0, 2870, 2b60, 3740, 3a38), P06 (+0, +358) | T+0xe8 | T+0x15 | |
| P09 0x1a8148 | abs 0x78 | abs 0x11 | |
| P01 0x194478 (MSCAL) | abs 0x314 | abs 0x319 | copies abs 0x319 to abs 0x130 and 0x220; an MSCNT after it is entry 0x1944c0 |
| P01 0x1944c0 (MSCNT) | none (the MSCAL's) | abs 0x319 | |
| stage program 0x201f78, 0x202008, 0x202170, 0x202390 | none | abs 9 | the setup entry 0x201f78 copies abs 9 to both buffers' T+0xd0 (abs 0x180, 0x320) |

The check below confirms each rule on every call: the first XGKICK of every call of an entry with an A+D rule reads that address, and the PRIM at its first vertex is its template's. P09's template (abs 0x11) was found by searching the VU memory at its first vertex for a tag with that PRIM; the P04 / P06, P01 and stage program templates are those of `re/vu1_programs.md` §2.2, §3.2 and §3.13. The two copies are what those programs write into memory that later calls kick.

**Check** (`tools/gsref.c`, GSREF_XLATE=1 and GSREF_XLRUN; deck capture deck_fd8, vblanks 24050-24057: 22,943 VU1 calls of 18 entries, 12.06 M VIF1 words).
- The translator reports every call before gsref runs it, names every entry with the right TOP (the 24 MSCNT calls included), and meets no unknown VIF command.
- VU memory that the converters read is equal on every call: the inputs, header, matrix and templates of df0, 25f0 and 2870, and the A+D packet and tag template of every rule. In T+0..0x17 only T+0x00 and T+0x17 differ, on 3740, 25f0 and 2870 calls (output that earlier calls left in qwords the EE did not upload); those programs do not use them (trial runs).
- GS registers at the first kicked vertex of each call, for the 29 registers a primitive reads (PRIM, both contexts' TEX0, CLAMP, TEX1, XYOFFSET, SCISSOR, ALPHA, TEST, FBA, FRAME, ZBUF, and PRMODECONT, PRMODE, TEXCLUT, TEXA, FOGCOL, DTHE, COLCLAMP, PABE): equal on all 22,911 calls that draw. The other 32 kick no vertex.
- Trial runs: each call runs a second time, drawing nothing, with gsref's state saved and restored around it. The output compared is a hash of every drawn primitive (PRIM attributes, the registers it reads, the vertex fields it uses) and of the registers the call leaves. A trial with nothing changed differs on no call.
  - On the translator's memory: identical on every call of 17 entries. P09: 4 of 1,344 calls differ, all the same way: one vertex's fog is 63 instead of 64 (6 primitive records). Early in those calls (pc 0x65-0x6a) the program loads a scratch position and a row-buffer vertex that an earlier call left (abs 0x83, abs 0x186); with the PS2's values its clip test sets flags and it runs 34 more instructions [C, from the traces]. How that path changes the fog is not traced.
  - With VF1-31, VI1-15, ACC, Q, P and I scrambled (real memory): identical except the stage program's main entry 0x202008 (all 3,320 calls). It uses VF4-10, VF19, VI4, VI5 and VI15 as an earlier call left them (its setup entry or its own previous call). Where those values come from is still to trace; its converter will have to carry them.
  - Every call run on the translator's memory instead of gsref's (GSREF_XLRUN=8): all 24 images of the 8 vblanks (display and frame buffers) are byte-identical to the normal run. P09's fog change reaches no pixel in these frames.
- GSREF_XLATE=1 leaves gsref's images and the v5 replay file (md5 3c33b098) byte-identical.

**What this settles.** Without micro code and without VU registers, the uploads, the rule table and two copies give the converters of 17 of the 18 deck entries everything the PS2 program reads, and the GS state of their first primitive. The radar program also needs the registers its setup entry loads.

**Next (x86):** move the df0, 25f0 and 2870 batch builders from gsref into `ge/` as converters fed by the translator, and check that the translator path writes the same replay file as gsref's export. Then the DIRECT draws (1,464 PATH2 tags in these 8 frames), and textures from the IMAGE uploads with their EE source.

**Converters behind the translator (the first item above; `ge/xconv.h`, `ge/xconv.c`; gsref `GSREF_XLCONV=1` with `GSREF_XLATE=1`; done 2026-10-23).**
- `xc_call` takes one call as the translator hands it over (call-start VU1 memory, GS registers) and returns one GERK_XFORM (df0) or GERK_XFORMF (25f0 / 2870) batch. The GS state comes from the registers, the texture from a host callback, and the GE-side tests are float. Build it with `-ffp-contract=off`: the clip math must not fuse a multiply with an add.
- df0 needs nothing the translator lacks: `vc_df0` reads the call-start memory, and the strips come from the data's ADC bits.
- gsref's 25f0 export also reads what the micro program stores back: the clip positions and the bit 14 flags. The converter recomputes them (`ge/vu1conv.c` `vc_vu_clip`). From traces of a 25f0 call (vb 24051, call 2422) and a 2870 call (call 1973) [C]:
  - clip = ((M0·x + M1·y) + M2·z) + M3 in the VU's order (MULAx, MADDAy, MADDAz, MADDw), each product and each sum rounded to float on its own, with the interpreter's flush and clamp of inputs and results;
  - FCSET 0 runs before the loop, so vertices 0 and 1 see zero flags for the missing vertices. Triangle i (i ≥ 2, no ADC in the data) gets bit 14 when vertex i, i−1 or i−2 has a clip flag;
  - the clipper's other inputs are exact: colour = ITOF0 of the integers, UV = ITOF12 with z = 1.0.
- 2870's main-loop cull runs only on triangles without clip flags (`re/vu1_programs.md` §3.7).
- A call whose vertices all sit at one point draws nothing; the converter skips it before reading it (240 of the deck's 2870 calls also hold colour words above 255, which `vc_fpos` refuses). A strip triangle with three equal clip positions does not count as drawn. So on the deck 2870 sends nothing, as on the PS2: every one of its strip triangles has zero area (quads collapsed to points, alpha 0).
- **Check, 8 deck frames (vb 24050-24057) [C]:**
  - the replay file written through the converters is byte-identical to gsref's own export in every frame (md5 2dc8f551, e789c3c9, 38f51488, 43b8091b, 4a26aa19, 9e08070b, 3c33b098, 7507c964). About 1,485 batches are compared per frame: 0 field or data differences, 0 batches the converter lacks;
  - on every effect call that is not skipped (1,160 calls, 58,048 vertices), clip positions, stored flag words, colour and UV floats all equal the interpreter's. The converter draws exactly the 55,728 strip triangles the program kicked without ADC;
  - converter totals: df0 / 25f0 / 2870 calls 11,696 / 1,208 / 768, batches 11,696 / 396 / 0; effect triangles by class 0-4: 26,672 / 14,915 / 11,352 / 2,487 / 302 (2870 culled 512); 2,667 clipped polygons with 9,594 vertices, 122 empty, 0 back-face culled.
- **What the converter sends that the PS2 draws nothing for:** 23-26 df0 calls per frame, never an effect call. df0 gets no per-vertex CPU work, so these stay and the GE drops them. Their triangles in df0chk's GE model, per frame:
  - about 93 off the scissor and 1 over the guard band;
  - 8-14 edge-on squares: a 10 x 10 quad at model y = 0 whose PS2 positions round to one line. On the GE these slivers are under 0.01 px tall and most land on one 1/16-pixel row (zero area).
  - Only 4 triangles in the 8 frames (vb 24052 call 266, vb 24055 call 990) span a row of pixel samples, so the GE could light at most a pixel there, depending on its sample point and rounding [I]. That is within §11.8's 0.07 px tolerance.
- Two gsref details the converter does not copy:
  - the effects' two-pass test uses the first drawn vertex's alpha, where the converter takes the call's lowest; on the deck both give 0, since the effects never write depth;
  - gsref's GE range test reads XYOFFSET_1 whatever the call's context; every deck effect call uses context 1, so the results are equal.
- P04's no-clip fast paths (header.w = 0) carry most of the deck: 780 671 of 671 calls, a70 160 of 168, df0 11,512 of 11,696, 1088 56 of 56 [C, gsref count of T+0x14 w]. This corrects `re/vu1_programs.md` §2.6.
- Tools: `tools/dev/vudis.py` disassembles the VU1 code in a gsref trace (GSREF_TRACE).
- **Next (x86):** the DIRECT draws (1,464 PATH2 tags in these 8 frames), then textures from the IMAGE uploads with their EE source.

**DIRECT draws (2026-10-23).** `ge/gsfront.h`, `ge/gsfront.c`: the GS front end. Register writes (A+D, REGLIST), PACKED quadwords and the vertex queue go in; primitives come out, each with its GS state. The rules are copied from gsref (`gs_reg`, PACKED in `gif_qw`, `vkick`, `setup` as `gf_setup`). Each vertex field carries a mark when a VU1 call the translator does not run may have written it (`gf_unknown`). It compiles with gcc and psp-gcc (-Wall -Wextra: no warnings but the RULES table's missing initializers in xlate.c). Steps:
1. Done: gsref takes `gf_setup` for its own state; its 8 export md5s (above) and 24 images are unchanged.
2. Done: the translator feeds every GIF path it sees into one front end (`xl_on_prim`, `xl_front`): PATH2 and PATH3 as they come, PATH1 only the A+D packet a call kicks first; after each call's tag template PRIM, `gf_unknown(GF_U_ALL)` (the program's own tags and vertices are not seen).
3. Done, `GSREF_XLDIRECT=1` (with GSREF_XLATE=1) [C]: every primitive gsref draws from PATH2 or PATH3 is compared with the front end's (the translator takes a DIRECT block word by word, gsref at its end, so the front end's primitives wait in a queue): path, state field by field, the vertex fields the primitive uses. 8 deck frames (vb 24050-24057): **752 of 752 PATH2 primitives equal** (line strip 64, triangle strip 32, sprite 656), none uncertain, none drawn by one side only; PATH3 draws nothing on the deck (0 GIF tags; the textures come by PATH2 IMAGE). The export with those primitives' batches made from the front end's data is byte-identical in all 8 frames (the md5s above), and the 24 images are unchanged.
4. Done: the batch writer is `ge/gebatch.h` / `ge/gebatch.c` [C]. A frame of replay batches is built in memory (batches, vertices, textures, data block: what `psp/ge_replay` reads as B, V, T and D), and `gb_write` saves it as a replay file. Calls: `gb_state` (a primitive's batch state, plus flags for what the replay lacks: alpha NEVER without FB_ONLY, FBMSK, DATE / FBA / PABE, AFAIL ZB_ONLY / RGB_ONLY), `gb_prim` (the append and two-pass rules), `gb_xform` (a converter's transform batch), `gb_put` / `gb_tex` (texture data from the host). gsref's export now runs on it and keeps only its texture conversion from GS memory and the pixel counts. Check: in all three modes (plain, `GSREF_XLCONV`, `GSREF_XLDIRECT`) the 8 exports keep their md5s, stdout and stderr are byte-identical, and the 24 images are unchanged.
5. Done, `GSREF_XLTEX=1` (with GSREF_XLATE=1) [C]: textures from the uploads, with no GS memory. `ge/gsup.h` / `ge/gsup.c` (and `ge/gslayout.h`, gsref's GS block and column tables) keep where each word of GS memory came from, not the memory itself:
   - each host to local transfer is a record: its rectangle, its format and the EE address of its first IMAGE byte;
   - each GS block lists the transfers that wrote it, newest first (4 entries), with the bits of each word they wrote (CT24 bits 0-23, T8H 24-31, T4HL 24-27, T4HH 28-31). A transfer that covers a whole block takes those bits away from the older entries;
   - a word is rebuilt from the bytes the game sent, read from EE RAM;
   - the CLUT buffer keeps, per row of 16 halves, the load that filled it, with the block lists as they were at that load (CLD 1-5 as gsref);
   - what the map cannot tell (written before it started, another layout, data in pieces, a local copy, an entry dropped from a full list) reads as missing, never guessed.

   The front end tells the map of each TRXDIR, of the IMAGE data with its EE address (PATH2 DIRECT and PATH3) and of each TEX0 / TEX2 CLUT load (`xl_uploads`). P06 (+0, +358) draws up to three texture passes, each after its own A+D packet: pass 2's at T+0xf0 when header y has 0x10, pass 3's at T+0xf8 when it has 0x100, 0x200 or 0x400 (`re/vu1_programs.md` §3.4), each with its own CLUT load. The translator now gives the front end both packets after the call, in that order (`xl_on_pass` reports each pass), so after a P06 call the front end holds the last pass's state, as the GS does. Without them 10 of the 320 palettes differed.

   Check, 8 deck frames (vb 24050-24057): every indexed texture the export converts (320) is made a second time from the map and compared with gsref's conversion from GS memory.
   - 318 are equal in every texel and palette entry, and none differs.
   - 2 lack texels: the font (tbp 0x3000, T4HL and T4HH; 143,616 and 105,824 texels), uploaded before the capture starts.
   - The capture holds 408 transfers (all in the 32-bit layout, none in pieces, no local copy) and 14,152 CLUT loads. 384 block entries were dropped from full lists (64 with 8 entries per list, with the same results); a dropped entry can only make words missing.
   - The export that takes the map's textures is byte-identical in all 8 frames (the md5s above). In all four modes (plain, XLCONV, XLDIRECT, all three with XLTEX) the exports keep their md5s and the 24 images are unchanged. stdout is unchanged; stderr changes only in the translator's A+D packet count (19,575 to 21,527: the pass packets).
   - It compiles with psp-gcc (gsup.c: 9.6 KB of code). The map takes 256 KB of block lists plus its records.

   **Open.** A word is read from EE RAM when the texture is used, not when it was sent. If the game reuses that RAM in between, the rebuilt texels would be wrong. The capture's EE RAM changes only by DMA, so this check cannot show it. §11.2's x86 check (a full hash of every upload, and a report of any content change under an unchanged key) belongs to the live x86 run; the font, loaded when the stage starts, is the first case to watch. *(Answered in step 6: 0 texels differ on either route, and one stale cache key, which a rule removes.)*
6. Done, **the translator live in the x86 build (2026-10-23)** [C]. Code:
   - `ge/xlive.h` / `ge/xlive.c`: the second walk of a VIF1 / GIF transfer with `runtime/ee_io.h` dma_feed's chain rules (normal mode; QWC > 0 first; refe, cnt, next, ref, refs, call, ret, end with a two-entry ASP stack; with TTE VIF1 gets each tag's upper two words; IRQ with TIE ends the chain). It hands the translator what the exact walk hands VIF1 and the GIF, each word with its guest address (bit 31: the scratchpad). It reads guest RAM and the scratchpad and writes nothing of the game's. Portable C, for the PSP too.
   - The x86 check build, `tools/dev/build_xlive.sh <out> <x86 reference dir>`: boot/boot.c and runtime/ee_io.h with `-DMGS2_XLIVE` (io_dma_start hands each VIF1 / GIF transfer to `boot/xlive_chk.c` before its exact walk), the rest of the reference build unchanged. `MGS2_XLIVE=<from>:<to>[:<digest file>]`: per vblank a packet hash of what the second walk took, by ee_io.h's PKT_HASH / PKT_TAG rules; with `MGS2_PKT_HASH` over the same vblanks each must equal the exact walk's line. The other switches are listed at the top of `boot/xlive_chk.c`.
   - `ge/xldig.h` / `ge/xldig.c`: one line per VU1 call (entry, PC, TOP, ITOP, the A+D packet and tag template addresses, hashes of the 16 KB VU memory and of the GS registers) and one per vblank (a hash of every primitive the front end gives), from the live build's digest file and from gsref (`GSREF_XLDIG=<file>` with GSREF_XLATE=1) on a capture of the same vblanks.

   **Check 1, 8 deck vblanks (24050-24057)**, with a GS capture made in the same run (`MGS2_GSCAP`): the 8 packet hashes are equal; the live digest file and gsref's are byte-identical (16,643 calls, 752 primitives); the game hashes are unchanged (241 of 241 to vb 24100). On this capture of the current build (reduced and full calls alternate: 1,294 and about 2,867 calls a vblank) the rule checks of the first check above hold: trial runs differ only on P09 (4 of 1,344 calls, the fog case), and the GSREF_XLRUN=8 images are identical.

   **Check 2, both routes from boot** (`MGS2_XLIVE=1:<end>`): Otacon's deck tutorial to vb 25,199 and v25f's free-play route to vb 36,499. Every vblank's packet hash equals the exact walk's (25,199 and 36,499 vblanks), and the game hashes equal the references (deck 252 of 252 against v21's, free play 365 of 365 against v25f's). On both routes the translator met no unknown VIF command, no call without an entry, no A+D packet without an end and no uncertain primitive, and no chain needed the walk's 1,000,000-tag guard. PATH3 carries nothing on the routes: the exact walk's 32 PATH3 tags all come before vblank 2 (the boot's GS setup).

| From boot | deck tutorial to vb 25,199 | free play to vb 36,499 |
|---|---|---|
| DMA transfers / chain tags / quadwords | 218,518 / 117.2 M / 3.06 G | 351,158 / 203.4 M / 5.44 G |
| VU1 calls (MSCNT) | 15.02 M (29,153) | 26.69 M (73,584) |
| A+D packets / PATH2 GIF tags | 12.86 M / 4.04 M | 22.53 M / 5.90 M |
| IMAGE quadwords | 2.04 G | 3.83 G |
| front-end primitives (zero area) | 3.84 M (38,664) | 5.43 M (42,808) |
| GS transfers: all / other layout / past 2048 / local copies | 475,682 / 13,308 / 769 / 1,728 | 897,538 / 20,963 / 686 / 1,728 |
| CLUT loads | 7.23 M | 13.28 M |
| block entries dropped from full lists | 19,981 | 19,981 |

   On the deck (vb 24050-24057) a vblank carries about 1.36 M VIF1 words, 333 k walked quadwords, 236 k IMAGE quadwords (3.8 MB of texture uploads) and 2,080 VU1 calls. The x86 run takes about 6 minutes for the deck route.

   **Check 3, the textures as the GS holds them** (`MGS2_XLIVE_TEX=1`). `ge/gsup.c` built with `GU_SENT` (a dev mode) keeps a copy of each upload's bytes as they came. Every texel and palette entry a draw uses is read twice through the map: from EE RAM now, as the PSP would read it, and from those copies, which is what the GS holds. The draws: each VU1 call with a rule (the texture of its template's context after its A+D packet, and after each P06 pass packet), converted whole through `ge/texconv.c`; each front-end primitive, the texels its vertices span plus one on each side. Calls of entries without a rule (2.74 M on the deck route, 4.01 M in free play) are not in this check: without a tag template their PRIM is not known (the census below). Each upload's PSP cache key (EE address, size, 64 words spread over it) is compared with a hash of all its bytes, and every vblank each upload the map still reads is compared with EE RAM.

| From vb 1 | deck tutorial to vb 25,199 | free play to vb 36,499 |
|---|---|---|
| uploads (a vblank) | 345,533 (1.26 MB) | 642,469 (1.64 MB) |
| cache keys: new / seen again / seen with other content | 157 / 345,376 / 0 | 231 / 642,237 / 1 |
| texture uses: calls / P06 passes / primitives | 11.54 M / 1.34 M / 1.11 M | 19.49 M / 3.11 M / 1.81 M |
| indexed texels read / not known / differing | 12.85 G / 4.69 M / 0 | 20.27 G / 6.13 M / 0 |
| palette entries read / not known / differing | 899.6 M / 4,624 / 0 | 1.89 G / 3,296 / 0 |
| direct-colour texels read / not known / differing | 6.19 G / 5.97 G / 0 | 10.62 G / 10.27 G / 0 |

   - **EE RAM at use equals what the GS holds** for every texel and palette entry the map knows, on both routes (0 differ) [C]. The game sends the deck's texture pages again every frame from where they stay in EE RAM (`re/dg_objects.md` §4).
   - **One stale cache key, in free play at vb 28,619** [C]: EE 0x1ed8500, 30,720 bytes: the text image, T4HH 512 x 120 at 0x3000, which the game draws into with the CPU (the RAM dumps after vb 28,356 and 28,619 differ in 276 words, rows 7-24). Its change missed all 64 sampled words. The source had already shown 41 keys (its second at vb 15,766).
   - **The rule "a source that has shown a second key is hashed in full from then on" leaves 0 stale keys** on both routes [C]. Only two sources per route ever change: the text image, and the T4 400 x 192 image at EE 0x1470240 on w00a (deck) or EE 0x1ee0000, 131,072 bytes (free play). The rule hashes 4.9 KB a vblank in full on the deck route and 15.0 KB in free play. Uploads per vblank by size, deck / free play (route averages): up to 16 KB 40.8 / 48.3 KB, 16-64 KB 128.9 / 194.1, 64-256 KB 301.8 / 480.5, larger 787.4 / 913.8. A source's first change could still miss the 64 words [I]; the x86 check build reports it.
   - **Sources the game reuses after the upload**: 125 (deck) and 159 (free play) distinct uploads had their EE bytes changed while the map still listed them, but no texel was read after the change. The one case read later is the boot's GS clear.
   - **The boot's GS clear** [C]: 16 transfers of 256 KB from EE 0x0c00000 in vblank 0 (the whole 4 MB), all zero bytes (`MGS2_XLIVE=0:41`, which prints what each upload of vblank 0 sent). That RAM is reused within the vblank, so a map started at vblank 0 reads other bytes from EE RAM for words that only the clear wrote: the texels past the 504-pixel title text image (x 504-511 of the T4HH page at 0x3000), 155,882 texel reads on the deck route. On the PSP such words are zero: the texture path starts from a zeroed GS image and does not read the clear's EE source.
   - **The font** [C]: the T4HL half of the T4 font page at 0x3000 is built at vb 33 by a GS local copy (T4 512 x 256 at 0x2000 into T4HL at 0x3000); the T4HH half is the text image (title: 504 x 75 from EE 0x12c1c80; deck and free play: 512 x 120 from EE 0x1ed8500), sent again every frame. The title's transfer needs 18,900 bytes and gets 18,896 (whole quadwords), so its last 8 pixels never arrive. The page lies in the upper 8 bits of the Z24 buffer at 0x3000 (512 x 512), which draws leave alone.
   - **Edge cases of audit L3 / L4**: no filling write (UNPACK with WL > CL) and no MSCNT right after an MSCNT on either route [C].

   **The map (`ge/gsup.c`), now** [C]:
   - local -> local copies are resolved when they run (the pixels as the map knows them then); a copy of a pixel it does not know is not known;
   - transfers in the T4, T8, CT16 and CT16S layouts are modelled as well (a nibble, byte or half per pixel; T4 and T8 with an even width), and `gu_texel` reads any modelled format, direct colour included;
   - drawn areas: each front-end primitive and each VU1 call (in its template's context, else both) marks its frame buffer and, with the depth test on and depth written, its depth buffer within the scissor (gsref's write rules); their words read as not known until a transfer writes them. The lanes follow the format and FBMSK, so a Z24 or CT24 draw leaves bits 24-31 (the font) alone. An area is marked again only when a transfer wrote into it since (8 remembered areas). On the PSP these are the GE's render targets;
   - a block list keeps 4 entries. An entry that writes the whole block takes its lanes from the older ones, and so does an entry whose words all read as missing (a drawn area, an unresolved copy), wherever its rectangle lies in the block: it hides the older entries there anyway. An entry that writes part of a block removes an older entry of the same format, buffer and rectangle whose lanes it covers (a hidden duplicate). Without these two rules the drawn areas' edge blocks overflowed the lists: 1.34 M entries dropped on the deck route, and at vb 15,766 two scissored depth draws over the Z24 buffer at 0x3000 (105 x 97 at 55,92 and 109 x 128 at 51,76, x,y) pushed the font's T4HL entry out of block 0x3306, so 6,083 font texels read from vb 25,121 were not known. With both rules: 19,981 on each route, all of them before vb 23,243, which both routes share.
   - Check: gsref on the 8 deck frames with the translator, converters, front end and map (`GSREF_XLATE XLCONV XLDIRECT XLTEX`): stdout, the 24 images and the vb 24,056 export are byte-identical to before; the same two font textures lack the same texels; block entries dropped 384 before drawn areas, 715 with them, 0 with the hidden-duplicate rule.

   **What the map cannot tell, by texture** (the totals list the textures with the most texels not known) [C]:
   - direct colour, 5.97 G of 6.19 G texels on the deck route: the frame buffers read as textures (CT32 at 0x0000 and 0x1000, 2.73 G and 2.72 G texels; CT24 at both, 31 M), the frame copies at 0x2000 (370.5 M of 383.9 M; 1,727 GS local copies of a drawn frame) and the 256 x 256 target at 0x2080 (117.7 M): all drawn areas, the post chain's reads, which on the PSP read the GE's own render targets. The CT16 images at 0x2000 (144 x 200 and 256 x 256, uploaded each frame; 201.8 M texels) are known but for 400 texels.
   - indexed, on the deck route 4.69 M of 12.85 G texels, in free play 6.13 M of 20.27 G: all in the T4HH page at 0x3000 (from vb 1,757), its texels outside the text image, which only the boot's clear wrote (zero). Nothing else: with the block-list rules above the font keeps its entry, and every other indexed texel of both routes is known.

   **The VU1 entry census** (`MGS2_XLIVE_CENSUS`, calls by entry between the stage borders) [C]. No VU1 program runs before vb 3,557: the title, the menus and r_tnk0 draw with the GIF alone. The deck route calls 24 entries, the 18 with a rule and 6 without, all first called in d00t (the opening): 0x1a31e0 (1.78 M calls), 0x1a6930 (423 k), 0x19c9a8 (269 k), 0x1a6bd8 (133 k), 0x19cca0 (86 k), 0x19f810 (1.5 k), 2.70 M of d00t's 9.19 M calls. Of these only 0x1a31e0 runs later: 10,780 calls on d01t and 28,536 on w00a. Free play follows the same route to vb 23,243; then d01t again from vb 23,663 to 29,051 (the boarding scene, skipped with START) with 1.07 M calls of 0x1a31e0, and on w00a from vb 32,404, during the walk, two P06 entries without a rule: +0x738 (0x1a0540, 165,786 calls) and +0xab0 (0x1a08c0, 63,562 calls), likely the guards, who are not in the tutorial [I]. So the deck in free play needs these two converters beside §11.2's list, and the cutscenes the six above.

7. Done so far, **the texture path on the PSP** (`ge/texcache.h`, `ge/texcache.c`; 2026-10-24) [C]:
   - the map gives a **content identity** per texture and palette (`gu_keys`): an upload's is its format, buffer, rectangle, EE address, size, the bytes it has received and their key (the 64 sampled words, or a full hash once its source has shown a second key: step 6's rule); a resolved local copy's is a hash of its pixels; a drawn area, and a local copy of one, are marked as the GE's own render target. A texture's identity is the identities of the block lists its region covers, with TEX0's TBP0, TBW, PSM, TW, TH and CLAMP; a palette's is the CLUT loads that filled its rows, with CPSM, CSA and TEXA. With the map started at vblank 0, a word no entry holds reads as zero rather than missing (`gu_zero_empty`: the boot's clear sent 4 MB of zero bytes), except in lanes whose entry was dropped from a full list.
   - `tk_get(gr, ctxt)` gives a draw's GE texture and palette, converted on a miss and kept under those identities (LRU under a byte budget; what this frame or the one before used is never evicted, since the GE may still be drawing it). Its kinds: a texture; a render target (a GS draw wrote its texels: on the PSP the GE's own buffer); not known (the map lost part of it); refused (32-bit direct colour, a side over 512, or a 16-bit TEXA the GE's 5551 alpha bit cannot say). Everything is counted.
   - **16-bit textures** (the deck's mist: CT16 144 x 200 and 256 x 256 at 0x2000, sent every frame) convert straight to GE 5551, whose bit order is the PS2's, with no palette; `ge/texconv.c` takes the format with the same geometry rules as the indexed ones (region plus an edge, swizzled, powers of two). A TEXA of 0 for alpha bit 0 and at least 0x80 for bit 1 is what the GE's alpha bit says; any other TEXA is refused rather than approximated.
   - **Check** (`MGS2_XLIVE_TK=1`, x86): every texture and palette the path gives is compared, texel by texel and entry by entry, with a conversion made from the map at that moment. 200 deck vblanks (23,201-23,400): 18,025 uses, **0 differing texels and 0 differing palette entries**; 192 conversions and 17,833 hits; 889 KB of textures and palettes kept; no render target and nothing refused in that window. gsref on the 8-frame capture after the converter change: stdout, the 24 images and the vb 24,056 export are byte-identical.
   - **The whole deck route** (vb 1-25,199, hashes 252 of 252) [C]: 1,426,464 texture uses, **0 differing texels and 0 differing palette entries** over the 1,398,437 compared, and no conversion missed a texel the map could not give. The cache converted **1,165 textures and 1,425 palettes for those 1.4 M uses** (1,397,272 and 1,397,012 hits): 5,645 KB in all, nothing evicted at no budget, and **the heaviest single frame used 1,887 KB**. Besides them, 11,292 uses were render targets (the post chain), 8,528 not known (the title's text page over the boot clear) and 8,207 refused (the title's T4 images wider than 512). So a budget of about 2 MB per frame and 6 MB for the route is the cache's shape on the deck; with about 4.8 MB free (§11.6) the route needs eviction, a frame does not.

   Then, in order: the GE side of the texture path (render targets as GE buffers; the title's T4 images over 512 split; 32-bit direct colour), the translator on the PSP (§11.5 step 3: df0, the effects, DIRECT and the textures first; entries without a converter counted and skipped), and the converters for the remaining entries one at a time, as df0 was - on the free route also the two P06 entries of the guards.

### 11.11 MGS2GE v6: the GE with the CPU at work, and repeatability (audit H2, 2026-10-23)
**Why.** v1-v5 timed the GE with the CPU idle. In the port the CPU runs the next step while the GE draws, and both use the RAM bus (v1: a CPU copy during the GE cost the GE 21 ms). Audit H2.

**What v6 runs** (`psp/ge_replay/main.c`; v5's replay file, md5 3c33b098; v5's source kept as `main_v5.c`):
- The full frame in v5's best exact form (df0 and effects transformed, fog: 38.93 ms on the PSP), 7 modes x 40 frames x 3 rounds: (1) reference; (2) the effects' clipped triangles copied into one plain list per call (no index list; v5: df0's index lists cost the GE 4.6 ms); (3) without the depth blur and the motion trail (picture changes, measured only); (4-7) the reference while the CPU works.
- The CPU work runs in a second thread 16 priority steps below the main thread, so it runs only while the main thread waits for the GE. Its rate while the GE draws is compared with its rate alone (GE idle; 5 x 100 ms before part 2 and again after it):
  - **line reads:** each access misses the data cache (4 MB region, 4,099 lines apart), followed by a loop sized at the start for 2,041 accesses per ms: the v24 deck logic's line reads from RAM per ms (PspProf busy step, prof 24001 24101: 1,181 I-cache fills, 593 D-cache fills by loads, 267 by stores). The logic waits for RAM 43% of its time in the model; this load about 35% on the PSP's miss times;
  - **reads + written lines:** the same with 600 of the 2,041 accesses per ms stores to a 2 MB region (the model's 600 dirty lines written back per ms; the new PspProf counters `dst` / `dwb`, `tools/ppsspp_pspprof/README.md`);
  - **next list built:** the next frame's list built with pspgu into a second 4 MB buffer (uncached writes; 22.35 ms alone in v5), the port's own pattern but not its final size;
  - **64 KB copies:** v1's worst case.
- Parts: the effects alone x 10, with and without the index list.
- **Repeatability:** every mode starts from black frame buffers, and every mode's last frame is saved in rounds 1 and 2 (`shot_<mode>_r<round>.bmp`). Modes 1, 2 and 4-7 draw the same picture, so on the PSP any difference among those 12 pictures (or between rounds of mode 3) comes from the run: GE timing or a busy bus. This settles §11.9's open 1,275-pixel v4 / v5 difference.

**Emulator (PPSSPP software GE, 2026-10-23; `/home/claude/ge_run6`, ephemeral):**
- **The motion trail and the depth blur carry history.** Before the black start (first smoke run, 40 frames x 2 rounds): mode 1 after the part tests and mode 1 after other modes differed in 53,127 pixels by 1-3 levels (rows 19-213, the depth blur's rows) after 39 frames; mode 3 (neither effect) was identical in both rounds. The trail's 50% blend with 8-bit rounding leaves more than one stable value per pixel [I], so the start decides which one a pixel settles on. This does not explain the v4 / v5 difference: both compared mode 1, which came right after part 1 in both builds, with a black display buffer in the blur's rows (PPSSPP drew both the same). The black start makes every v6 mode start alike, so any PSP difference among the exact modes is the run's.
- With the black start: all 12 pictures of modes 1, 2 and 4-7 are byte-identical, and mode 3's two rounds are identical. So the plain list draws exactly what the index list draws, and the CPU loads leave the picture alone in the emulator.
- The calibration converges in PPSSPP (2,026 / 1,976 accesses per ms; the loop counts differ on the PSP, whose misses PPSSPP does not time). PPSSPP draws the frame in 0.02 ms of PSP time, so no load completes a work unit while its GE draws: the loads during the GE can only be measured on the PSP.
- **Delivered 2026-10-23 12:28** to `Desktop\mg2 port\MGS2GE_v6` (checked on the PC): EBOOT dddc42f2572c2042f84d4cfbe7d637c5 (main.c 1e2645a0), deck_ge.bin 3c33b09829d6c6f3083a6750a2919551 (v5's), README ff8e49e8c9892ca26cb0d0cb8bd03816. To send back: log.txt and the 14 pictures. To compare them with: the PPSSPP pictures of the same build (`/home/claude/ge_run6/s40b`, ephemeral; rebuild with `XCFLAGS="-DSMOKE -DSMOKE_FRAMES=40 -DSMOKE_ROUNDS=2"` if gone).

**MGS2GE v6 on the PSP** (run by Ffm 2026-10-23 13:22, log `prof/hw_ge_v6/log.txt`; pictures container only) [C]. Vb 24056; frame modes 39 frames x 3 rounds; means over all rounds (each round within 0.01 ms of the others).

| Frame mode | GE ms (mean, min) | Change | CPU work while the GE draws |
|---|---|---|---|
| reference (v5's best exact form: df0 + effects transformed, fog) | 38.93, 38.84 | | |
| effects' clipped triangles without index list | 38.85, 38.75 | -0.08 | |
| no depth blur, no motion trail (picture changes, measured only) | 34.93, 34.84 | -4.00 | |
| CPU: line reads (2,065 per ms alone) | 43.67, 43.60 | +4.74 (+12%) | 1,712 per ms, 82.9% of alone |
| CPU: reads + written lines (2,014 per ms alone) | 43.98, 43.93 | +5.05 (+13%) | 1,630 per ms, 80.9% |
| CPU: next list built (22.93 ms per list alone) | 41.82, 41.74 | +2.89 (+7%) | 30.51 ms per list, 75.2% |
| CPU: 64 KB copies (74 KB per ms alone) | 45.14, 45.10 | +6.21 (+16%) | 57 KB per ms, 76.5% |

- Parts alone: effects transformed with GE cuts 7.73 ms, without the index list 7.66.
- **H2 answered: the GE and the CPU slow each other down.** Alone the deck frame fits 40 ms (38.93); with any of the four loads it does not (41.82-45.14). The CPU keeps 75-83% of its speed while the GE draws. The rates alone before and after part 2 agree within 0.1%, so the loads are steady.
- What it means for a step [I]: the line-read load was sized to wait for RAM about 35% of its time; 82.9% speed then means RAM misses take about 1.6x as long while the GE draws. The v24 model has the deck logic waiting for RAM 43% of its time, so the same factor would make it about 25% slower whenever the GE is busy, which in a pipelined frame is almost all the time. The harness's step times (GE idle) are therefore optimistic for the port by up to that much. To measure in the port itself, not to apply blindly: the logic's misses are not the test's evenly spread misses.
- **Repeatability settled:** all 12 pictures of modes 1, 2 and 4-7 (both rounds) are byte-identical, and mode 3's two rounds are identical. So on the PSP the same list from the same start draws the same picture, also under every CPU load, and the plain list draws what the index list draws. The v4 / v5 difference (§11.9: 1,275 pixels on Snake, data addresses differed) stays open: v6 used one file, so it does not test addresses.
- Pictures (3 x 3 rule): reference mode against the PS2 with fog on df0 and the effects 1.98 levels, 0.98% over 16, 0.20% over 32 (v5 2.00, 0.99%, 0.20%); against the PS2 as drawn 2.60, 2.28%, 0.99%. Mode 3 against the PS2 with fog on df0 and the effects 2.43, 1.51%, 0.34%. Against PPSSPP's picture of the same build: 80,041 pixels differ, mean 1.38, 0.43% over 16 (v5 1.36-1.75). Against the PSP's v5 picture of the same mode: 54,069 pixels differ, mean 0.43, none over 16: the start-state history of the depth blur and the trail that PPSSPP showed (above).

### 11.12 Stage A on the PSP: the volumes, and the plan (2026-10-26)

**Census** (x86 live check on v30's x86 objects: `tools/dev/build_xlive.sh /home/claude/xlive/x30 /home/claude/v30/x86`, ge/ built with `-DXL_CENSUS`; `MGS2_XLIVE_VCEN=1` in `boot/xlive_chk.c` prints a line per vblank, the UNPACK kinds and the data per entry; runner `/home/claude/xlive/x30/runv.sh`, ephemeral; `tools/dev/vcen_sum.py`). Four 100-vblank windows on the reference routes; every vblank's packet hash of the second walk equals the exact walk's (100 / 100 on each). The walk at vblank N carries the packets of the draw call of vblank N - 1 (§11.2), so a vblank is "shown" or "reduced" by that call. Means per vblank, shown / reduced [C]:

| | deck tutorial 24,001-24,100 | deck free play 31,501-31,600 | indoors, walk 32,601-32,700 | indoors, alert 36,901-37,000 |
|---|---|---|---|---|
| VIF1 words | 1.49 M / 1.20 M | 1.06 M / 1.01 M | 1.15 M / 1.08 M | 1.49 M / 1.37 M |
| texture uploads (IMAGE) | 3.68 / 3.67 MB | 3.05 / 3.05 MB | 3.73 / 3.74 MB | 4.83 / 4.82 MB |
| UNPACK data | 1.91 / 0.88 MB | 0.95 / 0.76 MB | 0.64 / 0.37 MB | 0.86 / 0.44 MB |
| UNPACK vectors (V4-16 / V4-32 / V2-16 masked) | 117.5 k / 52.6 k / 43.5 k; 50.3 k / 28.8 k / 9.5 k | 53.7 k / 29.6 k / 17.1 k; 42.3 k / 24.4 k / 11.3 k | 40.6 k / 15.0 k / 21.5 k; 21.6 k / 10.0 k / 11.8 k | 56.7 k / 19.2 k / 29.4 k; 27.5 k / 10.9 k / 14.5 k |
| micro code (MPG) | 54 KB / 54 KB | 54 / 54 KB | 46 / 46 KB | 45 / 45 KB |
| VIF command words | 38.7 k / 16.9 k | 20.5 k / 16.1 k | 15.1 k / 10.1 k | 21.2 k / 12.7 k |
| VU1 calls: converter / rule only / no rule | 1,632 / 1,161 / 0; 247 / 1,051 / 0 | 537 / 932 / 0; 286 / 886 / 0 | 434 / 354 / 94; 140 / 301 / 94 | 658 / 448 / 59; 140 / 372 / 61 |
| A+D quadwords kicked from VU memory | 21.1 k / 9.1 k | 11.3 k / 8.9 k | 8.9 k / 6.1 k | 11.1 k / 6.4 k |
| front-end primitives (DIRECT) | 93 / 92 | 85 / 84 | 49 / 49 | 65 / 65 |
| per vblank, both kinds: DMA transfers, chain tags, texture uploads, CLUT loads | 12, 11.5 k, 50, 1,210 | 12, 8.1 k, 40, 876 | 10, 5.3 k, 49, 695 | 10, 7.1 k, 59, 791 |

- **Texture uploads are most of the stream**: 3.0-4.8 MB a vblank, 63-84% of the VIF1 words, and as many on reduced vblanks as on shown ones. The reduced pipeline (`boot/dg_lite.h`) skips packet builders, not the uploads: the game sends its textures again every frame (§11.10 "The font" aside). No path-3 (GIF channel) data in these windows.
- **UNPACK uses three formats and seven CL / WL patterns** [C]: V4-32 (mode 0, unmasked); V4-16 (signed; mode 0, and mode 1 = add ROW in CL 1 WL 1 only); V2-16 masked, always with the mask 0x00ffff10 and WL 1, so only its row 0 counts: x and y from the data, z = ROW z, w = y. CL / WL: 1 / 1, 3 / 1, 3 / 2, 6 / 1, 4 / 4, 12 / 1, 12 / 4. Never WL > CL (no filling), never mode 2 (no running sum), never a write-protected lane, never unsigned. So any UNPACK can be decoded later from its source, its kind and a copy of ROW.
- **The micro code is uploaded again every vblank** (45-54 KB, the same programs).
- **Data by entry** (vectors unpacked before each entry's calls, 100 vblanks): deck: df0 5.59 M, 25f0 2.44 M, 2870 0.80 M (the converters: 59%), 3740 2.01 M, P06+0 1.75 M, 2b60 0.66 M, P06+358 0.63 M, +780 0.48 M, P09 0.29 M, stage program 0.19 M. Indoors (walk): P06+0 2.05 M and P06+358 0.98 M (people, [I]) against df0 1.44 M and the effects 1.24 M; the alert adds P06+0x738 (0x1a0540, 4,190 calls, 0.46 M) and P06+0xab8 (0x1a08c0, 2,063 calls, 0.18 M), with no rule yet (the guards, as on the free deck route [I]).
- **The crew's quarters' stage program has no rule:** its entries are named 0x201a18 (100 calls), 0x201aa8 (9,100), 0x201c10 (100), 0x201e30 (79), against w00a's 0x201f78 / 0x202008 / 0x202170 / 0x202390: the stage program sits in each stage's own code image, so its upload address moves [I: the same program; to check by its words]. It is the radar [I]: none of its calls in the alert window, where the radar is jammed. Rules by the micro code's words rather than its address will cover every stage.

**Why the per-word translator cannot run on the PSP** [I]: 1.0-1.5 M words a vblank through `xl_vif1` (a call, a switch, a store per word) is of the order of 30 M instructions, about 100 ms per vblank on the PSP; and decoding every UNPACK into a 16 KB VU memory model (the size of the PSP's whole data cache) writes 1.5-3.4 MB per shown deck frame.

**The plan for the PSP.** Each step is checked on x86 against the per-word translator through the digest file (`ge/xldig.c`: a line per call and per vblank must be equal), on the four windows above and from boot:
1. **Spans, not words** (`ge/xlfast.c`, beside xlate.c): the second walk hands over each tag's data as one span (pointer, quadwords, guest address); the translator decodes commands within it, carrying a command over span borders as now.
   - IMAGE: one `gu_image` call per packet's run within a span (it takes a byte count and checks contiguity itself).
   - MPG: the source of each micro instruction as now, skipped when the same range comes again from the same source.
   - UNPACK, decoded lazily: a descriptor per command (source pointer, kind, ROW copy, CL / WL, start) and, per VU quadword, the descriptor that wrote it last. A quadword is decoded when something reads it: a rule call's A+D packet and tag template, P06's pass packets, a converter's input. At the end of each transfer the quadwords still owned by a descriptor are decoded, since the game may reuse that RAM before the next one.
   - The rest (STCYCL, STROW, STMASK, BASE, OFFSET, MSCAL / MSCNT, DIRECT's A+D and PACKED quadwords): as now.
2. **Reduced vblanks:** the same state work (VU memory descriptors, GS registers, the texture map with its CLUT loads), no converters, nothing to the GE. Checked by the shown vblanks' digests.
3. **Shown vblanks:** converters and front-end primitives into `GbOut`, then a GE list builder for the harness from `psp/ge_replay/main.c` (batch, texture, blend, xf_setup, xf_draw, fx_draw); textures through `tk_get` (VRAM for the hot set); the list is kicked at the vblank and drawn while the next two steps run; two lists, two vertex and texture generations (the GE never reads guest RAM). The same list builder also runs in the x86 check build (a digest of the GE list per shown frame).
4. **Converters, in the order people see them:** P06+0 and +358 (Snake and the crew; 1-3 passes, 3 lights + ambient, two-matrix skin), the guards' P06+0x738 / +0xab8, P04+780 / +a70 (lit meshes), +3740 sprites, P09 (the sea), the stage program by its words (radar, 2D lines), +2b60 lines, +3a38 quads, P01 instanced, +1088. Each as df0 was: beside gsref's VU1 interpreter on every call of its entry, vertex by vertex (§11.2 check 2).
5. **Display:** the game's 4:3 picture in 362 x 272 with side bars (§11.7's faithful default; the choice is Ffm's, with pictures); the harness's numbers stay on the end screen and in log.txt.
6. **Measurements** (PPSSPP model, then the PSP): the translator's ms per vblank of each kind, converters and list per shown frame, texture conversions, GE ms per frame; pictures against gsref's at chosen vblanks of each route.

Considered and not chosen: running the VU1 micro programs on the PSP, compiled ahead of time as the VU0 programs are (`gen_vu.h`), for the entries without a converter. It would give a complete picture without per-entry work, but every vertex would be transformed, lit and clipped by the CPU (of the order of 100-200 VU instructions per vertex [I], tens of ms per frame indoors) and it needs the eager VU memory the plan avoids; the converters put that work on the GE's hardware transform and lights. gsref's VU1 interpreter stays the x86 reference.

#### Step 1 on x86: the span front, checked and timed (2026-10-26; task #44)
**What** (in `ge/xlate.c` itself, not a separate xlfast.c): `xl_vif1_span(p, n, src)` does what n calls of `xl_vif1` do; `xl_gif3_span`, `xl_flush`, `xl_vm_sync`, `xl_vm_q`, `xl_vm_logical` (dev). An UNPACK whose data the span holds whole and that can wait (not filling (WL > CL), not mode 2 but for V4-5, not over 1,024 quadwords of its own target, no masked lane with value 3 in a row it writes) becomes a descriptor (source pointer, format, mode, CL / WL, mask, target, index of a ROW / COL copy): ring of 4,096 descriptors, 1,024 ROW / COL copies pushed only when STROW / STCOL / an eager mode-2 UNPACK changed them, and per VU quadword the descriptor that wrote it last (`own[1024]`). A quadword is decoded when read (`vmq`: rule calls' A+D packets and tag templates, P06's pass packets, the P01 / stage-program copies), before an eager UNPACK writes over it, and at a flush: the end of each transfer (`ge/xlive.c` calls `xl_flush`: the game may reuse that RAM), a full ring, `xl_vm_sync`, `xl_vm`. V4-32 unmasked mode 0 decodes as a copy. DIRECT's IMAGE data goes to the texture map a run at a time (`gu_image_run`: pieces cut where a transfer gets all its bytes, so `on_data` fires as before). `ge/xlive.c` with `fast = 1` hands over host-contiguous runs (cut at the 16 KB scratchpad and the 32 MB RAM wraps) and a TTE tag's upper half as a 2-word span.
**Check** (`boot/xlive_chk.c`: `MGS2_XLIVE_FAST=1`, `MGS2_XLIVE_SYNC=1` decodes everything before each call's callbacks; the digests hash the VU memory as it reads, `xl_vm_logical`): a build of the code before the change (container `/home/claude/xlive/xref`, sources in `src_ref/`) against the new build per word, with spans, and with spans + sync. The whole log (every packet hash, census, texture-check and totals line, but for the timing and lazy lines) and the digest file are EQUAL in all 12 comparisons over five windows: boot 0-3,000 (from vblank 0), deck tutorial 24,001-24,100, deck free play 31,501-31,600, indoor walk 32,601-32,700, indoor alert 36,901-37,000 (`/home/claude/xlive/cmpx.py`, ephemeral).
**Lazy decoding** (spans, per 100-vblank window): deck tutorial 924,678 UNPACKs (15.03 M vectors) left to decode later; decoded when read 1.20 M quadwords (8.0%), at 677 flushes 0.15 M (1.0%; 77 flushes for a full ring of descriptors), before an UNPACK wrote over them 0: about 91% of the unpacked vectors are never decoded. Free play 0.77 M read (8.6%), 0.13 M at flushes (1.4%) of 8.92 M; indoor walk 0.48 M (8.0%) and 0.12 M (2.0%) of 6.02 M; alert 0.61 M (7.7%) and 0.12 M (1.5%) of 7.96 M.
**Time on x86** (this container, ms per vblank in the walk, the translator and the callbacks, without the packet hash, digests, texture check or census lines; build with the census counters, `-DXL_CENSUS`): word by word -> spans: deck tutorial 9.64 -> 1.84 (5.2x), free play 8.07 -> 1.14 (7.1x), indoor walk 7.52 -> 0.84 (9.0x), indoor alert 9.02 -> 1.15 (7.9x) (`/home/claude/xlive/runt.sh`, jobs2.txt).

#### Step 1 on the PSP: the translator measured (2026-10-26; task #45)
**The build** (`psp/xl1.sh`; MGS2T_xl1, a measurement build, not delivered; /home/claude/xl1/d, ephemeral): state hash 2's test build (v30 build d with the new hash, `/home/claude/h2/d`) with `boot_rt.o` built with `-DMGS2_XLIVE` (io_dma_start hands each VIF1 / GIF transfer to `xlive_chk_dma` before its exact walk), `main_psp.o` with `-DMGS2_XL` (region HOST_XL timed apart: window lines, summary, steps.csv column `xl_us`; `prof A B xl` in mgs2.cfg counts only that region in PspProf) and `psp/xl_psp.c` with ge/xlive.c (spans), xlate.c (lazy UNPACKs), gsfront.c and gsup.c (content keys on) linked after the rest (Makefile `XOBJS`, `MAINX`). g_opt's OPT_XL (1024, on by default) switches it; off it does nothing, back on it starts again knowing nothing (xl_reset; the map keeps what it knew). Nothing is drawn yet and no texture converted. +43 KB code, +155 KB bss.
**Exact in the emulator** (PPSSPP JIT, `xl1.sh emu`): deck route 63 / 63 ee + iop + vfb against `prof/hashes_x86_deck_h2.txt` and no split over hashref's 559 lines; indoor route with the translator on throughout, switching every 100 vblanks from 31,800 (`MGS2_OPT_CYCLE=31800:100:ffffffff,fffffbff`) and off (`MGS2_OPT=fffffbff`): each 127 / 127 against `prof/hashes_x86_in_h2.txt` and no split over hashref's 685 lines (every vblank of the dense range, vuh included). A first switching run, whose output files collided with another run's (a bug of xl1.sh's first form: `setup_game` overwrote the caller's route variable; fixed), printed a split at its last vblank (37,800: ee only, vfb and the VU0 registers equal to the reference's); its RAM image was lost and the clean rerun did not repeat it [not explained]. The h2 build's RAM at vb 37,800 on that route (`xl1.sh dump`) is kept in the container (`MGS2XL1D_h2in/split.37800`) for any later split there.
**Cost (PspProf, `prof A B xl`: only the translator region; 10 steps; `prof/model_xl1/`, `tools/dev/prof_fn.py`):**

| window | instructions / step | I-misses | D-misses | model ms / step | PSP [I] (x1.116) |
|---|---|---|---|---|---|
| deck tutorial 24,001-24,010 (deck route) | 21.25 M | 45.2 k | 54.2 k | **110.2** | ~123 |
| indoor walk 32,601-32,610 (indoor route) | 12.23 M | 23.2 k | 28.9 k | **62.3** | ~70 |

For scale, v30's whole indoor walk step models at 31.92 ms (`prof/model_v30_in.txt`): the translator alone is two to four times a whole game step. **Stage A as built cannot be the PSP's front end.**
**Where it goes** (deck tutorial, model ms per step): the texture map (gsup.c) about half: `bl_push` 16.6 (24.6 k calls a step, each with a `memmove` and a `memset` of its block's list: those two 7.3 + 4.5 more), `gu_clut_load` 11.0, `ld_unref` 5.3, `cell_bn` 5.0, `gu_drawn` 4.8, `list_id` 2.4, `xf_ident` 2.2, `cover` 1.5, `xf_unref` 1.4; the walk and the translator the other half: `xl_vif1_span` 16.8, `vif_cmd` 4.7, `gif_qw` 4.6, `ud_vec` 4.3, `xlv_dma` 3.7, `run` 3.0, `call` 3.0, `gf_reg` 2.0, `vm_one` 1.7, `kick_vm` 1.1, `gf_packed` 1.0. The indoor walk has the same order (`bl_push` 10.5, `xl_vif1_span` 8.3, `gu_clut_load` 6.4, `memmove` 4.9). The x86 profile of the same windows (callgrind with instrumentation only inside `xlv_dma`: `MGS2_XLIVE_CG=1`; `prof/model_xl1/x86_cg_*`) agrees: 18.9 M x86 instructions a vblank on the deck tutorial (inclusive: texture uploads `gu_trxdir` 18.5%, drawn areas `gu_drawn` 16.7%, CLUT loads `gu_clut_load` 15.6%; VU1 calls `call` 33.8%), 10.4 M indoors (`gu_trxdir` 37.2%, `gu_clut_load` 16.7%, `call` 31.2%).
**Why:** the map keeps, for each 256-byte GS block, a list of the transfers that hold its words, and every texture upload (3.7 MB a step on the deck, ~14.5 k blocks) and every re-marked drawn area (a whole scissor area, thousands of blocks, each time an upload dirtied it) pushes an entry on every block it covers; each CLUT load records its rows (37 k row records a vblank on the deck). The packet side re-parses what the exact walk already parsed: 26 k VIF commands a step, 9 k UNPACKs with an owner mark per vector (150 k), 2.8 k VU1 calls each re-reading and applying its A+D packet (21 k GS register writes through the front end).
**What can make it lean** [I, each to be checked on x86 against today's translator and measured with this build's switch and region]:
1. *The texture map by generations, not lists:* an upload records itself once (its rectangle, source, key) and stamps a generation number on the blocks it covers (no lists, no memmove); a drawn area is a rectangle with a generation; a texture's identity is resolved when it is used (TEX0 / CLUT change) and cached by its registers and the generations of its blocks. Same identities as now (0 differing texels on both routes).
2. *One walk:* the exact walk (`vif1_chain_fast`) hands its parsed commands to the translator; no second walk, no second parse.
3. *UNPACK owners by ranges*, not a mark per vector.
4. *VU1 calls:* an A+D packet equal word for word to the one before it changes no GS register: compare and skip.
5. *Shown steps only,* if each shown frame's stream stands alone (the game sends its textures and micro code again every frame, census above): to check on x86 by restarting the translator at each shown step and comparing identities and calls with the continuous one; it halves the average. **Checked: it does** (below, "Shown frames only").
Target [I]: under ~10 ms per shown step. Even at zero cost for the translator a step is ~78 ms on deck free play (v29 on the PSP: logic 42.4, draw pipeline 28.3, GS walk 4.9), and the draw pipeline's packet building stays; the native emitters of Stage B (§11.3) are what remove packet building and translation together for the heavy kinds (df0, the effects, P06).

#### Shown frames only: each shown frame stands alone (2026-10-26; task #47)
**Method.** The x86 capture (`runtime/ee_io.h`, `MGS2_GSCAP`; the x86 objects of v30 built by `tools/dev/build_xlive.sh` into `/home/claude/xlive/x47`, runner `/home/claude/skc/cap.sh`, ephemeral) now writes 24-byte records (kind, channel, info byte, address, quadword). A vblank record's info byte says whether that vblank had a full call of the draw pipeline (bit 0) and a reduced one (bit 1); a transfer record's says whether the transfer started inside a call. `tools/gsref.c` with `GSREF_SKIP=1` leaves out the transfers that follow a vblank whose calls were all reduced: the packets of a reduced call, as a translator that runs on shown frames only would see the stream. `GSREF_SKIPCHK` prints per vblank the VU1 calls, the primitives and a hash of them (PRIM, the registers each reads, its vertex fields), the textured primitives and a hash of what each can read (its clamp region's blocks in the bits its format reads, and its CLUT entries), apart for reads of blocks a draw wrote last (render targets); `=2` per call, `=3` and `=4` per primitive outside the calls (options in gsref's comment). `tools/dev/skc_cmp.py` compares a run with `GSREF_SKIP=1` against one without on the shown vblanks; summaries in `prof/skc_v30/`.
**Result** [C], shown vblanks equal in everything / compared:

| window (route) | shown | reduced | equal | the rest differ in |
|---|---|---|---|---|
| deck stage start 23,251-23,280 (deck; 10 vblanks of loading, no call) | 10 | 10 | 1 | the motion trail sprite only |
| deck tutorial 24,001-24,024 (deck) | 12 | 12 | 1 | the motion trail sprite only |
| deck free play 31,501-31,524 (v25f) | 12 | 12 | 0 | the motion trail sprite only |
| indoor walk 32,601-32,630 (indoor) | 15 | 15 | 15 | |
| indoor alert 36,721-36,744 (indoor) | 12 | 12 | 12 | |
| opening 15,500-15,542 (deck) | 21 | 19 | 21 | |

- Every VU1 call of every shown vblank is equal (entry, primitives, their hashes, what their textures read) in all six windows, and so is every primitive's state and vertices; no transfer starts inside a call. The game's packets for a shown frame do not depend on any reduced frame.
- The one difference is the known one (§9 "The trail at 25 fps", speed_notes §34): the trail's full-screen sprite (CT32 at TBP 0, 512 x 510 from row 1) samples the other display buffer, which the reduced frame drew (in all three deck windows shown frames draw into 0x1000 and reduced ones into 0). On the PSP the renderer draws it from its own previous shown frame; the weight is Ffm's choice (§11.7).
- Looked like differences, are not: clear sprites at a vblank's start carry the vertex F and ST and the texture, CLAMP and ALPHA registers left by earlier packets, all unused (no texture, fog or blending), so the hash takes a register or F only where the primitive reads it; the T4HH text image, sent every frame from the first alert text (vb 36,728), changes the high nibble of byte 3 of the font page, which a byte-wide hash counted as a change of the T4HL font in the low nibble: the font itself never changes, so the hash takes each nibble apart. The walk and the tutorial were also equal in the stricter first form of the hashes, but for the trail.
- In the opening the reduced calls send nothing (0 records left out); the vblanks after a vblank without a call (kind 0) still carry packets there (15,500 / 15,502 / 15,504: 1,234 calls each), so a vblank is skipped only when its calls were all reduced, never when it had none.
- **What it saves** (means per vblank, shown / reduced): calls and primitives, deck tutorial 2,486 / 1,307 and 30.2 k / 16.1 k; free play 1,622 / 1,273 and 14.9 k / 12.0 k; walk 897 / 526 and 14.4 k / 8.7 k; alert 1,433 / 987 and 21.2 k / 14.0 k; and per the census 1.01-1.37 M VIF1 words and 3.0-4.8 MB of texture uploads a reduced vblank. So the translator and the texture map skip 37-47% of the calls and nearly half of the words and uploads.
- **On the PSP:** the translator tests one flag at each transfer start: were the calls of the vblank before all reduced (boot.c `rt_dg_call` knows: `dg_shown_vb`)? It keeps its state across the skipped vblanks (VU memory model, GS registers, texture map). What this check shows is the GS side: every shown frame reads the same GS content and state either way. The texture map also names each block's EE source and reads it when the texture is used, so the map with the skip had its own check in the live x86 run (below).
- **Live x86 texture check with the skip** [C] (`prof/skc_v30/live_skip.txt`; `boot/xlive_chk.c` `MGS2_XLIVE_SKIP=1`: transfers after a vblank whose draw-pipeline calls were all reduced, by boot.c's `g_dg_calls` counters, are not handed to the translator; build `/home/claude/xlive/x48`, `MGS2_XLIVE_TEX=1 MGS2_XLIVE_TK=1`, 100 vblanks a window, each run with and without): every texel and palette entry the draws read through the map equals the GS's, and so does every texture and palette the PSP texture path builds; the same upload keys (none seen with other content) and the same conversions, so the same textures; uploads halve. In each window 50 vblanks followed reduced calls only.

| window | uploads all / skip | texture uses | texels compared (M) | palette entries (M) | differing | PSP path uses / conversions | differing |
|---|---|---|---|---|---|---|---|
| deck tutorial 24,001-24,100 | 3,472 / 1,735 | 195,750 / 134,819 | 155.9 / 120.9 | 16.4 / 9.3 | 0 / 0 | 19,621 / 16,080, 337 both | 0 / 0 |
| deck free play 31,501-31,600 | 2,800 / 1,400 | 134,244 / 74,447 | 88.6 / 64.3 | 16.5 / 8.5 | 0 / 0 | 13,028 / 9,616, 215 both | 0 / 0 |
| indoor walk 32,601-32,700 | 3,424 / 1,712 | 97,836 / 57,593 | 97.9 / 77.7 | 15.3 / 8.3 | 0 / 0 | 14,228 / 11,647, 253 both | 0 / 0 |
| indoor alert 36,901-37,000 | 4,163 / 2,084 | 115,592 / 72,798 | 142.3 / 110.9 | 16.4 / 9.5 | 0 / 0 | 19,082 / 15,708, 380 both | 0 / 0 |

  The texels the map cannot give are render targets in both runs (the trail's display buffers at TBP 0 / 0x1000, the font page's halves at 0x3000 that a local copy builds), half as many with the skip. The as-sent scan finds the same transfers changed in EE RAM after they were sent (deck tutorial 2, EE 0x137f450; alert 1, EE 0x1356fb0; 18,896 bytes each), no texel read from the changed bytes either way; the skip run counts each in two vblanks because no later upload replaces it in the skipped vblank. So the translator and its map may run on shown frames only.

#### The lean texture map (2026-10-26; task #47)
**What** (`ge/gsup2.c`, built in place of `ge/gsup.c` behind the same `gsup.h`; the design in its header comment): the same words and DRAWN / LOST flags as gsup.c, whose rules it keeps, and identities that change whenever what they name changes, with none of gsup.c's per-block lists:
- each 256-byte block has a state word per lane group (bits 0-23, 24-27, 28-31): nothing, the one record (a transfer, a local copy or a drawn area) that wrote all of the block in that group, or a list of records, newest first, where records wrote parts of it. A list never changes once made (a push makes a new head), so a CLUT load keeps the state words it read as its snapshot;
- a write of all 32 blocks of an 8 KB page in a lane group is one page state and a mask, not 32 block writes (97% of the uploaded blocks fill whole pages);
- no reference counts: records, list nodes and CLUT loads that nothing names any more go back to their pools in a sweep at the start of a write when a pool runs low (22 sweeps in the deck window's 100 vblanks);
- a host -> local transfer goes into the blocks when its data is all in (or when the map is read first, or when the next transfer starts), so its identity is final when the blocks get it;
- drawn areas (16 at most) are kept by their rectangles and marked again only after a write that is not a draw went into one of their blocks in their lanes (each page knows the drawn areas that may cover it); `gu_drawn` checks the last two areas first;
- a CLUT load takes the load kept last for its parameters (a table of 512) when the pages it read were not written since (their write counts), else when its blocks still hold the states they held, else it is kept as a new load; the CLUT rows point at the loads;
- an area's identity (a table of 1,024) is kept by its rectangle and made again only when a page under it was written since;
- content hashes by 32-bit words (two lanes of xxHash32's round): no 64-bit products on the PSP.
Its structure is 308 KB (the block states 192 KB), with pools on top (at the end of the deck window 1,024 records of 80 bytes, 2,048 list nodes of 16 and 1,024 loads of 136: 248 KB). gsup.c and xlate.c are unchanged.

**Check** (`boot/gudual.c`: both maps in one x86 live build, gsup.c renamed by `ge/gu_ren.h`, fed the same calls; every texel, index and CLUT entry the draws read is taken from both and compared; the lean map's identities are the ones used, so `ge/texcache.c` keys its cache by them and the texture path check compares that cache with the map's texels; for every area and CLUT range asked again, whether each map's identity changed, side by side). The forms were checked on the four 100-vblank windows as they were written; form 4, the one timed below, as x56 (`/home/claude/xlive/x56`, ephemeral, built after the timing: x55, the check before it, ran the source of 20:32, before form 4's three changes, and gave the same lines), all frames [C]:

| window | texels and indices compared | CLUT entries | area identities asked again: same / both changed / one only | CLUT identities: same / both changed / one only | texture path |
|---|---|---|---|---|---|
| deck tutorial 24,001-24,100 | 423.2 M, 0 differ | 34.3 M, 0 | 19,218 / 67 / 0 | 451 / 19,153 / 0 | 19,621 uses, 337 conversions, 0 differ |
| deck free play 31,501-31,600 | 287.5 M, 0 | 34.3 M, 0 | 12,813 / 0 / 0 | 298 / 12,713 / 0 | 13,028, 215, 0 |
| indoor walk 32,601-32,700 | 197.5 M, 0 | 32.5 M, 0 | 13,975 / 3 / 0 | 500 / 13,711 / 0 | 14,228, 253, 0 |
| indoor alert 36,901-37,000 | 287.0 M, 0 | 35.4 M, 0 | 18,702 / 3 / 0 | 541 / 18,524 / 0 | 19,082, 380, 0 |

Identity flags 0 differ in all four; the texture path's conversions are the ones of "Shown frames only" above (337 / 215 / 253 / 380). The CLUT identities change between most uses in both maps alike: the game loads different palettes into the same CLUT rows between uses (the texture path's palette cache, keyed by identity, still hits: 19,283 of 19,621 on the deck). The first form also had 3 identity changes the reference did not have (a skipped repeat upload kept its old place under a neighbour that shares a block edge; fixed, then repeat-upload skipping was dropped in the second form) and marked drawn areas again 182 times in the indoor alert where gsup.c did 93 (fixed: marked again only where a write landed; the last form marks 93 there, and 93 in the deck window where gsup.c marks 455). With shown frames only (`MGS2_XLIVE_SKIP=1`, x56 `*s6` and x55 `*s5` alike, the way the PSP build will run), the four windows again: texels and indices 298.7 / 184.5 / 157.2 / 224.3 M and CLUT entries 19.6 / 17.7 / 18.1 / 21.0 M compared, 0 differ; identities asked again 16,080 / 9,616 / 11,647 / 15,708 (areas both changed 67 / 0 / 3 / 3, as with all frames), one only 0 and flags 0 differ in all four; the texture path 16,080 / 9,616 / 11,647 / 15,708 uses with the same 337 / 215 / 253 / 380 conversions, 0 differ; drawn areas marked again 47 / 4 / 4 / 47 (gsup.c 129 / 4 / 4 / 47); CLUT loads found by their parameters 94 / 91 / 83 / 81%.

**Cost on the PSP model** (`psp/xl2.sh`: xl1 with ge/gsup2.c; the same windows and region as xl1, 10 steps each, `prof A B xl`; dumps in /home/claude/xl2/model, ephemeral; per-function sums `tools/dev/prof_fn.py`), model ms per step:

| form | what changed | deck tutorial 24,001-24,010 | indoor walk 32,601-32,610 |
|---|---|---|---|
| xl1 (gsup.c) | | 110.2 | 62.3 |
| 1 | state words with a reference count per block, repeat uploads skipped, CLUT loads kept once, drawn areas by lanes | 102.8 | 66.9 |
| 2 | no reference counts (sweeps), no repeat-upload skipping, the last CLUT load remembered, a faster walk over a transfer's blocks | 70.3 | 39.5 |
| 3 | CLUT loads found by their parameters (84-93% of them), drawn areas marked again only where written (deck window 366 k -> 22 k block writes), whole-page writes (deck window 1.52 M covered blocks -> 46,108 page writes and 44,876 block writes), content hashes by words | 52.6 | 30.2 |
| 4 | the last two drawn areas checked first, CLUT loads checked by page write counts, a fast case for CLUT rows | **49.9** | **27.2** |

Form 1 gained nothing: its reference counts and CLUT rows cost what the lists had. By source file, xl1 -> form 4 (deck / indoor): the map (gsup.c with its memmove / memset; gsup2.c) 62.1 / 38.1 -> **8.3 / 5.7**; the translator (xlate.c) 36.2 / 18.5 -> 32.0 / 16.7 (the same code: fewer cache misses beside the smaller map [I]); the walk (xlive.c) 6.7 / 3.4 -> 6.4 / 3.0; the GS front end (gsfront.c) 4.6 / 2.1 -> 3.1 / 1.7. Form 4 per step on the deck: 9.79 M instructions, 18.0 k I-misses, 23.5 k D-misses (xl1 21.25 M, 45.2 k, 54.2 k); its map functions: `gu_drawn` 2.29 ms, `gu_clut_load` 1.45, `rec_ident` 0.73, `rows` 0.63, `gc_maybe` 0.62, `clut_new` 0.56, `commit` 0.51, `whash` 0.39, `put` 0.37, `state_id` 0.29. **What is left is the walk and the translator: 41.4 ms of the deck's 49.9 and 21.4 of the indoor 27.2**: `xl_vif1_span` 15.41, `gif_qw` 4.34, `ud_vec` 3.70, `vif_cmd` 3.63, `xlv_dma` 3.47, `run` 2.89, `call` 2.12, `vm_one` 1.59, `gf_reg` 1.26, `kick_vm` 0.97, `gf_packed` 0.93 (deck). Next, in that order: one walk (the exact walk hands its parsed commands over: no second parse), UNPACK owners by ranges, A+D packets equal to the last one skipped, then shown steps only (each shown frame stands alone, above), each checked against today's translator on the four windows and timed with xl2.

#### One walk (2026-10-26; task #47)
**What:** the game's own exact VIF1 walk (`runtime/ee_io.h` `vif1_chain_fast`, the one-loop walk of OPT_GS_FAST) hands every run of words it feeds the VIF to the translator as well (`XL_ONE` -> `xl_vif1_span`, with the words' EE address: a RAM piece, a quadword, a scratchpad piece, a tag's two TTE words, a chain element's data), and ends the transfer with `xlive_one_end` (`xl_flush`). The translator's own second walk (`xlv_dma`: the DMA tags, the chain, the quadword reads) now runs only for GIF (path 3) transfers and for VIF1 transfers the exact walk does not take (`ch == 1 && VIF1_FAST_OK` at the start of the transfer, passed to `xlive_chk_dma`). Switches: x86 `MGS2_XLIVE_ONE=1` (with `MGS2_XLIVE_FAST=1`; the packet hash must be off, `MGS2_PKT_HASH=` empty, since with it the x86 walk is `dma_feed`); PSP `MGS2_XL_ONE` in mgs2.cfg (default 1, 0 = the second walk; read through main_psp.c's `mgs2_getenv`: newlib's getenv does not see mgs2.cfg, so xl3's first models with `MGS2_XL_ONE=0` had run the one walk). On the PSP the translator then runs inside HOST_GS: model region `xg` counts HOST_XL and HOST_GS together (`prof A B xg`).
**Check** (x56: x55's objects with form 4 of the map, `/home/claude/xlive/x56`, ephemeral): each of the four windows with all frames (`*o6` against `*n6`) and shown frames only (`*p6` against `*s6`): the translator's digests identical in all 8 (203,680 / 138,804, 132,213 / 73,514, 70,911 / 44,177, 87,489 / 58,858 lines), and every other line of the logs the same (gudual's texel, index and CLUT comparisons, identity counts and texture path; the game's state hashes; the translator's census: VIF1 words, calls, A+D packets, GIF tags, IMAGE quadwords, and the 100 per-vblank census lines without their quadword count) except the second walk's own counts (0 transfers, tags and quadwords) and the new line: 1,200 / 600, 1,200 / 600, 1,000 / 500, 1,000 / 500 VIF1 transfers fed by the exact walk. In the emulator (`psp/xl3.sh emu`, MGS2T_xl3, one walk on): deck route 63 / 63 ee + iop + vfb against `prof/hashes_x86_deck_h2.txt` and 370 / 370 against hashref (no split over its 559 lines), indoor route 127 / 127 and 434 / 434 (685 lines), the vuh lines of both equal to the h2 build's [C].
**Cost** (PSP model, one build: `xl3b`, xl3 with the mgs2.cfg fix; 50 steps, region `xg`; dumps in /home/claude/xl3b/model, ephemeral), ms per step:

| window | one walk | two walks | translator off (the GS walk) |
|---|---|---|---|
| deck tutorial 24,001-24,050 | **62.15** (12.52 M instructions, 19.4 k I-misses, 25.8 k D-misses) | 65.19 (13.17 M, 18.7 k, 28.0 k) | 6.94 |
| indoor walk 32,601-32,650 | **31.07** (6.05 M, 11.0 k, 15.8 k) | 32.23 (6.30 M, 10.1 k, 17.1 k) | 3.23 |

One walk saves 3.04 ms a step on the deck tutorial and 1.16 indoors: the second walk (`xlv_dma` 3.91 and `run` 3.06 ms on the deck) goes, and the exact walk (`io_flush`) pays 0.58 M more instructions and 3.2 k more I-misses for handing its runs over. The translator in these windows (`xg` minus off): deck 55.2 ms one walk / 58.3 two, indoors 27.8 / 29.0. A first estimate of +1 ms (xl3's 10-step model against xl2's) was wrong: 10-step windows of two builds differ by more than this effect, so changes are timed in one build from now on (a switch in mgs2.cfg) over 50 steps.

#### The lighter span loop (2026-10-26; task #47): measured, not kept
**What** (MGS2T_xl4, `psp/xl4.sh`): `xl_vif1_span` reduced to a loop that calls `vif_cmd` and a new `span_data` (no inlining) so that it holds few values; the parts of `ud_ok` that depend only on STCYCL, STMOD and STMASK worked out when one of those changes (`lz_state`); owner marks for CL = WL UNPACKs two at a store (`own_fill`); `ud_new` and `direct` not inlined. Exact (x86 x57: the eight digests of x56 equal, logs equal but for the file name).
**Cost** (10 steps, deck tutorial 24,001-24,010, region `xg`, against xl3): 60.48 ms against 56.59. The span work (`xl_vif1_span`, `ud_new`, `span_data`, `lz_state`, `direct`) took 3.85 M instructions a step against `xl_vif1_span`'s 3.43 M in xl3 (`lz_state` alone 0.50 M: STCYCL and STMOD come with nearly every UNPACK; `ud_new`'s call and nine saved registers 7.4 k times a step), and I-misses went from 16.5 k to 34.6 k a step (the pieces sit apart in the link). Not kept: `ge/xlate.c` is xl3's again.
**Where the span work goes** (xl3, deck tutorial, per step; `tools/dev/fn_ann.py`: counts per instruction): the owner marks of CL != WL UNPACKs, 98.8 k vectors x 9 instructions (0.89 M), of CL = WL ones 82.4 k x 3 (0.25 M); a descriptor per UNPACK, 7.5 k x 89 instructions (0.67 M); the call itself, 20.2 k spans a step (a TTE tag's two words and its data are two) x about 42 instructions with nine saved registers (0.8 M). UNPACK owners by ranges and fewer, longer spans are what would cut it.

#### Shown frames only on the PSP (2026-10-26; task #47)
**What** (MGS2T_xl5, `psp/xl5.sh`: xl3 with the mgs2.cfg fix, this and `ge/xlate.c` as xl3's): `psp/main_psp.c`'s `host_vb_end` hands the step's draw-pipeline calls (`g_dg_calls`, full and reduced) to `xl_psp_vblank` before it clears them; when they were all reduced, the next step's transfers are not handed to the translator, which keeps its state across them (the x86 check's `MGS2_XLIVE_SKIP` rule). mgs2.cfg `MGS2_XL_SKIP` (default 1; 0 = every transfer). log.txt's translator line says how many vblanks and transfers were skipped.
**Check, the translator's own output** [C] (x58: x56's objects with `ge/xldig.c`'s new mode `MGS2_XLIVE_DIGUSED=1`, `/home/claude/xlive/x58`, ephemeral; one walk, all frames `*u8` against shown frames only `*v8`; `tools/dev/skip_dig_cmp.py`): the whole-memory digests of the call lines (the 16 KB VU memory and the 256 GS registers at each call) differ between the two runs on most shown vblanks, because the skipped frames leave other values in parts the shown frame's calls do not read, and so does the primitives' digest (unused fields, as gsref found: an untextured sprite's ST, a fog value without fog). With `MGS2_XLIVE_DIGUSED=1` a call line carries the call's GS state as its primitives read it (`gf_setup` of the registers after its A+D packet, with the fields the PRIM does not use set to 0: the alpha test's without ATE, DATM without DATE, blending without ABE or AA1, FIX unless C = FIX, the fog colour without FGE, every texture register without TME, the CLUT unless a palette format, mip levels above MXL, a clamp region unless a REGION mode) and the primitives' digest takes only the fields each reads (S, T, Q only for STQ textures, UV only with FST, fog only with FGE). Every translated vblank equal, in all four windows: deck tutorial 50 of 50 (138,754 lines), free play 50 / 50 (73,464), indoor walk 50 / 50 (44,127), alert 50 / 50 (58,808); 50 vblanks skipped in each; the game's state hashes equal in both runs and to x56's; the texture checks as in "The lean texture map" (0 texels or CLUT entries differ, the texture path's conversions the same).
**Cost** (PSP model, xl5, one build, 50 steps, region `xg`, one walk; dumps in /home/claude/xl5/model, ephemeral), ms per step:

| window | every transfer | shown frames only | the GS walk alone (xl3b) |
|---|---|---|---|
| deck tutorial 24,001-24,050 | 63.64 (12.52 M instructions, 28.5 k I-misses, 23.8 k D-misses) | **45.11** (8.92 M, 19.1 k, 17.1 k) | 6.94 |
| indoor walk 32,601-32,650 | 31.09 (5.96 M, 13.5 k, 15.2 k) | **20.32** (3.90 M, 8.7 k, 9.8 k) | 3.23 |

The translator alone (minus the GS walk): deck 56.7 -> 38.2 ms a step (-33%), indoors 27.9 -> 17.1 (-39%); not half, because the shown vblanks carry more than the reduced ones (deck tutorial 2,486 calls against 1,307). Per shown frame (two steps) that is about 76 ms on the deck tutorial and 34 indoors. xl5's "every transfer" (63.64) and xl3b's one walk (62.15) run the same translator code: the 1.5 ms between them is the link's placement (28.5 k I-misses against 19.4 k), a reminder that cross-build differences of a few ms are layout as much as code.
**Emulator** (xl5 in PPSSPP, both routes to the end, the translator live with shown frames only; /home/claude/xl5/emu, ephemeral): the game exact, deck route 63 / 63 against `prof/hashes_x86_deck_h2.txt` and 370 / 370 against its hashref, indoors 127 / 127 and 434 / 434, vuh 311 / 311 on both (the same lines as the h2 build's), no split; the translator line: deck 4,790 vblanks after reduced calls only and 65,323 transfers not handed over, indoors 9,996 and 126,983.

#### Trimming the translator (2026-10-26 / 27; task #47): two parts kept, two measured and dropped
**What** (on xl5's translator; each step checked exact on x86 (x59 part 1, x60 parts 1 and 2, x61 parts 1 to 3: the eight digest windows of "One walk" identical to x56's, every other log line the same) and modelled on the PSP (xl6, xl7, xl8: xl5 with the step's objects; 50 steps, shown frames only, region `xg`; dumps in /home/claude/xl*/model, ephemeral)):
- **Part 1, the UNPACK path (kept):** a call's read of a waiting UNPACK (`ud_vec`) takes V4-16 (whole halfword vectors), V2-16 and V4-32 straight from the data and only the other formats bit by bit, and with no mask returns the data (plus ROW in MODE 1) at once; `ud_k` divides by CL with a reciprocal table (`RCP`: (o x RCP[cl]) >> 16 = o / cl for o < 1024 and 2 <= cl <= 16, checked for every pair); `ud_ok` works out where the last vector lands only when WL != CL (WL = 1 without a division); the owner marks of CL = WL UNPACKs go two at a store (`own_fill`), and WL = 1 needs no cycle count.
- **Part 2a, the entry cache (kept):** a call finds its entry's slot in the statistics and its rule (`xl_rule`, a pure lookup in the constant RULES table) in a 256-entry hash table (`EC`: `ent_index`, `rule_of`; cleared with the statistics by `xl_reset`) in place of two linear searches.
- **Part 2b, an A+D loop in `kick_vm` (dropped):** after a GIF tag of A+D quadwords in VU memory, those quadwords in one loop instead of one `gif_qw` call each.
- **Part 3, runs that follow on as one (dropped):** the exact walk (`runtime/ee_io.h` `XL_ONE`) kept the last run and joined the next one to it when it followed on in memory and in guest addresses (a TTE tag's upper half and the data after it), so the translator got the same words in the same order in fewer calls (`MGS2_XL_MERGE`).

**Cost** (PSP model, per step; the instruction counts are exact and decide; cache misses move with each build's placement):

| build | what | deck tutorial 24,001-24,050 | indoor walk 32,601-32,650 |
|---|---|---|---|
| xl5 | before | 45.11 ms (8.92 M instructions, 19.1 k I-misses, 17.1 k D-misses) | 20.32 (3.90 M, 8.7 k, 9.8 k) |
| xl6 | + part 1 | 45.47 (8.47 M, 30.4 k, 16.5 k) | 19.91 (3.61 M, 12.3 k, 10.2 k) |
| xl7 | + parts 2a and 2b | 47.16 (8.45 M, 39.7 k, 15.7 k) | 20.49 (3.66 M, 16.0 k, 8.3 k) |
| xl8 | + part 3 | 48.93 (8.60 M, 44.3 k, 16.5 k) | 21.11 (3.70 M, 17.9 k, 8.5 k) |
| xl8, `MGS2_XL_MERGE=0` | part 3's code, joining off | 49.67 (8.72 M, 45.2 k, 16.6 k) | 21.64 (3.79 M, 18.4 k, 8.6 k) |
| **xl9** | **xl5 + parts 1 and 2a (kept)** | **45.68 (8.35 M, 28.8 k, 21.5 k)** | **19.95 (3.63 M, 11.9 k, 10.4 k)** |

By function on the deck, M instructions a step: part 1 `xl_vif1_span` 2.775 -> 2.467 and `ud_vec` 0.643 -> 0.463 (together -0.49; the whole build -0.45); part 2a `call` 0.338 -> 0.243 (-0.095); part 2b `kick_vm` 0.192 -> 0.275 (+0.083: its test runs at every quadword kicked from VU memory, and the A+D runs it would take are short); part 3 `io_flush` 1.773 -> 2.039 (+0.27: the join's test at every run) against `xl_vif1_span` 2.464 -> 2.373 (-0.09): +0.15 net, +0.27 with the joining off.
**Kept:** parts 1 and 2a (`ge/xlate.c` md5 d5546b97...); `runtime/ee_io.h` and `psp/xl_psp.c` are back to xl5's (rebuilt from today's sources in xl9, boot_rt.o and xl_psp.o are byte for byte xl5's). x86 x62 (x56's objects with that xlate.c, `/home/claude/xlive/x62`, ephemeral): the eight digest windows identical to x56's (203,680 / 138,804, 132,213 / 73,514, 70,911 / 44,177, 87,489 / 58,858 lines) and every other log line the same. xl9 (xl5 with that ge_xlate.o, the same models): the kept parts take 0.57 M instructions a step off the deck tutorial (-6.4%) and 0.27 M off the indoor walk (-6.9%), about 2.4 and 1.1 ms by the instruction term alone. Indoors the model shows it (19.95 ms against 20.32); on the deck the misses ate it in this build (I-misses 19.1 -> 28.8 k, D-misses 17.1 -> 21.5 k: 45.68 ms against 45.11). The I-misses follow the code order, which these test builds take from xl5's profile (new code lands outside it); the D-misses also changed between builds with the same data structures (xl7 15.7 k, xl9 21.5 k; cause not checked). A placement of the translator's hot data (the VU memory model, owner marks, descriptors, GS registers) for the PSP's 16 KB two-way data cache is an item for the harness build [I].


### 11.13 Step 3: the live frame, checked on x86; the depth rule; the GE list (2026-10-27; task #54)
**What** (`ge/gelive.c`, `ge/gelive.h`): on a shown vblank the translator's callbacks build the frame as GE batches (`ge/gebatch.h` GbOut, `o.live`): the GS front end's path 2 / 3 primitives (the game's own GS draws, VU1 entry 0) and the batches of the VU1 calls with a converter (`ge/xconv.c`: P04+df0 as GERK_XFORM, +25f0 / +2870 as GERK_XFORMF), in the order they come. Textures come from the PSP texture path (`ge/texcache.c`) by pointer: the cache keeps a texture to the end of the next frame, so the GE can draw a frame while the next one is built; frame buffers sampled as textures are GE kind 2 (the buffer being drawn, or the other one). A lookup with the last one's TEX0, CLAMP and TEXA and no texture source changed since takes its answer (memo). Calls of the entries without a converter (P06 people, the 3740 sprites, P09 sea, stage program, radar, lines) are counted, not drawn: step 4. `gl_write` saves a frame as a replay file (GER2, textures copied in). x86: `boot/xlive_chk.c` `MGS2_XLIVE_GB=1` (frames), `2` (+ every memo answer checked against a real lookup), `3` (+ the depth ranges below); `MGS2_XLIVE_GER=<prefix>:<vb>,...` saves frames.

**Check 1: the frame builder in the game** [C] (x63: v30's x86 objects with gelive, `tools/dev/build_xlive.sh` GU=lean, `/home/claude/xlive/x63`, ephemeral; one walk, shown frames only, `MGS2_XLIVE_GB=3`; 100 vblanks a window, the translator live from the window's start): the game's state hashes equal the references in all four runs (241 / 241, 316 / 316, 327 / 327, 370 / 370 against `prof/hashes_x86_*_h2.txt`); nothing dropped; memo answers that differed 0; textures refused, not known or render targets 0; primitives left out (uncertain input, path 1) 0; state the GE draws otherwise 0; converter calls with bad input or no room 0. Per shown frame (50 a window), mean / largest:

| window | batches (transform) | data | textures | GS primitives | VU1 calls: converted, nothing drawn, other entries |
|---|---|---|---|---|---|
| deck tutorial 24,001-24,100 | 1,474 / 1,578 (1,456 / 1,561) | 995 / 1,096 KB | 243 / 255 (1,037 KB largest) | 93 / 94 | 1,456, 160, 1,158 |
| deck free play 31,501-31,600 | 269 / 337 (256 / 325) | 142 / 184 KB | 140 / 163 (635 KB) | 85 / 87 | 256, 280, 932 |
| indoor walk 32,601-32,700 | 310 / 335 (301 / 325) | 206 / 221 KB | 190 / 199 (1,162 KB) | 49 / 50 | 301, 133, 448 |
| indoor alert 36,901-37,000 | 536 / 637 (519 / 620) | 315 / 378 KB | 249 / 268 (1,687 KB) | 65 / 66 | 519, 138, 519 |

No texture was 16-bit (GE 5551) in these windows; the deck windows sample a frame buffer 33 times a frame (the depth blur's 32 strips and the trail). By converter over each window (df0 / 25f0 / 2870): calls 68,460 / 7,535 / 4,800 on the deck tutorial, batches 68,460 / 4,346 / 0 (the 2870 calls drew nothing in all four windows). Texture data per frame (1.0-1.7 MB) is more than VRAM holds beside two 8888 buffers and a 16-bit depth buffer (688 KB, §11.4): the GE reads textures from RAM in this step.

**Check 2: the frames against the reference renderer** [C] (x64: x63 with the depth split below, and `MGS2_GSCAP` captures of 11 vblanks from 24,050, 31,545, 32,645 and 36,945, `/home/claude/xlive/x64`, ephemeral). The captures force the exact walk's slow path, yet d24057, d24059, f31550, f31552 and w32651 are byte for byte x63's frames; the capture runs' state hashes equal the references to their ends (240, 315, 326, 369 lines). `tools/gsref.c` now has `GSREF_GELIVE=1`: its export made as the live renderer makes the frame (the front end's primitives and the converters' batches only, also for calls the PS2 drew nothing from, a transform batch ending the current batch, df0 without the joined index list). `tools/dev/ger_cmp.py` compares two replay files draw by draw (a through batch as its primitives, a transform batch as its data bytes, each with its batch's state; textures by content):

| frame | draws (transform / GS primitives) | equal | texture only in the live frame (PS2 drew 0 pixels) | other differences |
|---|---|---|---|---|
| deck tutorial 24,057 | 1,649 (1,555 / 94) | 1,644 | 5 | 0 |
| deck tutorial 24,059 | 1,646 (1,556 / 90) | 1,644 | 2 | 0 |
| deck free play 31,550 | 327 (240 / 87) | 313 | 14 | 0 |
| deck free play 31,552 | 326 (239 / 87) | 312 | 14 | 0 |
| indoor walk 32,651 | 250 (204 / 46) | 199 | 51 | 0 |
| indoor walk 32,653 | 254 (204 / 50) | 203 | 51 | 0 |
| indoor alert 36,951 | 602 (536 / 66) | 602 | 0 | 0 |
| indoor alert 36,953 | 623 (557 / 66) | 621 | 2 | 0 |

- **Texture only in the live frame:** gsref gives a converter's call the GE texture of the PS2's own textured primitives of that call; a call the PS2 drew no pixel from (every triangle off the screen or culled) has none there, while the live renderer looks every texture up. Those draws differ only in the texture bit, the twopass flag that follows from the texture's lowest alpha (4 of them) and the texture scale and offset of a transform batch; the GE draws nothing from them either.
- **A last-bit difference, found and fixed in the check:** the first comparison differed in the last bit of a few floats of the transform batches (proj[10], proj[14], vo[2], fog2). Not the lazy UNPACK decode (`MGS2_XLIVE_SYNC=1` gave the same, and the translator's digests of `cap_dc1`, live against gsref's `GSREF_XLDIG`, are identical): the x86 check build runs the whole process in the PSP's float mode (`MGS2_FPU_RZ=1`: round toward zero, denormal results flushed, as FCR31 0x01000001), and the converters run in the game's thread in that mode, while gsref ran round-to-nearest. gsref now has `GSREF_RZ=1` (the same MXCSR bits for the whole run); with it every float is equal. The PSP itself runs the converters in FCR31's mode, so the live x86 frame is the PSP's.

**Depth** (GE 16 bits against the PS2's Z24; §11.8's rule needs each frame's range). What these frames hold [C] (x64's `MGS2_XLIVE_GB=3` lines; a debug build of `xlive_chk.c` listing the depth writes over 1 M; the GER files measured by a script):
- Every df0 projection in the 8 frames has its near plane at Z24 16,773,120 and its far plane at 4,096: the game uses all of Z24.
- **Every GS draw that writes depth tests ALWAYS** (ZTST 1): the HUD's text sprites and boxes (Z 294,928 to 16,715,776, bottom right), the radar's lines (Z 0xFFFFFFFF, clamped to the top), the clears (Z 0) and sprites from a frame buffer, all drawn after the meshes. No depth-tested GS draw writes depth in these frames. So the range is taken from the depth-tested writers only; an ALWAYS writer outside it is clamped, which keeps its order against everything (§11.8: nearer than every write passes GEQUAL either way).
- The df0 meshes' written range by three cheap rules, against the PS2's own (gsref `GSREF_ZCENSUS`: the vertices of the depth-tested, depth-writing primitives the VU1 programs kicked, all entries):

| frame | PS2 (all entries) | V1: df0 vertices inside the clip volume | V2: df0 vertices in front of the camera | V3: corners of each df0 batch's box |
|---|---|---|---|---|
| deck tutorial 24,057 | 100..291,998 | 18,385..271,866 | 18,385..1,124,067 | 18,385..3,049,700 |
| deck free play 31,550 | 100..311,162 | 43,388..136,946 | 36,230..1,198,449 | 35,932..1,260,874 |
| indoor walk 32,651 | 200..482,426 | 102,366..403,231 | 100,774..924,261 | 100,600..924,261 |
| indoor alert 36,951 | 42,788..462,962 | 42,967..405,985 | 42,967..462,955 | 42,634..462,955 |

  V1 is below the PS2's top in every frame (triangles partly on the screen reach nearer than their visible vertices; in free play the nearest writer is not a df0 mesh at all, so the entries still without a converter must be measured too once they draw), so V1 would clamp the nearest surfaces. V2 and V3 cover it.
- **What each rule costs the picture** [C] (gsref `GSREF_ZMAP` with each frame's range, lo from the PS2's census, hi the rule's (at least the PS2's), §11.8's margin of 1/16 on each side; the 512 x 512 PS2 picture against gsref's Z24 picture of the same frame; pixels changed / over 16 levels):

| frame | PS2's exact range | V2 | V3 | fixed, the whole Z24 range (no measuring) |
|---|---|---|---|---|
| deck tutorial 24,057 | 3,161 / 5 | 3,216 / 17 | 3,342 / 56 | 4,214 / 266 |
| deck tutorial 24,059 | 2,405 / 2 | 2,492 / 6 | 2,707 / 22 | 3,944 / 299 |
| deck free play 31,550 | 1,053 / 11 | 1,105 / 18 | 1,113 / 18 | 1,870 / 271 |
| deck free play 31,552 | 737 / 6 | 843 / 21 | 868 / 22 | 1,964 / 262 |
| indoor walk 32,651 | 4 / 4 | 8 / 7 | 8 / 7 | 151 / 88 |
| indoor walk 32,653 | 4 / 4 | 9 / 8 | 9 / 8 | 122 / 66 |
| indoor alert 36,951 | 3 / 2 | 3 / 2 | 3 / 2 | 118 / 44 |
| indoor alert 36,953 | 2 / 0 | 2 / 0 | 2 / 0 | 121 / 57 |

  (Pixels over 64 levels: 0 for every measured rule; 4-67 for the fixed scale.) **Rule taken: V2.** Each depth-tested, depth-writing transform batch gives the range of its vertices' view depth w (one dot product per vertex, w > 0 only), mapped to Z24 by its projection and viewport and clamped to its near and far planes; depth-tested GS draws that write depth give their vertices' Z24; the frame's GE depth = clamp((Z24 - lo') x 65535 / (hi' - lo'), 0, 65535) with §11.8's margins, set by the list builder at the frame's end (through vertices' z and the viewport's z terms), so it is this frame's own range. A frame with no depth-tested writer keeps the last range. Its cost on the PSP is about 32 k df0 vertices a deck frame (5-14 k indoors) x one dot product [I], to be measured with the list builder; V3 (6 integer min / max per vertex and 8 corners per batch) is the fallback, at 20-51 more pixels over 16 levels on the deck.

**Check 3: from boot, as the PSP runs it** [C] (x64, `MGS2_XLIVE=0:<end>` through `/home/claude/xlive/runb.sh`: the translator live from vb 0 on the deck tutorial to 24,062, the free route to 31,557 and the indoor route to 36,957, one walk, the texture path (`MGS2_XLIVE_TK`) and the map's texel check on): the game's state equal on 924 of 924 hash lines (240 / 315 / 369); texels or palette entries the map could not give 0. The texture path over each run: 1.73 M / 3.21 M / 5.08 M uses, 1,221 / 1,287 / 1,730 texture conversions, 7,953 / 7,928 / 10,805 KB converted and all of it kept (the check has no budget: evicted 0); the heaviest frame used 1,906 / 1,906 / 2,355 KB. The frames at the windows' starts equal the window runs' frames except one font texture uploaded before the window (the window runs could not see it).

**The GE list builder** (`ge/gelist.c`, `ge/gelist.h`; 8,464 bytes of PSP code at -O2): a frame of batches (gebatch's arrays or a replay file's) to the GE's raw command words by the test player's reference mode (`psp/ge_replay` MODES[0]: df0 and the effects with hardware transform and fog, one PRIM per df0 strip, the effects' strip runs as strips and the rest of each call as one indexed TRIANGLES list (class 3 whole, class 4 and up as fans of the CPU's polygon), the depth blur in 16-row tiles from the top left, the trail in 16-pixel columns, a second pass for AFAIL FB_ONLY with Z write). Why: sceGu cost the deck frame's list 23.17 ms of CPU (`prof/hw_ge_v6`).
- The list keeps the last state it set and sends only changes; BASE goes out only when it changes; VADDR only when a strip does not follow the last PRIM's vertices, since the GE moves its vertex address on after a non-indexed PRIM (6,132 of the deck frame's 8,243 PRIMs rely on it).
- The frame's set-up (buffers, offset, viewport, scissor, depth range, the clear) is in the list; the clear's and the sprite cuts' vertices and the effects' index lists go in the list's own memory (aux). The list ends with RET (for sceGuCallList) or FINISH and END.
- A digest (FNV-1a 64 of the words, each address word hashed as which array and which offset, BASE left out) is the same on x86 and on the PSP for the same frame.
- Checks [C]: `tools/dev/gx_dump.c` (x86) walks each list as the GE reads it (BASE, VADDR, IADDR, VTYPE, the vertex address moving on) and checks that every PRIM reads inside its array: all PRIMs of the 8 frames do. Sizes (raw depth frames): deck tutorial 52,520 words (210 KB) + 33 KB aux, 8,242 PRIMs; free play 8,430 words + 28 KB; indoor walk 6,793 words; alert 17,398 words. PPSSPP (`psp/ge_replay` built with `-DSMOKE -DONLY_REF -DGELIST`, `XOBJS=gelist.o`): on x64's frames d24057, f31550, w32651, a36951 the pictures equal the player's own list's (sceGu, mode 1), and the digest the PSP code prints equals gx_dump's on each.

**Depth, as built** (the V2 rule above; `MGS2_XLIVE_GB_ZK=0`, gelive's `zraw`):
- `ge/xconv.c` (XcZr): each df0 call that tests depth (ZTST GEQUAL or GREATER) and writes it gives the view depth w of its vertices (one dot product a vertex; w > 0 only), kept as a w range per projection (pa, pb and the viewport's z terms). At the frame's end each range, clamped to its near and far planes, gives Z24 = vo2 - vs2 (pa - pb / w) (Z24 rises with w for each projection), and the union is the meshes' range. df0 writers with ZTST ALWAYS are counted and left out.
- `ge/gelive.c`: through vertices keep their Z24 (a float holds it exactly), transform batches carry the PS2's viewport z terms; the GS draws that test and write depth add their vertices' Z24 (Z 0 and the format's top left out). The map (`gx_depth_map`: margins of 1/16, lo' at least 0) is applied at the frame's end by the list builder (through z rewritten in place, the viewport's z terms per batch). A frame with no depth-tested writer keeps the last map. The effects' class 3 window (triangles the GE draws whole) is the last frame's range, without margins (the first frame has none: all class 4).
- Float only: psp-gcc's double is soft float rounding to nearest while the x86 check runs the PSP's round-toward-zero, so `xc_xform` (viewport, texture scale) and `gb_vert` (through x and y: x 15/256 and x 17/512, exact) now compute in float, and gsref's live export uses the same code. After the change the old mode's frames (Z24 x 0.2) still compare equal to gsref's draw for draw, with the counts of Check 2 (x66 against gsref rebuilt, `GSREF_RZ=1`).
- **Check 4** [C] (x66: x64 with the above, `/home/claude/xlive/x66`, ephemeral; Check 2's four windows, raw and old mode): each frame's range equals the measured V2 to within 1 Z24 unit (float against the script's double): deck 18,386..1,124,066 (measured 18,385..1,124,067), free play 36,230..1,198,449 (equal), indoor walk 100,774..924,261 (equal), alert 42,968..462,956 (42,967..462,955). In all 22 frames: no depth-tested GS writer and no ZTST ALWAYS df0 writer; no primitive with a 16-bit depth buffer; nothing outside the map by the x86 count of what the GE would clamp or drop (class 3 effect triangles 0, df0 triangles of depth writers 0, of other df0 calls 0), so df0 keeps CLIP_PLANES off. Game state and packet hashes and the translator's digests equal x64's in all 8 runs.
- **Pictures** [C] (PPSSPP software GE, 40 frames of the same file so the trail settles, the 3 x 3 rule of §11.8 against gsref's PS2 frame buffer; mean levels, over 16, over 32):

| frame | raw depth against Z24 x 0.2 | raw depth against the PS2 | Z24 x 0.2 against the PS2 |
|---|---|---|---|
| deck tutorial 24,057 | 1,129 pixels differ, 0.01 | 13.00, 33.3%, 11.4% | 13.00, 33.3%, 11.4% |
| deck free play 31,550 | 4,633 pixels, 0.04 | 13.03, 28.8%, 2.8% | 13.03, 28.8%, 2.8% |
| indoor walk 32,651 | 40,569 pixels, 10.82 | 2.07, 0.8%, 0.3% | 11.15, 22.9%, 13.2% |
| indoor alert 36,951 | 24,853 pixels, 12.86 | 3.62, 2.6%, 1.2% | 16.04, 19.0%, 15.1% |

  Indoors the fixed scale clamps every surface nearer than Z24 327,675 (in the alert frame the near pillar is lost); the frame's own range draws it as the PS2 does. On the deck the depth rule changes little and both are far from the PS2 for another reason: the live frame has only the converted entries. Snake (P06), the rain, the radar's lines (the stage program) and the bright windows on the upper wall are missing; v5's replay of the same view (gsref's export with every entry) drew them. Those entries are step 4.

**Memory for the harness** [C for the figures, I for the plan]: heap headroom 7,104 KB at the start of v30's runs, 4,288 KB at their end (v30me6b run 1); 2 MB of the volatile block free (§11.4). Per deck frame: about 1.1 MB of batches, vertices and data, a list of 210 KB plus aux. Textures: the heaviest frame 1.9 MB (deck) to 2.4 MB (indoors) from boot, and the cache converts 8-11 MB over a route, so the PSP needs a texture budget with eviction of at least the heaviest frame. Frame data single-buffered with a GE sync before the next shown frame is built (the GE has the steps between two shown frames to draw), double-buffered only if the sync is measured to cost.

**Unconverted entries in these frames** (the translator's "called first" lines; names as `re/vu1_programs.md` §0): P04+780 0x198b60, +a70 0x198e58, +1088 0x199478, +2870 0x19ac78, +2b60 0x19af68, +3740 0x19bb50, +3a38 0x19be50; P06+0 0x19fe08, +358 0x1a0160; P09 0x1a8148; P01 0x194478 / 0x1944c0; the stage program 0x201f78 (radar and map lines). Converted: P04+df0 0x1991d8, +25f0 0x19a9f0.

**Next:** 3e, the harness: the translator, frame builder and list builder on shown steps with raw depth, the texture budget, the GE sync, the display (two 8888 buffers, the 16-bit depth buffer), game hashes unchanged, emulator checks, then a PSP run (after the v30me6b runs are read). Then step 4, the converters by pixel share.

**3e as designed (2026-10-27, before the night pause; not coded yet; task #56).** Sizes from the x86 frames: the largest live frame 1,069 KB of batches, vertices and data (deck tutorial, 1,574 batches), the deck list 52,520 words + 33 KB aux, textures 1.9-2.4 MB in the heaviest frames, at most 266 textures a frame; VRAM: FB0 0x000000, FB1 0x088000, the 16-bit depth buffer 0x110000, free from 0x154000 (704 KB).
- psp/xl_psp.c under `#ifdef MGS2_GL`: tk_new(map, MGS2_GL_TEXKB, default 2,560 KB); gl_init(&glf, tk, map, 0 = raw depth, max_tex 1,024); from the volatile block's last 2 MB (main_psp.c host_vol_tail): the arena 1,696 KB (gb_arena nb 4,096, nv 8,192, data the rest, ~1,344 KB), the list 72 K words (288 KB) and aux 64 KB; gx_init with ret = 1, dig_on in a window (MGS2_GL_DIG); GxHost addr = the pointer itself, fb {0x000000, 0x088000}, zb 0x110000; the harness's text screen moves to a third buffer at 0x154000 (pspDebugScreenInitEx at 0x44000000 + 0x154000, 8888), shown again by finish() before the summary.
- GE set-up once, as psp/ge_replay: sceGuInit, sceGuStart(GU_DIRECT), DrawBuffer(8888, FB0, 512), DispBuffer(480, 272, FB1, 512), DepthBuffer(ZB, 512), Offset(2048 - 240, 2048 - 136), Viewport(2048, 2048, 480, 272), DepthRange(65535, 0), Scissor(0, 0, 480, 272), Enable(SCISSOR_TEST), Disable(CULL_FACE), Disable(CLIP_PLANES), Finish, Sync(0, 0); no sceGuDisplay; FB0 and FB1 cleared once.
- xl_psp_vblank(full, reduced): a frame being built ends there (HOST_GL: gl_end; a GxFrame from glf.o; gx_depth_map(map_lo, map_hi, 1); gx_frame into buffer (frame count & 1); sceKernelDcacheWritebackAll; sceGuStart / sceGuCallList(the list) / sceGuFinish; without OPT_GL_ASYNC the CPU waits for the GE (HOST_GW)); a finished GE frame (sceGeDrawSync(1) == 0) is shown with sceDisplaySetFrameBuf(0x44000000 + fb, 512, 8888, NEXTFRAME); tk_frame every vblank; after a step with a full call (and OPT_XL, OPT_GL, the translator on) a frame begins (wait for the GE if it is still drawing (HOST_GW), gl_begin). on_call / on_prim go to gl_call / gl_prim while a frame is built. The texture cache keeps what this and the last frame used, so the CPU must wait for the GE before the next gl_begin (it does).
- ge/texcache.c: texels and palettes with memalign(64) on the PSP (TK_ALLOC under __psp__; x86 as it is). runtime/host_time.h: HOST_GL 4, HOST_GW 5, OPT_GL 2048u, OPT_GL_ASYNC 4096u. psp/main_psp.c (MGS2_GL): NREG 6, windows and steps.csv columns gl_us and gw_us, cfg names "picture off" (~2048u) and "GE waited" (~4096u), MGS2_GL_SHOT (BMP from VRAM), the frames' digests in a window and a digest of them. psp/Makefile: XLIBS (-lpspgu) at the start of LIBS; MAINX and the SDK include on xl_psp.o. Script psp/gl1.sh from psp/xl5.sh: XOBJS adds ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o; MAINX="-DMGS2_XL -DMGS2_GL"; XLIBS=-lpspgu; the ME off.
- Checks before a PSP build: the game's hashes equal to the references; each frame's digest equal to x86's (boot/xlive_chk.c MGS2_XLIVE_GX: gx_frame with the PSP's arena sizes and max_tex, frames from the first DMA on, the buffer by frame count); PPSSPP pictures against the player's for indoor frames. The PSP build goes to Ffm only after the ME runs are read and with a description first.

**3e as built (2026-10-27; task #56; `psp/xl_psp.c` and `psp/main_psp.c` with `-DMGS2_GL`, script `psp/gl1.sh`, build MGS2T_gl1).** As designed above, with these changes:
- **One block for the list and its aux** (`ge/gelist.c`): the list's words grow up from the block's start and aux (the clear's quad, the sprite cuts' vertices, the effects' index lists) grows down from its top, 16-aligned with the gaps zeroed (`aux_get`, `alow`, `shared`); `gl_digest` hashes aux from `alow`. The 64 KB aux of the design overflowed: the x86 check from boot (x69) has frames with 234,272 bytes of aux, and the largest words 67,910 (deck) / 69,454 (indoors); together at most 300,920 / 309,528 bytes, never both maxima in one frame. The block is 352 KB (`GL_PSP_LIST`), the arena 1,696 KB (nb 3,072, nv 6,144, data ~1,432 KB; the largest frame used 2,043 batches, 2,608 vertices and 1,240 KB of data). x69 both routes from boot: 0 lists that did not fit, 0 batches left out.
- **VU memory synced by UNPACK** (`ge/xlate.c` `vm_all`, `xl_vm_mode`; `OPT_XL_VMUD` 8192 in `runtime/host_time.h`): before each converter call the picture needs the translator's pending VU memory decoded; the old sync scanned all 1,024 quadwords for an owner, the new one decodes only the pending UNPACKs' quadwords that are still theirs. The same VU memory either way: x70 (the new sync) gives x69's frame digests on both routes from boot (all aa5f8dd37134bb82 deck, 96612dfb5b399d51 indoors); x86 translator time 0.757 -> 0.632 ms a vblank (deck), 0.797 -> 0.704 (indoors). The harness can switch back per window (cfg 15, "VU sync by scan").
- **Sprite cuts counted exactly** (`ge/gelist.c` `pieces` steps as the cutting loop does; `cut_corners` shared), so a cut's aux is taken once with its true size.
- **For the first PSP runs:** every wait for the GE gives up after 2 s (`gl_sync`: sceGeDrawSync(1) polled with 100 us sleeps; a stop prints "picture: STOPPED", puts the harness's text on the screen and makes no more frames; the run goes on); the crash catcher and battery log of the ME builds (`psp/v26me4_exc.c` / `.S` with `-DMGS2_GL_EXC`: the record is drawn on the frame buffer on the display, `xl_psp_gl_screen`; `psp/v26me_bat.c` with ME_DIAG3/4/5, lines every 5 s) joined by `psp/gl1_glue.c` and the link's wraps (host_vb_begin, mgs2_exit, sceKernelExitGame, mgs2_main) instead of me_mix.c; the ME line's `v26me4_exc.o` built from the edited source is byte for byte v30me6c's. main_psp.c logs the program break's highest point and the lowest headroom at the end.
- **Results** (psp_harness.md "Build gl1"): in PPSSPP every frame of the digest windows equals x86's on the deck (95 / 95), indoors (95 / 95) and with the delivery's mgs2.cfg (50 / 50), the state hashes stay exact and no list overflows. The PSP heap's low point (the emulator's less 5,248 KB: about 276 KB with the 2,560 KB pool) set the delivery's pool to 2,048 KB (about 788 KB left [I]; every texture still fits, the heaviest frame 1,716 of 1,792 KB). Emulated cost indoors: the picture 14.97 ms a step, picture off -13.58, the old VU sync +5.57 (model: 78.54 against 68.64 ms a step in vb 36,901-36,931). Single heavy steps where a frame needs many new textures (vb 36,908: 54 textures and palettes, 375 KB; 0.6 s emulated, about 0.8 s modelled, 721 of its 822 model ms in the per-texel map path): task #59, conversion by whole GS blocks. Delivered as MGS2T_gl1 2026-10-27 12:23 (ME off).

**Texture conversion a GS block at a time (2026-10-27; task #59; `ge/texconv.c` `tc_texture_blk`, `ge/gsup2.c` `gu_block`, `ge/texcache.c` `tk_blk`; build gl2).** gl1 converted a texture texel by texel: `tc_texture_cb` -> texcache's `fe` -> `gu_texel` -> `gu_index` -> `gu_word` (settle, the block's three lane-group states, `words3`, `entry`), so every texel decoded the whole word it sits in (4 texels in T8, 8 in T4) and looked its block up again. In the heavy step at vb 36,908 (54 new textures and palettes, 375 KB) that path was 721 of the 822 model ms over its two steps (gl1 model h36908, ms a step: entry 130, gu_index 55, words3 54, gu_word 52, tc_texture_cb 28, gu_texel 19, fe 14, ram 10).
- **The block reader:** `tc_texture_blk` walks the GS blocks the region touches (16 x 16 texels in T8, 32 x 16 in T4, 8 x 8 in T8H / T4HL / T4HH), gets each block's 64 words once (`gu_block`) and places its texels: T8 the block's byte `ct8`, T4 its nibble `ct4`, the CT32-layout formats the top bits of word `ct32` (little-endian hosts: x86 and the PSP); the palette entries used are marked in a 256-byte table; the stored edge (one column and row past the region) is copied from what was written. It hands the texture back to the texel path (which counts what the map cannot give) when a texel is not known, for direct colour, or for a region past 2,048. Switchable: `OPT_TK_BLK` (16384, runtime/host_time.h; on by default), x86 `MGS2_XLIVE_TKBLK` (0 texel, 1 block, 2 both, compared).
- **`gu_block(m, bn, need, v, known)`:** the 64 words as 64 `gu_word` calls give them, in the bits `need` (texcache: T8H 0xff000000, T4HL 0x0f000000, T4HH 0xf0000000, else all): settle and the states once, then each lane group (bits 0-23, 24-27, 28-31; one pass when all three have the same state) from its own state. A group whose state's top record is a CT32-layout upload (CT32, CT24, T8H, T4HL, T4HH) that wrote the whole block and has all its bytes is read 8 pixel rows at a time (`blk_rec`; `rec_bytes` is linear for the own copy, the sent copy and the guest RAM callbacks; `entry`'s values); any other group goes word by word through `entry` as before. gsup.c and gudual.c give it as 64 `gu_word` calls.
- **What the blocks hold [C]:** in the first version only whole-block CT32-layout states took the fast path, 14,614 of 82,222 deck blocks; 67,608 had a different state per lane group: Z24 records in bits 0-23 with 4-bit textures in the top byte (T4HL uploads in bits 24-27; T4HH uploads or an older CT32 upload in bits 28-31): 39,864 Z24 / T4HL / CT32, 25,641 Z24 / T4HL / T4HH, 2,079 with a T4HH list node. No transfer writes Z24 on the deck (`MGS2_XLIVE_TRX=2` to vb 25,200 on x73: host uploads by dpsm 0x00 CT32 332,322, 0x02 CT16 6,270, 0x2c T4HH 4,799, 0x14 T4 384, none in Z24 or T4HL; local copies CT32 -> CT32, the frame-buffer copies, and one T4 -> T4HL, the font's 512 x 256 at 0x3000 at vb 33), so the Z24 records are drawn areas (`gu_drawn` puts a Z24 draw in bits 0-23) [C]: the depth buffer at block 0x3000, whose top byte holds the font page's two 4-bit halves (T4HL from that copy, T4HH from the text image uploads: §11.10 step 6, "The font"); where no T4HH upload reached, bits 28-31 still belong to an older CT32 upload. Per lane group: 80,182 of 82,222 blocks (deck) and 76,615 of 78,360 (indoors) are read whole from one upload.
- **Exact [C]:** x86 check builds x71 (the first version), x72 (lean texel loops, whole CT32 blocks) and x73 (per lane group) with `MGS2_XLIVE_TKBLK=2`, which converts every texture both ways and compares the bytes and the TcTex: deck 901 of 901 conversions, indoors 1,253 of 1,253, indoors with the delivery's 2,048 KB pool 1,595 of 1,595 equal, none handed back; every frame digest and state hash as before (deck 38df919103cf5808, indoors 7683b14b824c9b77, vb 31,801-31,899 103ac5bb4b1008d1 with 2,560 and 2,048 KB). PSP build gl2 in PPSSPP (`psp/gl2.sh`): deck 95 / 95 and indoors 95 / 95 digests equal x86's, state hashes 370 / 370 and 434 / 434 against the hashref (63 / 63 and 127 / 127 full lines against `prof/hashes_x86_*_h2.txt`), the same conversion and block counts as x86 (901 and 1,253 by blocks, 0 handed back; blocks 82,222 / 80,182 and 78,360 / 76,615).
- **Cost:** model of the heavy step (vb 36,907-36,908, PspProf, 2 steps): **88.73 ms a step against gl1's 410.99** (-322; tc_texture_blk 21.57, gu_block 9.92, entry 4.06, words3 1.64: the conversion about 38 ms a step against about 360). PPSSPP's clock, indoor route: the step at vb 36,908 168.1 ms against 645.3 (picture 100.1 against 577.1), vb 32,334 97.4 against 308.4; the other slow steps (vb 36,732, 36,776, 36,938, 37,700: conversions during the walk) 130-137 ms against 374-378; the timed range's mean 43.87 against 44.36 ms (all four settings). Next in this path: `tc_texture_blk`'s per-texel loop (4.96 M instructions a step here) if the heavy steps still matter after gl1's PSP numbers.

### 11.14 Step 4: the lit meshes (P06: Snake, the crew, the guards) on the GE (2026-10-27; task #63; build gl3)

- **What they are** (`re/vu1_programs.md` §3.4, 3.8, 3.14): P06's four entries, by the guest address their micro code came from: +0 0x19fe08 (lit), +358 0x1a0160 (two-matrix skin), +738 0x1a0540 (+0 with a per-vertex colour factor: the guards), +ab0 0x1a08c0 (both). Up to three passes a call, each with its own texture, clamp and blend from its own A+D packet (T+0xe8 / 0xf0 / 0xf8): pass 1 the lit colour; pass 2 (header y 0x10) the same positions with UV2; pass 3 (0x200 the normal projection, 0x400 the light ramp) its own float UVs. Up to gl2m these calls were not drawn (§11.13, "Unconverted entries").
- **The converter** (`ge/vu1conv.c` `vc_p06`): each vertex as the micro program computes it, in its order of operations: three directional lights and ambient, the MINI clamp, the guards' factor, FTOI0; the skin's two matrices weighted by the position's w; the UVs of passes 2 and 3; the constant fog F and ADC from header z. When no constant reaches 2^20 (every call seen) no result can reach the VU's clamp, so the vertices are computed without the per-operation flush (the fast form; the same bits). Vertex types by what the call holds: `VcVert` (16-bit UV and position), `VcVertE` (float UV: pass 3), `VcVertS` (float position: skinned), `VcVertF`. Pass 2 gets vertices of its own when UV2 - UV1 is not one constant (`vc_p06_uv2`, `VcLit.uv2` 2; §3.14 "Not everywhere").
- **Exact [C]** (`tools/dev/p06chk.c` on gsref's P06 dumps, both roundings): the 933,883 strip vertices of 21,454 calls in five capture windows (deck tutorial, deck free play, two indoor windows, the guards) and the opening's 12,012 (143 calls, 121 with pass 2's own vertices): position, flags with ADC, RGBA, ST and Q of every pass equal the PS2's.
- **On the GE** (`ge/xconv.c` `xc_lit`, `xc_pass`; `ge/gelive.c` `gl_pass`; `ge/gelist.c` `lit_draw`; replay kind GERK_LIT): at the call, its transform (df0's form; world rows x 32768 for 16-bit positions), a GerLit record per pass, the vertices and the strips go in the frame; passes 2 and 3 get their texture and state when the translator reaches their GS packets (`xl_on_pass`), so each pass's texture is looked up as the GS then holds it. Each pass is drawn with the call's world matrix and constant fog. Its vertices feed the frame's depth range as df0's do (`xc_zr_lit`).
- **Live frames [C]:** on x86 the live frames equal gsref's PS2 reference draw for draw on ten frames of three routes (7,033 P06 passes, 0 differ) and on the opening's vb 16,005 / 16,007 (39 / 39 passes; x75 against gsref8).
- **Volumes [C]** (x75 from boot, the x86 GX runs): the deck route to vb 25,200, 265,586 calls in 11,766 frames (+0 188,328, +358 77,258; 6.58 M vertices; passes 2 and 3 drawn 161,494 each, pass 2 with its own vertices 22,759); the indoor route to 37,800, 1,426,658 calls in 17,664 frames (+0 918,134, +358 398,877, +738 77,181, +ab0 32,466; 34.5 M vertices; passes 2 and 3 690,147 each, own vertices 36,619).
- **Texture memory:** with the guards textured the heaviest indoor frame needs 2,264 KB (x75, vb 36,9xx, a 2,688 KB pool), against gl2m's 2,048 KB pool (1,792 KB budget). gl3 makes room two ways: the host stack pool shrinks from 1,152 to 512 KB (`psp/gl3.sh`: other guest threads 48 KB, IOP threads 24 KB; 74 PSP logs used at most 10,456 and 5,376 bytes), giving the texture pool 2,688 KB; and room pieces wholly off the screen are left out before their textures are made (§11.15).

### 11.15 The off-scissor cut: room pieces wholly off the screen left out (2026-10-27; task #67; build gl3)

- **Why [C]:** in the heaviest indoor frame (vb 36,931, capture hc1, gsref8 GELIVE) 380 of the 589 df0 calls lie wholly off the screen (the PS2 wrote no pixel from them), and 585 KB of the frame's textures serve only them (gsref's sizes: 1,608 of 2,183 KB kept). They also cost the CPU (vertices, strips, texture lookups) and the GE (it clips them).
- **The rule** (`ge/xconv.c` `xc_off`, `xc_df0_off`, called from `xc_call` before the texture lookup): the 8 corners of the box of the call's 16-bit positions (as the GE reads them, s / 32768) go through the call's GE world matrix. Each scissor side is a half-space of the GE's view space (x, y, w): the screen X >= X0 for a point in front (w > 0) is vs0 x + (vo0 - X0) w >= 0, an affine function of the object position, so it is below 0 on the whole box when it is at all 8 corners; every vertex lies in the box, and so does every point of its triangles and of the near plane's clipped polygons. A call is left out when all 8 corners lie beyond one side by 2 GE pixels or more (`XC_CUT_PX`); a NaN never counts as beyond. Float throughout, round toward zero on both the PSP and the x86 check (so both decide alike; a call decided otherwise near the margin draws no pixel either way).
- **The depth range:** a call left out still gives the frame's depth range (§11.13's V2) what xc_df0 gives it (`xc_df0_zr`: a depth-testing, depth-writing call with a triangle to draw, its vertices' w), so the frame's map and the next frame's class 3 window for the effects are the same with the cut on or off. The first version (x76) left them out of the range too: the map then differed in 3,931 of 11,766 deck frames and 8,340 of 17,664 indoor frames (vb 8,760: 4,250..719,312 against 4,250..16,773,120), and 1,147 deck effect triangles moved between classes 3 and 4: a picture change, caught by the x86 runs before any PSP build. With `xc_df0_zr` (x77) the depth lines (range and map) equal the cut-off run's on every frame: deck 11,766 of 11,766 (x77 with `MGS2_XLIVE_CUT=0`), indoors 17,664 of 17,664 (x75), and so do the effect class counts.
  - An idea, not taken: a range of only what is drawn gives finer 16-bit depth, but the effects (not in the range) would have to be counted too, and it changes the picture: Ffm's choice, with pictures and numbers, if it is ever worth it.
- **Exact [C]:** gsref9 (`GSREF_XLCUT=1`) on the 8 captures (the deck tutorial at vb 24,050, free play 31,545, the indoor walk 32,645, the guards 36,901, the heaviest frame 36,925, the alert 36,945, and with no df0 calls the opening 16,000 and the indoor route at 27,000): 8,357 calls left out (in that order 1,784, 665, 456, 1,613, 2,110, 1,729, 0 and 0: xlive/cut1/log_cap_*.txt), the PS2 wrote 0 pixels from every one of them. GELIVE exports with and without the cut at 4 frames (deck tutorial 24,057, indoor walk 32,651, alert 36,951, the heaviest 36,931): the cut's draws are the full list's with 1,107 df0 draws out (356, 110, 261, 380), every other draw equal and in order (a sequence alignment of ger_cmp.py's draws).
- **From boot on x86** (x77, `tools/dev/gl1_x86.sh`, 2,688 KB pool) [C]: the deck route's df0 calls left out 340,524 of 653,468 (52%; 6.44 M vertices); texture lookups 921,903 against 1,217,849 with the cut off, conversions 952 against 996 (7,226 against 7,409 KB), the heaviest frame 1,346 against 1,438 KB. The indoor route: 637,942 of 1,732,548 (37%; 12.7 M vertices); lookups 3.60 M against 4.16 M, conversions 1,326 against 1,701 (9,557 against 10,972 KB), evicted 2,058 against 2,826, the heaviest frame 1,998 against 2,264 KB (2,352 KB budget). No texture refused for room either way; the game's hashes unchanged (deck vb 25,200 ee 21f7508b9ac03c20, indoors 37,800 87acb392f4bf3e07, cut on and off); with the cut off the frame digests equal x75's (deck 95 / 95).
- Switchable: `OPT_GL_CUT` (32768, `runtime/host_time.h`; on by default), x86 `MGS2_XLIVE_CUT=0`, gsref `GSREF_XLCUT=1`; counters in the converters line ("df0 calls left out off the scissor") on x86 and the PSP.
- Not covered yet: the effects (25f0 / 2870) and the P06 calls (their float or skinned positions would need their own box).

### 11.16 Task #70: the P06 loop and the unpack by kinds, both exact (2026-10-27; build gl4, delivered 21:57 with crash catcher 3: psp_harness.md "Build gl4")

- **Why [C, PspProf at vb 37,795, the hw run's settings]:** gl3's heaviest indoor step models 217.1 ms against gl2m's 113.0. The three biggest parts: ud_vec 38.2 ms (the lazy unpack's one decoder for every format, 6.86 M instructions; P06's vertices now go through it), p6_vertex 27.4 (vc_p06's per-vertex function, 5.99 M instructions) and vc_p06 14.5.
- **The unpack by kinds** (`ge/xlate.c` `ud_kind`, `ud_dec`, `ud_run`; gl3's decoder stays as `ud_any` for every other format): each UNPACK descriptor gets a kind when it is marked: 1 a straight copy (V4-32), 2 and 3 V4-16 without and with the ROW add, 4 V2-16 with the mask row. vm_all's loop runs once per kind with that kind's decoder inlined. The decoded words still go to the host's own VU memory copy, never guest memory.
- **The P06 loop** (`ge/vu1conv.c` `p6_loop` and `vc_p06`; gl3's converter stays as `p06_any`): a call goes through a loop made for its entry (4 kinds), its number of lit lights (1 to 3) and its passes (2 and 3, or neither) when its constants give the fast form, it has a lit light, no lit colour is below zero, it has no light ramp (pass 3 kind 2) and its vertices lie in one run of VU memory. That makes 24 loops, with the constants in locals and every product and sum computed as p6_vertex's fast form computes it, in the same order. What the loop takes for granted (16-bit positions and UVs, the UV z, the guards' factor below 2^22 in size) is checked as it goes, and a failed check sends the call back to p06_any, whose result then stands. vc_xform now writes its fields directly (no memset, no index loops).
- **Exact [C]:**
  - p06chk with the new converter on gsref's P06 dumps of the five capture windows: 933,883 strip vertices of 21,454 calls, 0 wrong, in both roundings (the RN dumps, and the RZ dumps with -rz). All 21,454 calls take the new loop (a counting copy: p06_any 0 calls).
  - Differential fuzz against gl3's code (scratch p06opt/src): vc_p06 on random VU memories with edge values, 100,000 calls in RN and in RZ, 0 differ (the edge values send 81,505 to p06_any; 11,328 refused alike). vc_df0 and vc_fpos with the new vc_xform: 100,000 each way, 0 differ. The unpack (scratch xlud/fz.c): random descriptors of all 13 formats, random reads and a flush, 100,000 iterations, 0 differing (329 M vectors: 178 M by ud_any, 151 M by kinds 1 to 4).
  - x86 from boot (`tools/dev/gl1_x86.sh`, 2,688 KB pool; x78 = the P06 loop, x79 = both): the "picture:" lines and the game's hashes equal x77's on the deck window (95 frames, ee 21f7508b9ac03c20), the indoor window (95) and vb 31,801-31,899 (50; ee 87acb392f4bf3e07).
  - PPSSPP (gl4 d, `psp/gl3.sh` with GL3=/home/claude/gl4): the frame digests equal x79's on all three windows and gl3's emulator shots are byte for byte the same (4 deck, 6 hw, 5 indoor). Game hashes: the deck route 370 / 370; the indoor route 434 / 434 in a run without the profiled step, and in one profiled at vb 37,695. Profiled at vb 37,795, the hash at vb 37,800 differs (433 / 434). The EE RAM there against the exact run's (a forced dump, ramdiff) differs in 1,753 words in 82 ranges, every one a float's last bit. That is PPSSPP's interpreter, which runs only the profiled step and is not exact (gl3.sh's note); the steps outside it are exact. Why the interpreter's rounding differs in gl4's step and not gl3's is not pinned down [I: an interpreter path that leaves the host rounding at nearest].
- **Cost [C, the model from PspProf, one shown step]:**
  - the deck step (vb 25,195): 137.66 -> 85.71 ms (20.46 -> 9.77 M instructions; I-misses 170.2 -> 150.0 k);
  - the heaviest indoor step with the hw settings (vb 37,795): 217.12 -> 162.47 ms (31.51 -> 17.84 M instructions; I-misses 286.6 -> 301.0 k; D-misses 116.0 -> 115.1 k); with the in settings 228.69 -> 159.93.
- **What is left in that indoor step (162.47 ms) [C]:** I-misses now cost more than the instructions: 301 k misses are 63 ms of the model (70 cycles each), and the 17.84 M instructions 75 ms. By function: vm_all 21.7 ms (now holding the unpack, with its 35.9 k D-misses), vc_p06 16.1, xc_call 12.1 (27.9 k I-misses), gu_clut_id 7.8, vc_xform 7.4 (24.7 k I-misses against 4.4 k in gl3: it no longer stays in the cache between calls), the game's overlay code 6.7, tk_get 6.6, gu_area_id 6.0, block_id 5.8, state_id 4.7, vc_df0 4.1.
- **Next levers (ideas, not measured):**
  - the I-misses: each call walks translator, converter, texture lookup and GE emission code larger than the 16 KB I-cache. Grouping the pure maths of a frame's calls by kind, while the texture lookups and GE emission stay in the GS's order, could keep each part hot (the texture map depends on the uploads' order, §11.12's generations);
  - vm_all's D-misses;
  - the texture-map functions.

### 11.17 Task #68: the codec screen (2026-10-28)

- **What a codec frame holds [C, gsref on the codec capture c17000, vb 17,001, frame buffer 0x80]:**
  - the faces are 3D meshes from VU1 program 7 (ELF RAM 0x1a31e0, 7 pieces, 0x36e0 bytes; the `.DVP.overlay..0x0.458739.257.0` group): 309 calls a frame, 9,140 tristrip primitives, gouraud and fog, three blend passes, ATST 5 / AREF 0x40. It has no rule and no converter, so the live picture draws no face. The same program draws the opening cutscene (1.78 M calls in d00t) and some deck objects;
  - P06 (0x19fe08) runs 13 calls a frame but they cover only ~3,700 pixels;
  - the 2D frame of the codec (borders, frequency, buttons) comes through path 2 and 3 sprites and lines, which the GS front end already draws; the scan lines are lines with (Cd - 0) * FIX + 0, FIX 0x5f;
  - the subtitle is a T4HH image in the text page at 0x3000 (TEX0 1024 x 256) read through REGION_CLAMP u 0..512 (513 texels), v 0..120;
  - CT24 at 0x2000 is read as direct colour by the faint large face overlays (FIX 0x18) and by a full-screen glow (1024 x 1024, uv 112..143, (Cd - 0) * Ad + Cs). Both are refused today (`gl_tex` takes CT32/CT24 only as frame buffers at 0 or 0x1000), which likely leaves the background teal instead of near black [I].
- **The grey block was the subtitle [C]:** `geomx` refused the 513-texel region (tw = p2(513) = 1024 > the GE's 512), so the subtitle's sprites drew untextured.
- **The fix (`ge/texconv.c` `geomx`, md5 18c8a061):** a REGION_CLAMP side of exactly 513 is stored as 512 texels with no edge column; the GE's own clamp gives texel u0 + 512 as u0 + 511. On gsref's texture-use extents the subtitle's taps reach texel 496 at most on all four boot captures (c5000, c9000, c13000, c17000), so the trim changes no tap.
- **Checked on x86 [C]:** the twin (`tools/dev/gl1_x86.sh`, x80t against x79, route i, digests vb 15,500-28,800): 3,449 frames change, all inside codec calls 1 and 2 (vb 15,641-19,897 and 26,068-28,758). Each gains exactly one texture (+32 KB), its "refused" lookup goes from 1 to 0 and its list grows 13 words; batches, primitives and every other counter are equal. The other 2,828 frames are digest-identical. State hashes: 378 / 378 equal `prof/hashes_x86_in_h2.txt` to vb 37,800.
- **Still open on this screen:** the face program's converter (the large item: faces, the opening, deck objects), CT24 direct colour at 0x2000 (overlays and glow), and the one render-target lookup each codec frame still makes.

### 11.18 Task #82: the P07 converter, the codec faces on the GE (2026-10-28; build gl5, delivered 2026-10-28: psp_harness.md "Build gl5")

- **What P07 is [C]:** VU1 program 7 (ELF RAM 0x1a31e0; re/vu1_programs.md 3.15): skinned (1-4 bones, bone matrices in VU memory, weights x 1/128, the blended normal made unit length by ERLENG when more than one bone), lit by three directional lights (L, colour matrix C, ambient, clamp cmax, FTOI0), UV1 in fixed point, optional pass 2 (UV2) and pass 3 (environment UVs from the normal through R, scale and offset). The codec faces (309 calls a frame, ~22.5 vertices a call), the opening cutscene and some deck objects.
- **Code:** `ge/vu1conv.c` `p07_any` (every call: each result through the VU's flush and clamp, any vertex type, every pass) and `vc_p07` with `p7_loop` (the usual calls, a loop per bone count and pass set; the conditions that make it give p07_any's bits are in the comment above P7F; any failed check redoes the call with p07_any). `ge/xconv.c` `xc_lit` takes P07 as it takes P06 (kind 4); `ge/gelive.c` `gl_conv_entry`; `ge/gelist.c` draws its passes as P06's.
- **Checks [C]:** x86 twin over the codec route: 4,509,370 P07 calls, 101,480,329 vertices, the fast form compared with the exact form on every call (XC_P7CHK): 0 differing, 0 fallbacks; passes 2 and 3 drawn 2,621,574 / 2,621,574. gl5 in PPSSPP (`GL3=/home/claude/gl5 sh psp/gl3.sh emu codec`, cfg vblanks 31,400, shots 30600 / 30997 / 31197 / 31397, digests 30,990-31,399): every hash equal to hashref, the 205 frame digests equal the x86 twin's (all e5535d8d57747651); the faces are drawn. gl5 also carries 11.17's subtitle fix.
- **The opening stays black [C]:** in the gl5 emulator run along the whole route (psp_harness.md "Build gl5") the opening's shots from vb 4,297 to 14,997 and at 20,498 / 21,998 have no pixel that is not black, as in gl4c, although P07's calls there are converted; what keeps the opening black is elsewhere (other programs, render targets; not looked at yet).
- **Cost [C model, I PSP]:** PspProf over the whole step at vb 31,510-31,590 (80 steps, cfg `prof 31510 31590 all`, PPSSPP_VFPU_RN=1): 132.7 ms a step, against 75.9 on the PSP for gl4c (faces not drawn) at the same vblanks; the faces nearly double a codec step. By part, the code fetches, and the plan of exact speed-ups: `codec_speed.md`.
- **Correction to 11.17:** the images at 0x2000 that the overlays and the glow read are CT16 (PSM 02), not CT24 (task #83).
- **Seen in gl5's emulator shots (psp_harness.md "Build gl5") [C]:** a faint line in screen row 220 in every codec shot (+6 over the box; solid from the words' left edge to past the box's right edge when words show, dotted otherwise): the text sprite's last row, texel row 120 of REGION_CLAMP v 0..120, which the 512 x 120 text image does not cover [I]; check against gsref with task #84. The largest GE list and aux together grew to 322,392 of 360,448 bytes (gl4c 278,532).

### 11.19 Task #83: the codec's glow and overlays (2026-10-30: the cause, the fix and its checks; ships in gl7)

- **What reads 0x2000 as direct colour in a codec frame [C, x86 twin `MGS2_XLIVE_RTLOG=30997:30997`, container codec68/x84r/out_r.log]:** two faint overlays, TEX0 tbp 0x2000 tbw 3 CT16 512 x 512, tcc 0, CLAMP 0/0 (repeat), TEXA ta0 0x80 aem 0 ta1 0x80, blend (Cs - Cd) x FIX 24 + Cd, texel boxes 0..127 x 0..180 and 0..140 x 0..182 (a 144 x 200 noise image the game uploads each frame, the same bytes every frame); and the glow, tbp 0x2000 tbw 4 CT16 1024 x 1024, tcc 1, CLAMP 1/1 (clamp), TEXA ta0 0x80 aem 1 ta1 0x80, blend (Cd - 0) x Ad + Cs, texel box 134..168 x 118..152, which holds uploaded black texels. On the PS2 the overlays add a faint noise and the glow adds black: the background stays near black.
- **Why the PSP showed teal [C]:** three gaps one after the other. (1) `ge/texcache.c` `tk_get` converted a 16-bit texture only when TEXA had ta0 = 0 and ta1 >= 0x80, so the overlays were refused; the glow's 1024 x 1024 area holds 27 drawn 64 x 64 squares (y 768-960, far from its box), so it counted as a render target, and it is over the GE's 512 anyway. (2) Had they been converted, `ge/gelist.c` `gx_batch` drew a texture of kind 1 without a palette untextured, and every 16-bit texture has none. (3) The glow's blend (signature 0x19) had no rule and fell to the default Cs x As + Cd x (1 - As). So both drew untextured, in their vertex colours, over the whole screen: the teal. With the fix alone the background is near black, as on the PS2 (checks below).
- **The fix [C]** (switch `tk_t16`, on by default; `MGS2_XLIVE_T16=0` in the twin gives the pictures before; the PSP builds keep it on):
  - `ge/texcache.c`: a 16-bit texture converts when TEXA's ta0 and ta1 are each 0 or >= 0x80; `alpha16` sets the GE 5551 alpha bit from TEXA (A bit 1: ta1; A bit 0: 0 with AEM where all 16 bits are 0, else ta0; the overlays are all 1.0, the glow 0 where a texel's 16 bits are all 0); `tk_get_cl` takes a draw's CLAMP in place of the context's. `ge/texconv.c` and `ge/gsup2.c` (`blk_rec16`): CT16 and CT16S converted a GS block at a time (gs_ct16, gs_pa16).
  - `ge/gelist.c`: a 16-bit texture is a GE 5551 texture with no palette; blend 0x19 is Cs + Cd (both factors FIX white: exact where Ad is 0x80 or Cd is black; the glow only draws on the cleared background).
  - `ge/gelive.c` `prim_clamp` / `prim_axis`: a DIRECT primitive with a 16-bit texture, or one over 512 texels on an axis, converts the region its texels can reach: the vertices' texel box plus one texel each side for bilinear taps, clamped where the GS clamps (CLAMP: the texture; REGION_CLAMP: MIN..MAX; REPEAT only when no tap wraps), then taken out to whole GS blocks (16 x 8 texels CT16, 16 x 16 T8, 32 x 16 T4, 8 x 8 in the CT32 layout) inside those limits and the GE's 512. Every tap stays inside the region and clamps where the GS's does. The codec's other DIRECT draws with a 1024-texel side (the text page, T4HH 1024 x 256, and the panel's T4 256 x 1024 at 0x2000) now take the same path: each converts the blocks its box reaches instead of the game's REGION_CLAMP.
  - **Why the blocks [C]:** the overlays' boxes move by a few texels from frame to frame, and the CLAMP is part of a texture's identity, so the first version (boxes as they are) converted both overlays again every frame: on the codec route (PPSSPP master runs of the PSP harness) 15,432 conversions and 704,937 KB converted, against gl6's 1,323 and 11,500 KB; an earlier build of it (b) cost the model's codec step 8.1 ms (PspProf vb 31,510-31,590: 123.7 ms against gl6's 115.6). With the blocks: 1,405 conversions, 14,802 KB.
  - **Cost with the blocks [C model, I PSP]** (PspProf vb 31,510-31,590, 80 steps, `prof/top_g83e.txt` against `top_g6.txt`): 118.2 ms a codec step against gl6's 115.6 (+2.6, +2.2%). The instructions fall a little (12.58 M against 12.61 M a step); the rest is cache misses, I 136.8 k against 131.7 k and D 77.3 k against 75.3 k a step, spread over many functions whose code moved (xc_call +1.2 k I-misses, gb_prim +1.0 k, host_leave +0.7 k; xc_pass -1.0 k), plus gu_area_id's 22 k more instructions (the new regions' identities [I]). At the PSP's I-miss cost (about 73 cycles, not the model's 125) that is about +1.5 ms a codec step [I]; gl7 times it on the PSP.
- **Checks [C]:**
  - **Against the PS2** (gsref frames from the game's own GS data, `tools/dev/piccmp.py`, mean levels / share over 16): vb 15,643 gl6 186.0 / 100% -> 1.0 / 0.00%; vb 16,999 49.5 / 80.1% -> 2.3 / 0.89%; vb 30,995 20.8 / 42.2% -> 3.0 / 1.17%. What is still off is thin lines and letter edges (the 512 x 512 to 480 x 272 squeeze) and the faces' noise lines. Side by side in Ffm's folder, `SHOTS\codec_background_fix`.
  - **PPSSPP master, codec route (container s1/mst_g83c, mst_g83d):** every state hash equals hashref (438 lines, no split) and the 205 frame digests of vb 30,990-31,399 equal the x86 twin's (first version: twin f83/tdbg; with the blocks: f83/trd). The 13 shots with the blocks equal the first version's but two, vb 30,995 and 39,897, which differ only in screen row 220 (302 and 325 pixels, at most 6 levels): the faint line of 11.18 (#84), the text sprite's last row reading the text page's texel row 120, which the subtitle image does not cover. Against the PS2 frame of vb 30,997, which has no line there, that row is closer with the blocks (mean 3.16 levels against 8.00). Its texels change with when the page was converted, a further sign of the stale page of #84 [I].
  - **Exact state:** every hash of vb 31,500-31,599 equals hashref (a hash at every vblank; the first difference at vb 31,600 is the profile's stop, #92).
  - **Indoor route, x86 twins** (f83/ti_ref: the shared code; f83/ti_fix: the fix; `gl1_x86.sh <dir> i i 31801:31899`, 17,664 frames): the lists differ in codec calls 1 and 2 only (vb 15,641-19,899 and 26,068-28,760: +6,366 and +4,017 batches, 6,048 and 3,742 uses of 16-bit textures, refused lookups 9,351 -> 5,248 and 3,869 -> 0, render targets 2,167 -> 1) and, in 783 other frames (vb 1,117-2,665 and 36,719-36,757), only in the textures' bytes (1 to 4 KB less a frame; batches, primitives and lookups equal; DIRECT primitives with a 1024-texel side [I]). The 50 digests of vb 31,801-31,899 are equal (25efe5bcab7e30fa). The codec route's master shots at vb 1,199 and 2,199 (the same game there) equal gl6's byte for byte.
- **Still open:** #84 (the stale text page and the row-220 line; `me_step1_notes.md` 5 adds that the page's GE rows 120-127, below the 120-row image, which the sprite's last row reads in row 120, came out other when converted again after a take-back at vb 36,000: 132 nonzero texels a row in the run's own conversion, all 512 in the new one, under the same identity [C]); blend 0x6a, (0 - 0) x FIX + Cd, has no rule (3,684 batches in 501 frames, vb 8,756-39,551 on the codec route; #68); the 21 frames of vb 15,761-15,801 with about 250 refused lookups each (the faces' first frames [I]), as before the fix.

### 11.20 Stage B designed: lanes in the walk (2026-10-30; task #99)

**Where the picture time goes now [C model]** (gl7, PspProf, 80 steps each, all four speed-ups on; dumps in the container, gl7/model; each function placed by the object file that defines it): ms a step = instructions / I-misses / D-misses.

| part | aft deck vb 30,810-30,890 (211.2 ms a step) | inside vb 33,410-33,490 (145.0) |
|---|---|---|
| translator (spans, VU memory, calls) | 38.5 = 13.5 / 13.0 / 12.0 | 28.1 = 8.7 / 10.3 / 9.0 |
| converters | 34.8 = 10.1 / 18.9 / 5.9 | 27.2 = 6.7 / 15.7 / 4.8 |
| texture map (gsup2) | 15.0 = 2.9 / 8.5 / 3.6 | 15.2 = 3.3 / 8.4 / 3.5 |
| texture cache and conversion | 8.3 = 1.0 / 5.3 / 2.0 | 8.3 = 1.0 / 5.4 / 1.9 |
| frame and batches | 8.1 = 0.8 / 6.0 / 1.3 | 7.5 = 0.7 / 5.6 / 1.3 |
| GS front end | 6.8 = 0.9 / 3.4 / 2.5 | 5.0 = 0.6 / 2.5 / 1.9 |
| harness glue | 4.6 = 0.4 / 3.3 / 0.9 | 3.6 = 0.3 / 2.5 / 0.8 |
| GE list | 2.9 = 1.8 / 0.0 / 1.0 | 2.8 = 1.8 / 0.0 / 1.0 |
| **picture, sum** | **119.0 = 31.5 / 58.4 / 29.2** | **97.6 = 23.1 / 50.5 / 24.1** |
| the walk (io_flush) | 12.2 = 6.2 / 3.9 / 2.2 | 7.5 = 3.5 / 2.6 / 1.4 |
| game code (logic and the game's draw code) | 52.7 = 16.9 / 18.8 / 17.0 | 23.9 = 6.8 / 10.1 / 6.9 |

- The picture runs on frame steps only (the translator skips vblanks whose calls were all reduced, §11.12), so a shown frame carries twice these figures.
- **Half of the picture time is instruction-cache misses** (58.4 and 50.5 ms a step). Every VU1 call goes through the walk, the span reader, call(), the A+D kick, the GS front end, the texture lookup, a converter, the batch and the glue: more code than the 16 KB I-cache holds, so most of it is fetched again for each call. Spread over the calls, about 135 µs a VU1 call on the deck (2 x 119 ms over 1,768 calls a frame step) [I].
- Calls a step (the model's call graph, deck / inside): VU1 calls 884 / 471; converter calls 510 / 395 (df0 179 / 186, effect fans 93 / 16, P06 187 / 139, and P06's extra passes 155 / 154); spans 10,919 / 6,475.
- vm_all (every pending quadword decoded before each converter call, gelive.c gl_call) 13.5 / 10.1 ms a step, 26.5 µs a converter call on the deck; it also decodes the rule-only calls' data, which nothing reads.
- The game's own packet builders are cheap: P1214a0 0.49, P120860 1.38, P12e8b8 1.24, P12d910 0.60, P13bf78 0.79 ms a step on the deck.
- Side finds: vc_xform 4.84 ms a step on the deck (10.5 µs a call), and xc_xform calls soft-float double conversions (__truncdfsf2, __unpack_d in libc): worth fixing whichever way Stage B goes.

So the aim is less code per call and the work done in batches, more than fewer instructions.

**Three ways** [C for what each one touches, I for its savings]:
1. **Emitters at the builders** (§11.3 as written): the builder's call writes GE batches instead of packets on frame steps. Saves the builders and the walk of their packets too. Against: the game's RAM changes on frame steps (the packet bytes are not written), so every hash reference is made again and the from-boot read check (speed_notes §34) must show the game never reads the unwritten bytes; the packet pointer must still advance by the original size (P1214a0 skips a group when [0x1e50a0] would pass [0x1e5090], so writing less would draw groups the PS2 drops); P12d910's return value is compared by its caller (0x121d5c); the batches must be spliced into the walk's order by a marker in the chain; a hook in the game's code for each builder.
2. **Lanes in the walk (recommended).** The packets stay as the game builds them. At a tag, the exact walk (`runtime/ee_io.h` vif1_chain_fast) recognises a builder's packet layout (`re/mesh_packets.md` §3, 6-10), checks that no tag of it stops or bends the chain (no IRQ bit, no END / RET / CALL), leaves the walk's VIF state as the generic walk would, and hands the whole group to its lane in one call. The lane: (a) keeps the translator's state as the span reader would (STCYCL, the double buffer and TOPS, the UNPACK descriptors and own[] at their known strides, the call's bookkeeping), so the VU memory model stays exact for every call without a lane; (b) applies the material's A+D from the material record in stream order (GS registers, front end, CLUT load, texture lookup); (c) records the call: its entry, the records its REF tags point at (constant block, streams, header) and its batch slot; (d) does the vertex work straight from those records into the slot. The records are guest RAM, which nothing writes while a transfer is walked, so (d) can wait until the transfer's end and run kind by kind with its code hot: codec step 4's grouping idea (`codec_speed.md`), for every kind. For: the game's RAM and every hash unchanged by construction; the draw order is the walk's; no hooks in game code; a layout no lane recognises goes through Stage A as now. Against: the builders still run on frame steps, and the walk still reads each group's 7-8 quadwords.
3. **Direct reads only:** the converters read their input through the descriptors (own[]) instead of vm_all; nothing else changes. The smallest change; it saves at most vm_all's 13.5 / 10.1 ms a step and keeps the long per-call code path.

**Recommendation: 2, with 3's direct reads as the lanes' input.** It reaches what costs (the span reader, call(), vm_all, the per-call code and its I-misses) without changing anything the game can see. The builders' own time is then the reduced pipeline's business (speed_notes §33 "Next": 0x12e8b8, 0x12d910, 0x120860, 0x13bf78 return at entry on steps that are never shown), checked as §34 checked 0x1214a0. When converter calls stop calling vm_all, descriptors wait until the ring of 4,096 is full; that flush should then scan own[] (1,024 entries) instead of every descriptor's vectors.

**Lanes in order:**
1. **P1214a0 groups** (§3: 7 or 8 quadwords; entries +780, +a70, +df0, +1088 by P04's table 0x17cff0). df0 is drawn today and is the largest converter kind on both routes (179 / 186 calls a step). +780 / +a70 / +1088 are walked and parsed today but not drawn (rule only, §11.13): their converters are added inside the lane.
2. **P12e8b8 groups** (§6, P06: Snake, the crew, the guards; optional material and stream quadwords).
3. **P12d910 sort-pass prims** (§7): +25f0 / +2870 drawn today; +2b60 lines, +3740 sprites and +3a38 quads not yet.
4. **P07, the codec faces** (309 calls a frame): its builder is not in `re/mesh_packets.md` §12 yet; first find it (an x86 trace of the writer of its packets).
5. P09 the sea (§8), P01 (§9), the stage program (§10: the radar).

**Lane 1, one change per measurement:**
- a. x86 twin, conversion at once (no waiting): with the lane on and off, the digest file (`ge/xldig.c`: a line per call and per vblank) and every frame's picture digest equal from boot on the deck, indoor and codec routes; the translator's state (V, the VU memory as read, the GS registers, the map's identities) equal at the end of every transfer; the walk's VIF state and the chain's end equal after every transfer; every hash as hashref (the game is not touched).
- b. Model (PspProf on gl7's two windows) against 211.2 / 145.0 ms a step.
- c. The vertex work at the transfer's end, kind by kind: the same checks, the model again.
- d. Then a PSP build, described to Ffm first, after gl7's run is read.

### 11.21 Lane 1 (P1214a0 groups) built, checked and timed (2026-10-30; task #100)
**What (x86 tree /home/claude/sb1, saved as `tools/dev/sb1/sb1_lane1.diff` with its scripts; not in the shared code yet):** `runtime/ee_io.h` `lane_p04` in `vif1_chain_fast`: at a tag, with TTE on and no command data left, a P1214a0 group (REF tags whose TTE words are STCYCL and an UNPACK taking exactly the REF's data: the constant block (24 qw, optional), the material (8), streams A and B (V4-16 at T + 0x100 / 0x101, STCYCL 3 / 1) and C (V2-16 masked at T + 0x102); then a CNT of 2: the header's UNPACK, MSCAL and three NOPs; no IRQ bit; every piece in one run of RAM or scratchpad) goes to `ge/xlate.c` `xl_lane_p04` whole. The walk's state afterwards is the generic walk's (STCYCL 1 / 1, nothing left, the last command a NOP), its x86 command counters too. The translator does what `xl_vif1_span` does with those words (the descriptors, own[], STCYCL, the counters, the call) and gives a P04+df0 call its three streams (`XlCall.rec`, only when the header's count is at most each stream's vectors and the kinds are 2 or 3, 2 or 3, and 4); `vc_df0_rec` reads the vertices from the game's data, `gl_call` skips `xl_vm_sync` for it (the translator decodes the quadwords vc_df0 reads besides: T + 0..3, T + 0x14, abs 2..5). With the lane on, `vm_all` scans own[] when more than 1,024 vectors wait (the same quadwords decoded). Switches: x86 `MGS2_SB_LANE=1` (`MGS2_SB_CHK=1`: every lane df0 call also against vc_df0 on the decoded VU memory); PSP g_opt `OPT_XL_LANE` (0x100000; off = gl7's path).
**Checks [C], lane on against lane off, from boot:** indoor route to vb 37,800: all 17,664 frame digests the same (vb 26,061-26,459 also = gl7's twin), 378 / 378 hashes = hashref; deck tutorial to 25,200: 11,766 frames, 252 / 252; codec route to 40,000: 18,935 frames, 400 / 400. The translator's totals (words, commands, calls, A+D kicks, GIF tags, front-end primitives, UNPACK kinds, entries) and the walk's VIF command counts the same. `MGS2_SB_CHK`: 1,732,479 + 653,169 + 529,965 lane df0 calls, 0 differ. The per-call digest file (`ge/xldig.c`: the VU memory as read and the GS registers at each call, every primitive) on the deck, vb 30,800-30,900: 89,897 calls (18,911 df0) byte for byte.
**Model [C model]** (PSP build sbp/d: gl7's objects with this tree's boot_rt.o, xl_psp.o and ge_*.o, EBOOT 4cba02e8; gl7's windows; lane off = `MGS2_OPT=ffefffff`): aft deck 220.4 -> 216.7 ms a step (-3.7, -1.7%), inside 149.2 -> 144.1 (-5.1, -3.4%). Saved: vm_all -4.0 / -4.4 ms (the df0 calls' decodes), the span front and vif_cmd -1.7 / -1.9; cost: xl_lane_p04 +1.1 / +1.3, vc_df0_rec +0.4 / +0.6 over vc_df0 (its reads of the game's data miss the D-cache).
**What it says:**
- vm_all is still 9.5 / 5.9 ms with the lane on: every other converter call (P06, the effects, P07) decodes every pending quadword, the lane's groups' included. Lane 1's saving waits on the other converters reading their input without a whole-memory decode (their own lanes).
- The build itself costs, lane off, +9.2 ms on the deck and +4.2 inside over gl7's model: io_flush lost its PGO profile (vif1_chain_fast changed: 9.5 k -> 19.2 k I-misses a step, +3.9 ms), ud_new and ud_ok are no longer inlined into the span front (+0.9), and other functions' I-misses moved with the new layout. So a delivery build of any lane needs the PGO profile and the layout refreshed (`tools/dev/pgo_run.sh`, the layout flow of speed_notes) and ud_new kept inline. The same refresh with the picture code in the layout search may itself be worth something on gl7's code (the picture's I-misses are 58 ms of 119 on the deck) [I].
**Next (recommended):** lane 2, the P06 groups of P12e8b8 (Snake, the crew, the guards), with the same checks; with lanes 1 and 2 the deck's and the corridors' converter calls no longer need the whole-memory decode, and the model then says what the lanes give together. Then step c (the vertex work at the transfer's end, kind by kind), then one PSP build with PGO and layout refreshed, described to Ffm first.

### 11.22 Direct reads (xl_vm_need): exact, but slower; parked (2026-10-30; task #102)
**What (x86 tree /home/claude/sb1, in `tools/dev/sb1/sb1_stageb.diff`):** before a converter call, `ge/xlate.c` `xl_vm_need` decodes only the VU quadwords that call's converter reads, instead of every pending one (`vm_all`): abs 0..0x3f, T + 0..0x17, T + 0xfe / 0xff, and the vertices from T + 0x100 (stride 3 for df0 / FX / FX2, 6 for P06, 7 for P07, times the header's count at T + 0x14 (P07: T + 0), at most 1,024 quadwords), and for P07 each vertex's bone matrices ((B[b] & 0xffff) + 32, 4 quadwords each, b < nb <= 4, B at T + 0x106 + 7 i); any other entry still calls vm_all, and a lane-1 df0 call (rec) only the constants. A pending quadword met in those ranges decodes its whole descriptor (`need_q` -> vm_all's `ud_run`), which is exact within a transfer: the descriptors' data in guest RAM does not change before `xl_flush` decodes the rest at the transfer's end. Switches: x86 `MGS2_SB_NEED=1` (`MGS2_SB_NCHK=1`: every converter call run on both memories, the decoded-on-need one and the full decode, and the batches, data and sizes compared); PSP g_opt `OPT_XL_NEED` (0x200000), read by `psp/xl_psp.c` only in builds made with `-DSB_NEED`.
**Checks [C], from boot, with lane 1 on:** indoor route to vb 37,800, deck tutorial to 25,200, codec route to 40,000: every frame digest equal to the lane-1 run's (17,664 / 11,766 / 18,931), hashes 378 / 252 / 400 = hashref, the translator's totals equal. `MGS2_SB_NCHK` on the indoor route: 5,288,081 converter calls run twice, 0 differ.
**Decodes saved [C x86, whole routes, quadwords decoded when read + at flushes]:** indoor 682.7 M -> 547.6 M (-19.8%), deck tutorial 311.9 M -> 266.6 M (-14.5%), codec 578.9 M -> 541.9 M (-6.4%); flushes 5.35 M -> 87,580 on the indoor route.
**Model [C model]** (PSP build sbn/d: gl7's objects with the sb1 tree's ge, xl_psp and boot_rt, EBOOT 663c3940; gl7's windows on the indoor route; off = `MGS2_OPT=ffdfffff`): aft deck **217.8 -> 219.3 ms** a step (+1.5), inside **145.4 -> 147.9** (+2.5). need_q costs 11.41 / 7.92 ms (1.49 M / 1.01 M instructions a step) where vm_all cost 9.31 / 5.81 (0.89 M / 0.51 M); D-misses -3.5 k / -1.2 k a step. The decodes saved are real, but need_q visits every quadword of every range it is given (about 90 for the constants and 6 n for a P06 call of n vertices) to find the few descriptors that own them, and that scan costs more than the decodes it saves. A pending-bit map (1,024 bits; ranges checked 32 quadwords at a time) would take most of the scan away, for maybe 0.5-1 ms on the deck [I]; not worth its upkeep in ud_new and ud_run before the bigger levers below.
**Also measured:** with direct reads off, sbn's code is +1.1 / +1.3 ms over sbp's (lane 1 alone, 216.7 / 144.1) from placement alone.
**What it says, and next (recommended):** the decode bookkeeping is a few ms; the picture's time is I-misses (deck 267 k a step: instructions alone 64 ms of 218). The picture code (`ge/*.c`, `psp/xl_psp.c`) has never had a PGO profile (no gcda: plain -O2) and is not in the layout search (`layout_elf.ld` of gl1-gl7 orders game and runtime functions from pre-picture traces; xc_call, vc_p06, tk_get_cl and the rest sit where the linker left them). So next: PGO and a layout search with the picture on, on the lane-1 build, from full-step traces of both routes (the v17 flow: several windows, some held out), checked exact in PPSSPP. That is exact by construction (placement and branch layout only), is needed by any lane build anyway (§11.21), and it moves the baseline every later lever is measured against. Then the profile decides between lane 2 (P06) and step c (vertex work by kind at the transfer's end).

### 11.23 The picture code's first PGO profile and code layout (2026-10-30; task #103)
**Why:** since gl1 the picture code (`ge/*.c`, `psp/xl_psp.c`) was plain -O2: no PGO profile, and not in the layout search (gl1-gl7 reuse the pre-picture `layout_elf.ld`, so xc_call, vc_p06, tk_get_cl and the rest sat where the linker left them), while the picture's time is mostly I-misses (§11.20).
**How (container /home/claude/sbg; `tools/dev/sb1/pgo_gl.sh`, steps gen, run, use, trace, order, d):** the base is gl7's build with the sb1 tree's lane 1 (as `mkpsp.sh`).
- gen: boot_rt.o (lane 1 changed vif1_chain_fast, so its old profile no longer fit io_flush), xl_psp.o and the twelve picture objects built with `-fprofile-generate`, main_psp.o with `MGS2_PGO`; the game's own objects (gen.o, gen_dispatch.o, the overlays) keep gl7's profile, their code being gl7's.
- run: the indoor route from boot in PPSSPP with `pgo 15000 37790` (the opening's two story calls, Snake waiting and walking on the aft deck, the two minutes inside; picture on): 434 / 434 hashes = `hashref_x86_in_h2.txt`, 665 s.
- use: those objects rebuilt with `-fprofile-use` (the Makefile's PGO=use flags), relinked with gl7's layout files: build u (EBOOT b00c6dfb).
- trace: u in PspProf with an instruction trace of eight 5-step windows per route (`prof a a+5 all`). Deck trace: story call 1 (vb 16,001 and 17,501, overlay d00t), call 2 (26,501 and 27,801, d01t), Snake waiting (29,601, 30,101) and the walk (30,701, 31,101) in w00a; inside (w01a): 31,901 to 37,151 every 750 vb. Only w00a and w01a are laid out, so the calls' fetches in the overlay arena move up 16 MB (a multiple of the I-cache's 8 KB way: the same sets): layout_sim keeps them as fixed code in the cache instead of reading them as w00a's functions. Two windows of each trace are held out of the search (a call-2 window and a waiting window; inside 35,651 and 36,401).
- order: `tools/layout_sim.c` on both traces at once (one ELF order for both overlays), 40,000 moves, seeds 1 and 2; `tools/layout_order.py` now lists the picture's objects and libpspgu / libpspsystemctrl_user (absent from older builds, so skipped there).
- d: u relinked with seed 2's order (`tools/dev/relink_order.sh`): EBOOT **6b888a4e**.

**Layout [C sim]:** functions fetched in the windows: ELF 929 (2.25 MB), w00a 504 (1.91 MB), w01a 457 (1.63 MB). I-misses of the windows, u's placement -> seed 2: deck trace 4.27 M -> 3.16 M (-25.9%), held out 1.29 M -> 0.94 M (-27.0%); inside 5.26 M -> 3.96 M (-24.7%), held out 1.74 M -> 1.31 M (-24.6%). Seed 1: 7.18 M fitted and 2.26 M held out, against seed 2's 7.12 M and 2.25 M. `layout_sim -new` on build d's real addresses gives exactly the searched counts on both traces.
**Exact [C]:** build d with gl7's delivery cfg in PPSSPP 1.20.4 (`GL3=/home/claude/sbg TAG=sbg sh psp/gl3.sh emu hw`), to the end of the route: the 434 hash lines (= hashref), the 311 VU hash lines, the 199 frame digests of vb 26,061-26,459 and the picture's totals equal gl7's run line for line, and the 13 shots equal gl7's byte for byte. The code is 11 KB larger (3,998,476 bytes of text against 3,987,228), so the heap is 32 KB smaller.
**Model [C model]** (PspProf, 80 steps; gl7's windows; ms a step):

| build | aft deck vb 30,810-30,890 | inside vb 33,410-33,490 | codec route vb 31,510-31,590 |
|---|---|---|---|
| gl7 | 211.2 | 145.0 | 117.3 (I-misses 132.1 k) |
| sbp: lane 1 (§11.21) | 216.7 | 144.1 | |
| u: + the profile | 197.8 (I-misses 210.5 k) | 129.0 (156.1 k) | |
| **d: + the layout** | **175.8** (152.0 k) | **114.6** (117.5 k) | **96.4** (74.9 k) |

- The profile alone: deck -18.9 ms (-8.7%), inside -15.1 (-10.5%); the layout on top: -22.0 (-11.1%; I-misses -27.8%) and -14.4 (-11.2%; I-misses -24.7%, as simulated). Against gl7: deck **-35.4 ms (-16.8%)**, inside **-30.4 (-21.0%)**, and the codec route's window **-20.9 (-17.8%)** with I-misses -43% (the codec calls' code was in the deck trace's windows; both builds were profiled there today, their 373 hash lines equal to vb 31,500).
- Largest falls from the layout (deck): io_flush -2.3 ms, gl_call -1.6, gu_clut_load -1.4, gif_qw -1.4, gu_drawn -1.3, call -1.3, on_call -1.2, memset -1.2, xc_call -1.1. Some rose where the search traded: vc_xform +1.5 / +1.1 (I-misses 3.0 k -> 7.1 k a step on the deck), vc_p06 +0.5 / +0.4, drew_ctx +0.4 / +0.4; the next refresh (every lane build needs one) searches again. ud_new and ud_ok stay out of line in u and d (the profile's choice; §11.21 asked for ud_new inline), yet the span front with them (xl_vif1_span + ud_new + ud_ok) costs less than gl7's with them inlined: codec window 4.45 ms a step against 4.57, deck 7.87 against 10.46, inside 5.05 against 7.35 (deck and inside with lane 1). §11.21 put the calls at 0.9 ms a step on the deck; forcing them inline in a profiled build is untried.

**Where the time is now [C model]** (build d; ms a step, deck / inside, each function by the object that defines it): game code 50.6 / 23.0; picture **89.2 / 70.5** (gl7 119.0 / 97.6): translator 29.5 / 19.8 (vm_all 8.8 / 5.4, ud_new 4.5 / 3.0), converters 29.0 / 22.4, texture map 10.3 / 10.5, GS front end 5.6 / 3.8, frame and batches 5.3 / 4.9, texture cache 4.9 / 4.8, GE list 2.6 / 2.4, glue 2.2 / 1.8; the walk 11.5 / 7.1; runtime helpers 11.7 / 5.4; libc 6.3 / 5.2.
- **The converters' I-misses hardly moved:** 36.6 k -> 34.1 k a step on the deck (12.8 ms), 32.7 k -> 29.7 k inside (11.1 ms), while the translator's fell by about 40% (32.2 k -> 18.6 k; 24.3 k -> 15.2 k) and the texture map's by about half (20.5 k -> 9.9 k; 18.3 k -> 11.4 k). A converter call runs xc_call (9.9 KB, with the paths it inlines), a vertex routine (vc_p06 26.8 KB of specialised loops, vc_df0_rec), vc_xform, xc_xform, xc_gs, xc_state and vc_strips, and between two calls the translator, the texture lookups and the walk run: no order keeps that in a 16 KB cache. That is step c's target (the vertex work kind by kind at the transfer's end, its code hot), not a layout's.
- The D-misses did not move (vm_all 13.6 k a step on the deck, ud_new 3.8 k): the decode bookkeeping of §11.22.

**What it says, and next (recommended):**
1. **Lane 2 (P06 groups of P12e8b8)** with lane 1's checks. Batching the P06 work needs each call's input to outlive the call: the VU memory a P06 call reads is overwritten two calls later, while lane 2's records (the game's streams, constants and materials, which nothing writes during a transfer) are not. Lane 2 also saves for the P06 calls what lane 1 saved for df0 (the span reading and the per-call decode).
2. **Then step c for df0 and P06 together.** Design questions first: the cut test and the texture lookup stay at the call (they read the GS state and GS memory of that moment, and the cut must come before the lookup); a batch's data size is known only after its strips are read, so the deferred batches' data lands in the frame arena in a different order (the same picture; the per-call digests then compare the drawn content, not addresses); the passes 2 and 3 of a P06 call (xc_pass) follow their call.
3. A PSP build of this one now (gl8: gl7's cfg, build d) checks on the hardware the I-miss saving step c is counted on.

### 11.24 Lane 2 (P12e8b8's P06 groups) built, checked and timed (2026-10-30; task #101)
**What (x86 tree /home/claude/sb2, saved as `tools/dev/sb2/sb2_stageb.diff` (all of Stage B against the shared code) and `sb2_lane2.diff` (lane 2 against sb1's tree), with its scripts; not in the shared code yet):** `runtime/ee_io.h` `lane_p06` in `vif1_chain_fast`, tried after lane 1 at a tag whose TTE word is STCYCL 1 / 1 (no command data left, TTE on): a P06 group (`re/mesh_packets.md` §6) is the optional constant block (REF of 24 quadwords, UNPACK V4-32 to T + 0), three material slots (each a REF of 8 quadwords, V4-32 to T + 0xe8 / 0xf0 / 0xf8, or a CNT of 0 whose TTE words are STCYCL 1 / 1 and a NOP: not sent), six stream slots at STCYCL 6 / 1 (A, B and F V4-16 at T + 0x100 / 0x101 / 0x105, C, D and E V2-16 masked at T + 0x102..0x104, each UNPACK taking exactly its REF's data, or CNT 0 + NOP), then the CNT of 2 (the header's UNPACK to T + 0x14, MSCAL, three NOPs); no IRQ bit, every piece in one run of RAM or scratchpad. `ge/xlate.c` `xl_lane_p06` does what `xl_vif1_span` does with those words (STCYCL per slot, the descriptors and own[], the counters, the census, the call) and gives the call the six slots (`XlRec`, lane 2: pointer, vectors, decode kind; ROW); call() decodes the other quadwords P06 reads (T + 0..0x14, T + 0xfe, 0xff, abs 2..5). `ge/vu1conv.c` `vc_p06_rec` takes A, B (kind 2), C (kind 4), D on pass 2 (header y bit 4) and F for the colour factor (+738, +ab0) from the game's data when each was sent with at least the header's count, else returns -8 and `xc_lit` decodes the VU memory and runs `vc_p06` (exact either way); the 24 specialised loops of §11.16 gained a twin each that loads from the streams (`p6_loop` REC), and pass 2's own UVs come from stream D (`vc_p06_uv2_rec`). `gl_call` skips `xl_vm_sync` for a lane call. Switches: x86 `MGS2_SB_LANE` is now a mask (1 lane 1, 2 lane 2, 3 both; `MGS2_SB_CHK=1` checks each lane df0 and P06 call against the converter on the decoded VU memory: n, VcLit, the vertices of passes 1-3, the flags); PSP g_opt `OPT_XL_LANE2` (0x400000, on by default; `MGS2_OPT=ffbfffff` = lane 1 only).
**Checks [C], from boot, lanes 1 + 2 against lane off (sb1's runs of §11.21) and against lane 1 alone on this build:**
- indoor route to vb 37,800: all 17,664 frame digests the same, 378 / 378 hashes = hashref; deck tutorial to 25,200: 11,766 frames, 252 / 252; codec route to 40,000: 18,931 frames (§11.21's 18,935 counted four summary lines), 400 / 400. Each route both with `MGS2_SB_CHK` and without it (the checks decode the VU memory at each lane call, so only the run without them is the PSP's path).
- Every other line of the logs the same (translator totals, census, UNPACK kinds, the walk's VIF command counts, textures), but the lazy decode's counts: quadwords decoded at flushes 547.0 M -> 271.7 M indoors, 210.8 M -> 138.2 M on the deck, 461.1 M -> 388.1 M on the codec route; decoded when read (call()'s quadwords for the lane calls) 125.6 M -> 155.7 M, 92.0 M -> 109.0 M, 107.7 M -> 126.8 M; in all -36%, -18%, -9%.
- Every P06-family call took the lane: 2,164,203 groups indoors (= the census's calls of 19fe08, 1a0160, 1a0540 and 1a08c0), 952,002 on the deck, 1,110,766 on the codec route; the converter calls among them (shown frames) 1,426,658 + 265,586 + 373,221, none sent back to the VU memory, and with `MGS2_SB_CHK` 0 of 2,065,465 differ.
- The per-call digest file on the deck (vb 30,800-30,900, `dig_x86.sh`, no checks): 89,897 calls (19,183 P06 from the lane) byte for byte as lane off and lane 1.
**Model [C model]** (build `/home/claude/sb2p/d`: sb2's tree with gl7's cfg as `mkpsp.sh` makes it, the picture code plain -O2 as sbp's (no PGO profile, no layout), EBOOT af3c743e; gl7's windows, 80 shown steps each: deck vb 30,810-30,890, inside 33,410-33,490; the lanes switched by `MGS2_OPT` in the one build):

| model ms a step | lanes off (`ffafffff`) | lane 1 (`ffbfffff`) | lanes 1 + 2 |
|---|---|---|---|
| aft deck | 221.6 | 217.7 (-1.8%) | 211.7 (-9.9 ms, -4.5%) |
| inside | 151.6 | 147.4 (-2.8%) | 141.5 (-10.1 ms, -6.7%) |

Lane 1 alone moves as on sbp (§11.21: -1.7%, -3.4%); lane 2 adds -6.0 / -5.9 ms. By part (`profcat.py`, off -> both lanes, deck / inside):
- the translator 41.3 -> 31.5 ms / 30.2 -> 21.1: instructions -25% / -36%, D-misses 34.2 k -> 20.2 k / 26.3 k -> 13.3 k a step, I-misses 38.5 k -> 35.7 k / 30.1 k -> 27.4 k. vm_all (the decode at flushes) 13.1 -> 3.8 / 9.9 -> 1.5 ms, xl_vif1_span 5.3 -> 2.4 / 3.6 -> 1.0, vif_cmd 2.9 -> 1.1 / 2.1 -> 0.4; the lanes' own work (xl_lane_p04 + xl_lane_p06) 2.6 / 2.5 ms.
- the converters 33.5 -> 34.6 / 26.5 -> 27.4 ms: they read the streams from the game's data now (vc_p06_rec 7.04 ms against vc_p06's 6.70 on the deck, 5.78 against 5.77 inside; vc_df0_rec 1.96 against vc_df0's 1.72, 2.62 against 2.11), their D-misses 15.7 k -> 18.6 k / 12.8 k -> 15.6 k.
- the walk -0.6 / -1.0 ms, the GS front end -0.4 / -0.4, the texture map -0.2 / -0.7.
- A step's calls: converter calls 511 / 395 (df0 179 / 186, P06 187 / 139, the effects' P04+25f0 and +2870 144 / 70, of which 93 / 16 reach vc_fpos; the others return at the "all vertices at one point" test), P06 passes 155 / 154, texture lookups 599 / 522; transfers (xlive_one_end) 6 / 5. With both lanes 144 / 70 calls still decode the whole VU memory at their flush (xl_vm_sync): the effects' calls, which have no lane.

**What is left [C model]:** the picture's code misses the I-cache. Both lanes on, deck: the converters 47.1 k I-misses a step (17.7 ms of their 34.6; 92 a call), the translator 35.7 k (13.4 ms), the texture map 23.2 k, the walk 20.6 k, frame and batches 17.3 k, the texture cache 13.4 k, the GS front end 10.9 k, the glue 8.6 k: 177 k, 66 ms of the step's 211.7 (instructions alone 61.6 ms). Inside: the converters 40.1 k (15.1 ms; 101 a call); the picture's parts 146 k, 55 ms of 141.5. The lanes took instructions and D-misses away; the I-misses hardly moved (-1.7 k, -3.1 k), as §11.23 found for the layout. This build has no PGO profile: sbg's build with it and the layout (§11.23) missed 34.1 k a step in the converters on the deck, so a delivery build's counts are lower than these.

**Next (recommended):**
1. Size step c before building it. The converters' I-misses are the largest single target left (sbg after PGO and layout: 34.1 k a step on the deck, 12.8 ms; 29.7 k inside, 11.1 ms), and step c runs the vertex work of df0 and P06 at the transfer's end, kind by kind, so that each kind's code stays hot through all its calls (6 / 5 transfers a step against 366 / 325 df0 and P06 calls). What it saves depends on how much of a call's work can move (the cut test and the texture lookup stay at the call, and df0's input is read for the cut) and on how much of the translator's code then stays in the cache. That can be counted without building it: an instruction trace of lanes 1 + 2 in PspProf, its fetch runs regrouped as step c would run them (each converter call's work after its texture lookup, and its passes, moved to its transfer's end, df0 calls first, then P06), through layout_sim's cache model against the trace as it is. If the saving is worth the work, the design questions of §11.23 item 2 are settled in a new x86 tree: the per-call digests compare content (the blobs land in another order); whether a call makes a batch is known at the call (the draw order and o->cur's merging depend on it); the VU constants a deferred call reads are saved at the call (the next groups overwrite them); the game's data itself stays as it is until the transfer ends (vif1_chain_fast walks a whole transfer before it calls xlive_one_end).
2. A lane 3 for the effects' groups (144 / 70 calls a step) would end the last whole-memory decodes (vm_all 3.8 / 1.5 ms) [I]: smaller than step c's target.
3. Every lane build needs its PGO profile and layout refreshed (§11.21) before a PSP build; the next one carries lanes 1 + 2 (and step c if it is built), described to Ffm first and made only after gl8's run is read.

### 11.25 gl8 on the PSP: the cost model's weights refitted; step c sized by a trace replay (2026-10-30; tasks #105, #106)
**gl8 on the PSP [C]** (psp_harness.md "Build gl8", run 1): exact to vb 37,800, its 13 shots equal gl7's PSP shots; the model's own 80 steps inside 126.47 -> 107.97 ms a step (-14.6%; the model said -21.0%), the deck window -11.7% (the model -16.8%), the story calls -10.4% / -10.3%.
**The weights refitted [C fit]:** the model of §11.20, ms = (instructions + 125 x (I-misses + D-misses)) / 333,000, reads high on the picture builds (inside 145.0 / 114.6 against the PSP's 126.47 / 107.97; the deck 211.2 / 175.8 against 194.98 / 172.20) and gave gl8's I-miss saving about 1.7 times its worth. Least squares over the four (build, window) pairs, with PspProf's counts of those steps (gl7/model deckon, inon; sbg/model ndon, ndion):

  ms = (1.647 x instructions + 68.2 x I-misses + 85.9 x D-misses) / 333,000

fits the four within 1% (127.07, 107.02, 194.68, 172.69 ms). Checks outside the fit: 68 cycles an I-miss is the hardware probe's 213 ns for a jump to a code line not in the cache (every gl log: "probe icache: jump to a new line ... 223.3 ns, in cache 9.8 ns"); 86 cycles a D-miss lies between the probe's clean line (185 ns, 62 cycles) and dirty line (336 ns, 112); A-D in gl7 (instructions -1.31 M a step inside, -1.01 M on the deck, misses almost unchanged): the fit -6.68 / -4.80 ms, the PSP -6.97 (each all-off window against its on neighbours) / -4.62, the old weights -4.2 / -2.7; the codec route's window (gl7 g7cod -> sbg ndcod) the fit -10.7%, the PSP's story calls -10.4% / -10.3%, the old weights -17.8%. The 1.65 cycles an instruction is this mix's average (stalls, multi-cycle FPU and VFPU work, uncached and bus waits while the GE draws): a saving of simple instructions is worth less, of stalling loads more [I]. `profcat_fit.py` (the scratchpad; profcat.py with these weights, FITW=a,b,c) prints each part's three shares.
**What it changes [C model, fit]:** gl8's step is mostly instructions: the deck 102.9 ms of 172.7 (60%; I-misses 31.1, D-misses 38.6), inside 59.2 of 107.0 (55%; 24.1, 23.7). By part (deck / inside): game code 48.6 / 21.0; the translator 30.6 / 19.5 (vm_all 8.6 / 5.1, ud_new 5.1 / 3.3, gif_qw 3.6 / 2.5); the converters 27.5 / 20.0 (instructions 16.1 / 10.7, I-misses 7.0 / 6.1; xc_call 7.1 / 5.5, vc_p06 6.1 / 5.3); the walk 13.2 / 7.5 (io_flush 12.2 / 6.9); the runtime 12.1 / 5.7; the texture map 9.3 / 9.6. Lane 2 with the fit (sb2p's dumps, §11.24): lanes off -> 1 + 2 deck 201.3 -> 190.6 ms (-10.7, -5.3%), inside 131.2 -> 120.5 (-10.7, -8.2%), lane 2 alone -6.3 / -5.9: the lanes cut instructions and D-misses, which keep their worth.
**Step c sized [C sim]** (`tools/dev/sb2/stepc_cfg.py`, `stepc_sim.c`, `stepc_trace.sh`): PspProf's instruction trace of sb2p's build (lanes 1 + 2, no PGO profile) on the indoor route, 8 windows of 10 shown steps (the deck vb 29,601, 30,101, 30,701, 31,101; inside 31,901, 32,651, 33,401, 34,151), through layout_sim's I-cache (16 KB, 2-way, 64-byte lines, LRU, empty at each window's start): A as traced, and with each df0 and P06 converter call's work held back to its transfer's end (xl_flush's entry), df0 calls first, then P06, P06's passes with their call; kept at the call: the texture lookup (B), and xc_gs (C), and df0's input and cut (D, the one step c can do). A equals PspProf's own I-miss counts in all 8 windows. Held: 15,685 df0 and 12,759 P06 calls (355 a step) and 13,677 passes; left at the call the other kinds' 8,944 (the effects); 432 transfer ends (5.4 a step), at most 571 calls held in one.

| k I-misses a step | A as traced | B | C | D |
|---|---|---|---|---|
| deck (4 windows) | 274.3 | 210.9 | 214.2 | 218.6 (-55.7, -20%) |
| inside (4 windows) | 191.4 | 136.3 | 139.7 | 142.4 (-48.9, -26%) |

D by part, the deck (inside): the converters 49.3 -> 24.0 (38.8 -> 16.8), the translator 39.1 -> 32.7 (26.7 -> 21.1), the texture map 24.4 -> 18.8 (22.9 -> 17.0), frame + batches 18.4 -> 12.2 (15.7 -> 10.3), the texture cache 13.9 -> 10.6 (13.5 -> 10.2), the GS front end 11.1 -> 7.8 (6.8 -> 4.6), the walk 21.6 -> 19.8 (12.9 -> 11.6), libc 11.9 -> 9.2 (8.4 -> 6.2); game code unchanged. The other parts gain because the converters' code stops evicting theirs call after call. With the fit: **-11.4 ms a step on the deck, -10.0 inside** [C sim, I-misses only]. Not counted [I]: the records (each held call's inputs saved at the call and read at the end: D-misses and instructions for 355 calls a step), the code step c adds, and the PGO profile and layout: a delivery build starts with fewer converter I-misses (gl8's 34.1 k a step on the deck), so its saving is smaller, about 7-10 ms [I].
**Next (recommended):** 1. the PGO profile and layout refreshed for lanes 1 + 2 (pgo_gl.sh), with the x86 and emulator checks: gl9's build, needed before any PSP build of the lanes; 2. on that build, the step c replay again and the fitted ranking by part, then decide step c (about 6% of the step on the PSP) against the instruction side (55-60% of the step); 3. gl9 described to Ffm before its delivery.

### 11.26 gl9: lanes 1 + 2 with the profile and code order made again; step c sized again on it (2026-10-31; task #107)
**Build [C]** (container /home/claude/sbh; `tools/dev/sb2/pgo_gl.sh`, §11.23's steps on sb2's tree): sb2's tree (lanes 1 + 2, §11.24) as `mkpsp.sh` makes it; gen (boot_rt.o, xl_psp.o and the twelve picture objects with `-fprofile-generate`, main_psp.o with `MGS2_PGO`; EBOOT 1b03c213); run (the indoor route with `pgo 15000 37790`, 655 s: 434 / 434 hashes = hashref); use (those objects with `-fprofile-use`, relinked: build u, EBOOT 2c04aff4); trace 0 / 1 (§11.23's windows); order seeds 1 and 2 (40,000 moves each, about 45 min each on two threads: start them with a two-hour limit); d 1: EBOOT **cb711ab0**. Text 4,029,592 bytes (gl8 3,998,476: lane 2's code), so the heap's headroom is 32 KB smaller.
**Layout [C sim]:** u's placement -> seed 1: deck trace 4.03 M -> 3.13 M I-misses (-22.4%), held out 1.23 M -> 0.94 M (-23.8%); inside 5.03 M -> 3.96 M (-21.2%), held out 1.66 M -> 1.29 M (-22.0%). Seed 2: 7.12 M fitted and 2.24 M held out, against seed 1's 7.09 M and 2.23 M. `layout_sim -new` on d's addresses gives the searched counts exactly on both traces. Functions fetched in the windows: ELF 905 (2.16 MB) on the deck trace, 751 (1.98 MB) inside; w00a 504 (1.91 MB), w01a 457 (1.63 MB).
**Exact [C]:**
- PPSSPP master with gl9's delivery files (`tools/dev/ppsspp_master_run.sh`, MGS2_EXC=0; container msm/PSP/GAME/MGS2M_gl9): to "end: end of the run"; 434 / 434 hash lines = hashref; the 199 frame digests = gl8's; lists 201,966,485 words and 17,663 frames shown, as gl7 and gl8 on the PSP; 12 of the 13 shots equal gl8's PSP shots byte for byte, and 30,998 equals gl7's master shot (its last 7 rows are the GE's own on the PSP, #68).
- PPSSPP 1.20.4 with the delivery's settings (`GL3=/home/claude/sbh TAG=sbh sh psp/gl3.sh emu hw`): the 199 frame digests, the 312 VU lines and every picture, translator and texture line equal gl8's emulator run; the 13 shots equal gl8's emulator shots byte for byte; 433 / 434 hashes: vb 37,800's ee differs (a969bc6f, reference 87acb392), the one line after gl3.sh's profiled step at vb 37,795-37,796, whose interpreter is not exact (gl8's run printed 201,966,625 list words there, gl9's 201,966,628; the exact count is 201,966,485). Without the step (`NOPROF=1 TAG=sbhn`): 434 / 434 hashes = hashref, the 199 digests, the 312 VU lines and the 13 shots as above, 201,966,485 list words.
**Model [C model, §11.25's fit]** (PspProf, 80 steps, gl7's windows; ms a step):

| | aft deck vb 30,810-30,890 | inside 33,410-33,490 | codec route 31,510-31,590 |
|---|---|---|---|
| gl8 (sbg d) | 172.7 | 107.0 | 98.0 |
| **gl9 (sbh d)** | **164.7** (-8.0, -4.6%) | **102.2** (-4.8, -4.5%) | 98.6 (+0.6, +0.6%) |

- By part, deck (inside): the translator 30.6 -> 25.5 (19.5 -> 15.9): vm_all 8.6 -> 4.1 (5.1 -> 1.5), xl_vif1_span 3.4 -> 1.9 (1.9 -> 0.8), vif_cmd 2.2 -> 1.2 (1.2 -> 0.4), with xl_lane_p06 +1.3 (+1.0) and call() +1.1 (+1.0: the lane calls' quadwords decoded when read); the walk 13.2 -> 12.1 (7.5 -> 6.6, io_flush); the GS front end 4.5 -> 3.2 (3.0 -> 2.3, gf_packed's D-misses); the converters 27.5 -> 26.3 (20.0 -> 19.4; vc_p06 6.07 -> vc_p06_rec 5.99); the texture map +0.7 (+1.1) and the texture cache +0.5 (+0.5): I-misses where this search traded (gu_area_id +0.4 / +0.6); game code 48.6 (21.0 -> 21.2).
- Instructions 102.9 -> 98.8 ms (59.2 -> 56.1), I-misses 31.1 -> 30.8 (24.1 -> 23.9), D-misses 38.6 -> 35.1 (23.7 -> 22.2). Lane 2 cuts instructions and D-misses, as on sb2p (§11.25 with the fit: -6.3 / -5.9 ms without a profile); with the profiles -8.0 / -4.8.
- On the PSP [I]: gl8's 172.20 (the deck window line vb 30,900) and 107.97 (the model's 80 steps inside) times these ratios: about 164 and 103 ms a step.

**Step c sized again, on gl9 [C sim]** (`stepc_trace.sh` on sbh/d, §11.25's eight windows; stepc_sim's variant E added: the P06 calls held, df0 at the call). A equals PspProf's own I-miss count in all 8 windows. Held: 15,685 df0 and 12,311 P06 calls, 13,100 passes; the effects' 8,944 left at the call; 432 transfer ends.

| k I-misses a step | A as traced | B | C | D | E (P06 only) |
|---|---|---|---|---|---|
| deck (4 windows) | 150.9 | 108.3 | 108.2 | 111.6 (-39.3, -26%) | 122.3 (-28.6) |
| inside (4 windows) | 112.0 | 69.3 | 69.3 | 73.4 (-38.6, -34%) | 86.7 (-25.3) |

- With the fit: D **-8.05 ms a step on the deck, -7.91 inside**; E -5.85 / -5.19 [C sim, I-misses only]. On sb2p without a profile (§11.25) D was -11.4 / -10.0: the profile and the layout already took about a third of the deck's target and a fifth of the inside's.
- D by part, the deck: the converters 26.8 -> 11.0, the texture map 14.5 -> 8.8, the translator 18.0 -> 13.8, frame + batches 7.6 -> 3.9, the texture cache 7.9 -> 5.5, the walk 8.1 -> 6.1, libc 5.4 -> 3.5, the GS front end 3.0 -> 1.6, the glue 2.8 -> 1.6; game code 43.8 -> 43.2.
- Not counted [I]: the records (each held call's inputs saved at the call and read at the end: instructions and D-misses for about 350 calls a step) and the code step c adds.
- The trace's game [C]: each PspProf window runs PPSSPP's interpreter, which is not exact, so the game leaves the reference after the first window (this run's hashes from vb 30,500, sb2p's from 30,000). Windows 1-7 are each run's own game: the deck windows' converter calls equal sb2p's run window for window, the inside windows have fewer P06 calls (5,613 against 6,062 in the four; df0 the same). Within one trace A to E compare the same steps. A per-window replay from boot (8 runs) would give each window the exact game, if a decision needs it.

**What it says [C model, fit; I where marked]:**
- gl9's step is mostly instructions: the deck 98.8 ms of 164.7 (60%), inside 56.1 of 102.2 (55%). The largest single consumers of instructions (deck / inside, instruction ms): io_flush, the walk itself, 8.0 / 4.0 (1.62 M / 0.80 M instructions a step), the compiled VU0 box cull vupv_660a267e_0 4.4 / 0.9 (game logic), xc_call 3.8 / 2.6, ud_new 3.6 / 2.2, vc_p06_rec 3.4 / 3.1, gif_qw 2.8 / 1.9, vc_vu_clip 2.6 / 0.4, vm_all 2.4 / 0.9 (the effects' whole-memory decode), xl_vif1_span 1.7 / 0.6, gx_batch 1.6 / 1.6, call 1.4 / 0.9.
- Step c's target is now about 8 ms a step on both (5% of the deck's step, 8% inside), before its own costs: with records of a few hundred instructions for about 350 held calls a step, about 7 ms net [I]. Variant E (the P06 calls only, the simpler half) about 5.5 / 5 ms before costs.
- A lane for the effects (lane 3, roadmap order) would end vm_all (4.1 / 1.5 ms) and part of the walk's and the span front's work for their 144 / 70 calls a step [I: not counted yet].
- The codec calls did not gain (98.0 -> 98.6 ms, +0.6%): io_flush +0.68 ms (+105 k instructions a step; lane 2's test at the walk's tags [I]) and the texture map's I-misses +0.7 (this search's trades), against small falls elsewhere; the faces (P07) have no lane.

### 11.27 After gl9: the next speed step ranked (2026-10-31; task #108)
**gl9's step [C model, §11.25's fit]** (sbh/model, 80 steps each): the deck 164.7 ms = instructions 98.8 + I-misses 30.8 + D-misses 35.1; inside 102.2 = 56.1 + 23.9 + 22.2; the codec route 98.6 = 61.8 + 16.2 + 20.6.

**The walk line by line [C model]** (io_flush: deck 11.13 ms, 1.62 M instructions a step; inside 6.03; codec 8.76. Scratchpad `fnlines.py`, `fnpcs.py`, `fngroup.py`: PspProf's per-PC counts mapped through boot_rt.o rebuilt with -g, whose io_flush code is byte for byte d's). A deck step: 3,257 register writes reach io_flush, 511 start a DMA (446 scratchpad copies, about 48 VIF0, 6 VIF1 chains, 6 GIF); the VIF1 chains walk 7,083 tags, 405 of them whole lane groups (217 df0, 188 P06) and 6,679 one by one (11,104 VIF codes, 5,258 DIRECT words); lane_p04 is tried 1,153 times and lane_p06 936 times for those 405 groups. On the codec route both are tried 1,866 times a step for 6.5 groups (the faces' groups start like theirs).

| ms a step (deck / inside / codec) | |
|---|---|
| lane tests (the test at each tag, lane_p04, lane_p06, lane_ptr) | 2.65 / 1.81 / 1.76 |
| tag loop and TTE words | 2.77 / 1.21 / 2.70 |
| per-word walk (VIF codes, DIRECT words) | 2.68 / 1.22 / 2.52 |
| register-write dispatch (io_flush's own lines, io_at) | 1.07 / 0.63 / 0.85 |
| VIF0 (the MPG compare: 85 k / 63 k / 17 k instructions) | 0.98 / 0.57 / 0.12 |
| scratchpad copies, DMA start | 0.97 / 0.59 / 0.81 |

**The options [C for the parts they reach, I for the savings]:**

| | deck | inside | codec | what it takes |
|---|---|---|---|---|
| **step c** (df0 and P06 work at the transfer's end, §11.26's variant D) | -8.05 before its costs [C sim]; about -5 to -7 net | -7.91; about -5 to -7 | 0 | records: P06 about 600 bytes a call (the 27 VU quadwords it reads besides its streams, its GS state, texture and passes): about 0.6 / 0.45 ms; df0's batch building waits on its vertices (about 270 KB a deck step): about 1-1.5 ms; the per-call digests compared by content; a new x86 tree |
| **exact trims** | about -3.5 | about -1.3 | about -2 | one first match for both lanes (on the codec route nearly every try fails), a table for the register writes, DIRECT data a quadword at a time, the MPG compare four words at a time, and a fast path in vc_vu_clip (the effects' vertices: 2,713 a deck step at 193 instructions each, with 16 VU float clamps a vertex) |
| **lane 4, the codec faces (P07)** | 0 | 0 | vm_all 5.65 + spans 2.43 + vif_cmd 1.52 [C]; about -5 to -7 | the faces' packet builder found first (§11.20) |
| **lane 3, the effects** | vm_all 4.08 at most [C]; about -3 to -4 | 1.53; about -1 to -1.5 | 0 | P12d910's layout (§7) |

- The largest instruction pile is the game code (27.8 / 11.2 / 11.6 ms of instructions), the roadmap's logic item; the compiled VU0 box cull vupv_660a267e_0 alone is 4.4 ms on the deck (speed_notes' native candidate).
- Moving the converters' vertex math to the VFPU would cut more instructions, but the picture's float bits would no longer equal the PC's: not exact, so not proposed.

**Recommendation:** step c and the exact trims for the next speed build, each switchable and timed in turns on the PSP: about -8 to -10 ms a deck step and -6 to -8 inside [I], on the two gameplay areas; then lanes 4 and 3, so the faces and the effects join step c's batching. Asked Ffm on a card (2026-10-31); no PSP build before gl9's run is read.

### 11.28 The exact trims (2026-10-31; task #111; Ffm's pick "Batching + trims", 01:26)
Tree /home/claude/sb3 = sb2 (gl9's picture code) + `tools/dev/sb3/sb3_trims.diff`; the scripts and the switches in `tools/dev/sb3/README.txt`. Each trim leaves the walk's, the translator's and the picture's results as they were:

1. **One lane tried, not two** (OPT_LANE_MATCH 0x800000; runtime/ee_io.h vif1_chain_fast). Every group lane_p04 takes has STCYCL 3 / 1 in the TTE word of the tag after its first material (t[10] when t[3] is the constant block's UNPACK 0x6c188000, else t[6]); every group lane_p06 takes has 1 / 1 there; both need 112 bytes of room at least (seven quadwords), so with less neither is tried. On success both lanes read their constant block the same way (lane_p04's q0 = t[3] == 0x6c188000 given t[2]'s STCYCL 1 / 1; lane_p06's adds a REF of 24 quadwords, which lane_p04 also requires of that tag), so the index is right for whichever lane can succeed. The lanes have no side effects before they accept (lane_ptr is a pure look-up): skipping one that cannot accept changes nothing.
2. **io_flush without the DMA start inside** (no switch; io_dma_start noinline). With the DMA start (and the VIF1 chain walk) inlined, io_flush saved and restored ten registers on every EE register write (240-byte frame), and 2,746 of a deck step's 3,257 writes start no DMA; now io_flush is 732 bytes with no frame on those paths and io_dma_start 10,220 (PSP objdump, plain -O2). The same code runs in the same order.
3. **DIRECT data a quadword at a time** (OPT_DIRECT_QW 0x1000000; vif1_fast_words). When a DIRECT quadword starts (qi 0) with four words of the command and of the run left, gif_qw gets the four words from the data itself: the call the word-by-word path makes after its fourth word, with the same bytes. qbuf is not read while qi is 0 (and is host state, outside the hashed RAM).
4. **The MPG compare eight words at a time** (OPT_MPG_CMP 0x2000000; vif0_run): up to the block of eight that differs, then the pairs as before, so k (the first differing pair, or the end) is the same.
5. **The effects' clip positions without needless clamps** (OPT_CLIP_FAST 0x4000000; ge/vu1conv.h vc_clip_in / vc_vu_clip_nc, ge/xconv.c xc_fpos, XcHost.clipfast). When the call's 16 matrix values (once a call) and the vertex's 3 position values are each 0 or within 2^-28 .. 2^60 in size, every product is 0 or at least 2^-56 and every sum 0 or at least 2^-125 (a sum of two floats is a whole multiple of the finer one's last bit: 2^-79, 2^-102, 2^-125 after the three sums), and nothing reaches 2^122: no clamp of vc_vu_clip changes a value, and the same IEEE single steps in the same order (-ffp-contract=off) give the same bits on x86 and on the PSP. Otherwise the vertex takes vc_vu_clip.

**x86 checks [C]:** the three routes from boot with the trims on, and again with the configurations taking turns every 100 vblanks (all on, all four off, each one off alone: MGS2_OPT_CYCLE=0:100:ffffffff,f87fffff,ff7fffff,feffffff,fdffffff,fbffffff): every state hash (indoor 378, deck 252, codec 400) equals gl9's x86 logs (sb2 x1/out_i3n.log, x1d/out_d3n.log, x1c/out_c3n.log) and the routes' hashref files, every "picture: vb" digest (17,664 / 11,766 / 18,931 frames) is the same, and so are the VIF command totals and every other line but the timings. The effects' vertices: indoor 15,120,048, deck 6,127,416, codec 10,975,000; none of them needed the clamps.

**The model [C model, §11.25's fit, no PGO]:** the PSP model build /home/claude/sb3p/d (sb3's objects on gl7's build, as sb2p is for sb2: neither has a profile for the picture code) on gl9's three windows, 80 steps each (PspProf: deck 30810-30890 and inside 33410-33490 on the indoor route, the codec route 31510-31590), against sb2p (gl9's code, same build way; its codec window run tonight, s2cod) and against sb3p with the four switched trims off (cfg MGS2_OPT=f87fffff: trim 2 alone):

| ms a step | gl9's code (sb2p) | sb3, the four off | sb3, all on | change | of it, instructions |
|---|---|---|---|---|---|
| deck | 190.6 | 190.0 | 187.1 | -3.5 (-1.8%) | 101.4 -> 98.3 |
| inside | 120.5 | 120.3 | 119.4 | -1.1 (-0.9%) | 57.8 -> 56.8 |
| codec | 116.3 | 115.4 | 114.2 | -2.1 (-1.8%) | 64.4 -> 62.5 |

By trim, in instructions a step (deck / inside / codec; the windows' per-PC counts, `fncmp.py`, and the walk's lines through a -g build whose io_flush and io_dma_start code is the model build's byte for byte, `fnlines.py`, `fldiff.py`, scratchpad):
- trim 2: io_flush 1,797 k -> 1,656 k with io_dma_start (-142 k / -62 k / -149 k: the frame's saves and restores);
- trims 1, 3, 4 (io_dma_start, off -> on): -191 k / -97 k / -220 k; on the deck about 94 k of it DIRECT (the word path's two lines -80 k, the quadword line +11 k), 22 k the MPG compare (85 k -> 64 k), the rest the lane tests (lane_p04 and lane_ptr less, the new test's three lines +28 k);
- trim 5: vc_vu_clip's 522 k / 74 k / 0 go, xc_call grows by 244 k / 32 k (the inlined fast form): -279 k / -42 k / 0 (the codec window has no effects).
Together -612 k / -201 k / -369 k instructions = -3.0 / -1.0 / -1.8 ms at 1.647 cycles each. The rest of each change is misses, and those moved both ways with the placement (the walk's D-misses +0.5 ms on the deck, the converters' I-misses +0.8): a build without the profile places code and data at random, so only the instruction part is firm until the PGO + layout refresh.

**Next:** the batching (step c, task #112) in the same tree, then gl10 = gl9 + trims + batching with the profile and code order made again (`tools/dev/sb3/pgo_gl.sh`), the PSP timing the batching and the trims (as a group) in turns. Ffm went to sleep at 01:50 (2026-10-31); the batching starts on their word.

### 11.29 Step c, first part: lane 2's P06 calls held to the end of their transfer (2026-10-31; task #112; Ffm's pick "Batching + trims", 01:26)
Tree /home/claude/sb4 = sb3 (§11.28) + the batching (+ §11.30's thinning, off by default); the diff and scripts in `tools/dev/sb4/` (README.txt there). Switch OPT_XL_STEPC (0x8000000, on by default; MGS2_OPT=f7ffffff turns it off). §11.26's variant E: the P06 calls of lane 2 (their vertices in the game's data, XlCall.rec) are converted at the end of their transfer (xl_flush, which the walk calls before the game can change that RAM; the host's callback xl_on_flush), one after another, so the converter's code stays in the I-cache; everything else runs at the call as before. df0 (variant D's other half) is a later, separate step.

**What stays where it was [design]:** the order of the frame's batches, every batch's content, the texture table and its order, the frame's depth range. Each held call takes its batch slot at the call (gb_slot: the batch table as gb_xform_in would leave it, the current through batch ended) and each drawn pass at the pass; at the call it also does what xc_call does before the converter, in the same order: St.calls, xc_gs, the texture look-up (gl_tex: the frame's table, the memo); each drawn pass its xc_gs and look-up at the pass, after the translator applied the pass's A+D packet. The GlCount counts are made at the call and the pass (conv, lit_calls, lit_passes, lit_nopass), since the call's outcome is known there (below).

**What moves: the data block's layout.** A held call's blob lands at the end of its transfer, after the blobs of the calls that came later, so batches' data offsets can differ (gl_digest and the list digest hash offsets). The content check is a new digest, gl_digest_c (x86): per batch its fields without first and its data by kind (XFORM: GerXform, vertices, strips, indices; XFORMF: GerXform, vertices, polygon vertices, classes, fans; LIT: GerLit, its GerXform, vertices and strips), the through vertices, the textures, the depth range and map. The depth range does not depend on the order: one projection's Z24 is weakly monotone in w, so its min and max are those of the extreme w whatever the calls' order. (On the three routes the offsets came out the same as well: below.)

**When a call is held [design]** (xc_hold_test, no side effects; else the call runs as before): lane 2's call (c->rec lane 2), a P06 entry, not an MSCNT continuation, vc_p06_exact off; the header's count n 1..64; the streams vc_p06_rec checks (A, B kind 2; C kind 4; D on pass 2; F for the factor; each with n vectors at least); ROW z 0x1000 (else the call would be bad input, -6); no ADC from the header (XYZF2 w) and a vertex i >= 2 without ADC in stream B's w (so the call has a strip: vc_strips); the frame's room: a free record (else the held calls run first), batch slots for the call and its drawn passes, and gb_left - pend >= round64(bound), bound = sizeof GerXform + np sizeof GerLit + 76 n (xc_lit's own -5 test), pend the held calls' bounds rounded to 64. With these the held call converts (xc_lit returns 1) at the flush, and passes 2 / 3 are drawn exactly when header y has 0x10 / (0x600 and not 0x100): xc_lit's REC path gives uv2 > 0 iff 0x10 when ROW z is 1.0, and p3 > 0 iff that.

**The room [design]:** a blob takes round64(its size) of the data block whatever its place, so gb_left after any set of blobs is the same in any order. A held call's original cap was at least gb_left - pend >= bound (no -5); a converter call that is not held runs only when gb_left - pend >= GL_XCAP (else the held calls run first), so its cap is GL_XCAP both ways; afterwards gb_left >= pend still. At the flush each held call gets cap = min(gb_left, GL_XCAP) >= bound.

**The record [design]** (XcHeld, ge/xconv.h; GL_HOLD = 256 of them, 64-byte aligned; the cfg line MGS2_GL_HOLD sets how many may be in use, 1..256): entry, top, header y, the fog colour (xc_xform takes the colour, not the registers: the same value), the batch slots, the GS state and texture of the call and of each drawn pass, the XlRec, and an image of the VU quadwords the converter reads besides the streams, laid out as the VU memory of a call at T 6: img[a - 2] for address a, so abs 2 .. 5 (fog, alpha and clamp, the viewport) in img[0 .. 3] and T + k in img[4 + k] for k 0 .. 6, 8 .. 0xb, 0x14, the skinned entries' 0xc .. 0xf, and pass 3's 0x10 .. 0x12 when it is drawn (hi_io: runs of quadwords, no address list). At the flush the converter reads the image in place (XlCall.vm 32 bytes before img: addresses 0 and 1 are the record's own fields, which P06 never reads; top 6), with no copy. A call that draws pass 3 also reads T + 0xfe and 0xff (img[25], img[26]), 0x100 quadwords from T: its image goes into a scratch VU memory at T 6 first.

**A call whose vc_p06_rec says -8 at the flush** (the constants large, below zero or unlit, pass 3's light ramp, a negative UV; none in the three routes' 2.07 M lane-2 calls, §11.24): the original decoded the VU memory and ran vc_p06 on it, which the later groups have since overwritten. The scratch memory is rebuilt instead (sv_rebuild): the streams' vectors at T + 0x100 + s + 6 i, the call's own T (A, B, F V4-16: four halfwords sign-extended; C, D V2-16 masked: x, y, ROW z, y), then the image's quadwords over them (the real memory's last writes: the header's UNPACK comes after the streams), and vc_p06 runs on it. Checks: MGS2_SB_CHK (each held call converted both ways, compared), MGS2_SB_FORCE (every held call through the rebuild), MGS2_SB_POISON (the scratch memory's other quadwords, and the record's image before it is filled, 0xa5), MGS2_SB_P6EXACT (vc_p06_exact: every call through p06_any).

**x86 checks [C]** (the routes from boot; `cmp4.py`): step c on (the default) against off (MGS2_OPT=f7ffffff) and against sb3 (§11.28's logs: x1/out_ion.log, x1d/out_don.log, x1c/out_con.log), the record as above (sb4 x3 logs; the first form, which copied the quadwords with an address list into a scratch memory at every flush, gave the same, x1 logs):

| route | frames | P06 calls held | flushes: transfer's end / records full / room / frame's end | content digests on = off | picture lines (list, frame digests) on = off = sb3 | state hashes |
|---|---|---|---|---|---|---|
| indoor (to vb 37,800) | 17,664 | 1,426,658 (every lane-2 P06 call) | 7,664 / 3,921 / 0 / 0 | 17,664 equal | all equal | 378 equal |
| deck (to vb 25,200) | 11,766 | 265,586 | 2,928 / 741 / 0 / 0 | 11,766 equal | all equal | 252 equal |
| codec (to vb 40,000) | 18,931 | 373,221 | 8,217 / 903 / 0 / 0 | 18,931 equal | all equal | 400 equal |

No held call failed to place and no pass came after its call ran (both counters 0). The list and frame digests are equal too, not only the content: on these routes a transfer with P06 groups has no other converter call after them, so the held blobs land where they did. Every other log line is the same but the new step c line. Rare paths: the first form on the indoor route, MGS2_SB_CHK=1 1,426,658 checked, 0 differing, and MGS2_SB_FORCE=1 with MGS2_SB_POISON=1 the picture and hashes as above; the record's form on the codec route with MGS2_SB_FORCE=1 MGS2_SB_POISON=1 (x3/out_c5fp.log): all 373,221 held calls through the rebuild, content digests, picture lines and the 400 hashes equal.

**The model [C model, §11.25's fit, no PGO]** (PspProf, 80 steps; model build /home/claude/sb4p/d2: gl7's build with this tree's picture objects; step c on and off in the same build):

| ms a step | step c off | on | change | instructions | I-misses | D-misses |
|---|---|---|---|---|---|---|
| deck 30,810-30,890 | 187.1 | 180.7 | -6.4 (-3.4%) | 98.4 -> 99.1 | 53.7 -> 47.2 | 35.0 -> 34.4 |
| inside 33,410-33,490 | 119.1 | 114.2 | -4.9 (-4.1%) | 56.9 -> 57.4 | 40.8 -> 35.7 | 21.4 -> 21.0 |
| codec route 31,510-31,590 | 114.4 | 113.9 | -0.5 | 62.7 -> 62.7 | 30.9 -> 30.6 | 20.8 -> 20.6 |

(The first form gave deck -4.0, inside -3.0, codec -0.3 on build sb4p/d; the record's form drops its scratch memory fill at each flush and its address list, sv_put +0.95 and hq_addr +0.47 ms on the deck then; the rest of the gap is the two builds' placement [I].) By function on the deck (`fndelta.py`, -6.41 ms: instructions +142 k, I-misses -31.7 k, D-misses -2.4 k a step): the converter's and the walk's misses fall (vc_p06_rec -1.37 ms, xc_lit -1.21, xc_call -0.68, tk_get_cl -0.61, vc_xform -0.55, gu_area_id -0.44, xl_lane_p06 -0.43, xc_state -0.41, gu_clut_load -0.36, gb_xform_in -0.34, call -0.31, gif_qw -0.21 ...) while the hold costs 2.9: xc_hold +1.42 (68 k instructions, the GS state and texture look-up it took over from xc_call, the record's copies and its store misses), xc_hold_test +0.58, xc_held_run +0.55, gb_slot +0.34. The trace replay of §11.26 had sized variant E at -5.85 / -5.19 ms before its own costs (on gl9's build with its profile).

**How many calls to hold [C model]** (cfg MGS2_GL_HOLD; build sb4p/d3 = the record's form + §11.30's code, its switch off; the deck window, no shots before it, as the three runs must match: a shot before the window moves it by 0.2 ms): 256 (the default) 179.1 ms a step, 32 179.5 (+0.4), 16 180.5 (+1.4: instructions +23.6 k, I-misses +3.2 k, D-misses +2.3 k a step, spread over the converters, the texture look-ups and the batch code, which the extra flushes reload; the records' own misses do not fall). On the deck route (to vb 30,890) the flushes for full records go 1,091 -> 10,106 -> 21,090 while those at a transfer's end stay 4,660. So GL_HOLD stays 256 (a record is 1,024 bytes on the PSP: 256 KB); the state hash at vb 30,500 is the same in all three.

**Next:** gl10 = gl9 + the trims + step c (+ §11.30) with the profile and code order made again (`tools/dev/sb4/pgo_gl.sh`), then the emulator and master checks; the PSP times the trims and step c in turns.

### 11.30 Lighter rain and foot splash on the PSP's screen (2026-10-31; task #114; Ffm 15:04, 15:16, 15:42)
Ffm (15:04): "wen snake walks in the deck there is water splash, can we remove it or reduce it for 1/3 ? same with the rain, it to much for the psp screen, if possible also reduce to 1/3 of it and lets check out how it is"; (15:16) "lets start with the rain to 1/2 then, and then we decide if we stay 1/2 or 1/3 if realy needed"; the walking splash starts at 1/2 too; (15:42) the rain meant is "those big sheets of speckled drops close to the camera". Picture only: the game runs as on the PS2 (thinning in the game's own effect code would change its state and the hashes); the PSP draws fewer of those drops. The thin falling streaks are not these: the PS2 draws them with a VU1 program of their own (line strips) that the live picture has no converter for yet, so the PSP skips them as every call of an unconverted program (ge/gelive.h, gl_call); on the deck frame of vb 30,800 they are 32 calls, 18,206 pixels of the PS2 picture, where one sheet call alone changes 101,686 and the splash 8,297 (gsref, a call left out at a time). When the streaks get their converter they come in at the same share (said to Ffm 15:41).

**Who draws them [C]** (x86 probe on the indoor route's aft deck, vb 30,590-30,811; the actors by walking the game's lists): the sheets are the draws of the actor whose file name is d_splash_camera.c (at vb 30,800 actor 0x15b2020, act function 0x342878, level 8 of 11): four effect calls a frame (entry 0x25f0, TEX0 tbp 0x2000 cbp 0x223c, ALPHA 0x44), each 64 vertices = 16 strips of four, their vertex data at 0x15b3470, 0x15b3870, 0x15b3c70, 0x15b4070, inside the actor's DG object (actor + 0x40 = 0x15b3130, its allocator block 0x15b3130 .. 0x15b7470). The foot splash is ft_splsh.c's (actor 0x144c470, act 0x329f60, level 5): one call (64 vertices, ALPHA 0x48) in 70 of the 111 frames, data at 0x144ea10 in its object (actor + 0x860 = 0x144e790, block .. 0x144fa10). No other call's data lies in those blocks. A sheet's 16 drops spawn together, grow and fade over about four frames and spawn again in the same slots; the splash's strips move less than 300 units a frame: a strip's slot is stable, so keeping every N-th strip keeps the same drops from frame to frame (no flicker).

**How [design]** (ge/fxthin.c, ge/xconv.c; the README in `tools/dev/sb4/`):
- **Finding the blocks, once a frame** (fx_thin_scan, before the frame's first effect call is converted; only when a share is above 1): the walk of the 11 actor lists in the ELF's data (heads from 0x1b9d94, 0x64 apart, the tail at head + 0x30; an actor's +0 the next, +4 its act function, +0x28 its file name), the names read once per (act function, name pointer) pair (a 512-entry open-addressing cache, emptied when the stage image's 32-byte header at 0x1f1c00 changes or it is 3/4 full); for each actor of the two names its DG object and that object's allocator block (header 0x10 bytes before: previous, next, 0, size; accepted when the next block's previous is this one and the block is 1 MB at most). Anything that does not look right ends the walk (counted: bad).
- **Which call** (xlate.c): XlCall.src, the guest address of the data of the last UNPACK to T + 0x100 (the first vertex quadword) since the previous call; a call whose src lies in a block is thinned with that block's share.
- **Thinning** (xc_fpos): the strips are counted as vc_strips sees them (a strip starts at a vertex with ADC whose next has ADC and whose previous has not), strips 0, N, 2N ... are kept; a left-out strip's vertices skip the clip work and its triangles get class 0 (not drawn). The call's lowest alpha is still taken over all its vertices, so the passes (one or two) are those of the whole call, and a kept strip's triangles and polygons are byte for byte the ones the whole call draws.
- cfg MGS2_GL_RAIN=N (the sheets), MGS2_GL_SPLASH=N (the splash): 1 strip in N drawn, 1..8; 1 or no line: as the PS2. x86: the same names as env lines; MGS2_THIN_CHK=1 converts each thinned call again without thinning and compares.

**x86 checks [C]** (sb4/x5, the routes from boot; MGS2_SB_LANE=3, step c on):
- Switch off (no lines): every log line as sb4 before the change (indoor 17,664 frames, 378 hashes; deck 11,766, 252).
- 1/2 and 1/3: the state hashes all equal (indoor 378, deck 252); the picture differs in 1,756 indoor and 1,009 deck frames, the first at vb 8,756 (stage d00t: the bridge's rain in the opening). Indoor at 1/2: scans 5,221, actors walked 1,045,650 (about 200 a scan), names read 623, cache emptied 6; blocks: sheets 1,767, splash 1,273; bad 0, over 0; calls thinned 9,362, strips 149,792, left out 74,896. Deck at 1/2: scans 1,377, names read 284; blocks 1,013 / 1,013; calls 4,296, strips 68,736, left out 34,368.
- MGS2_THIN_CHK (each thinned call converted both ways): indoor 1/2: 9,362 calls, 0 differing; kept strips' triangles 116,752 in the same classes, left-out 116,752 class 0, kept polygons 42,673 the same and in order; 571 calls with nothing left to draw (then none of the whole call's kept triangles is drawn either). Indoor 1/3: 9,362, 0 differing; 83,196 kept, 138,660 left out, 32,335 polygons; 935 empty. Deck 1/2: 4,296, 0 differing; 57,680 / 57,680; 17,238 polygons; 133 empty.

**The PSP build in the emulator [C model, §11.25's fit, no PGO]** (model build /home/claude/sb4p/d3: sb4p/d2 + this change; the indoor route to vb 30,890, PspProf on the deck window 30,810-30,890, 80 steps; three shots before it; the same build with the cfg lines and without):

| ms a step (deck) | total | instructions | I-misses | D-misses |
|---|---|---|---|---|
| switch off (tr0) | 178.9 | 99.5 | 47.7 | 31.7 |
| 1/2 (tr2) | 178.8 | 99.2 | 47.7 | 32.0 |
| 1/3 (tr3) | 179.2 | 99.3 | 47.9 | 32.0 |

At 1/2 the CPU time is the same within 0.1 ms: the left-out strips' clip work (vc_cflags -0.15 ms, vc_plane -0.12, xc_call -0.05, vc_clip_tri -0.04) pays for the scan (fx_thin_scan +0.10 ms: 6.4 k instructions and 0.27 k D-misses a step, about 260 actors walked a frame). The GE draws half the sheets' and the splash's pixels (one sheet call alone is about 100,000 pixels of the 130,560 on the deck frame); the CPU does not wait for the GE on the deck (its frame is about 40 ms there, the step about 175), so that saves no step time today [I]. The log's line (1/2, to vb 30,890): scans 1,937, actors walked 505,069, names read 489, cache emptied 5; blocks: sheets 1,552, splash 1,058; bad 0, over 0; calls thinned 8,459, strips 135,344, left out 67,672. Each of the three runs' 371 state hashes equals the route's reference (no split; vb 30,500 ee 2419e0e670c8e124).

**Pictures** (the three runs' shots at vb 30,598, 30,698 and 30,798, before the timed window: Snake crossing the aft deck): pixels that differ from the switch-off shot, of 130,560: 1/2 74,213 / 105,326 / 94,880, 1/3 100,287 / 108,221 / 104,478 (the sheets cover most of the frame). The PNGs (all, 1/2, 1/3 and side by side, 1:1) are in Ffm's PC folder, SHOTS\rain_and_splash, with a README. At 1/3 the CPU time is 179.2 against 178.9 (+0.3: placement noise of this size is usual between runs whose work differs [I]). gl10 carries 1/2 (cfg MGS2_GL_RAIN=2, MGS2_GL_SPLASH=2): Ffm chose "Half" on the card (2026-10-31 16:55) after the pictures and confirmed it at 16:58 ("lets stay 1/2 if we need more perfomance we go 1/3"); told that 1/3 saves no step time today (the table above); 1/3 stays a two-line cfg change.

### 11.31 gl10: trims + step c + the lighter rain, profile and code order made again (2026-10-31; task #115)
The build of §11.28 + §11.29 + §11.30 (tree sb4) with the picture code's PGO profile and code layout made again (`tools/dev/sb4/pgo_gl.sh`, its training and trace runs with MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2, the delivery's setting); details, md5s and every check in psp_harness.md "Build gl10".
- **Layout fix:** `tools/layout_order.py` did not list ge_fxthin.o, so fx_thin_scan was left to the linker and the relinked build's simulated I-misses were 2-3% above the search's; listed now, `layout_sim -new` equals the search exactly (deck 2,785,288, inside 2,909,352; gl9's search 7,121,132 for both traces, gl10's 5,694,640).
- **Exact [C]:** x86 with the delivery's turns (everything on, step c off, everything on, the trims off) on 3 routes: content digests, picture lines and state hashes equal; PPSSPP master and 1.20.4 to the end: 434 / 434 hash lines, gl9's 199 digests and 12 of its 13 shots byte for byte (master = gl9's PSP shots), the 13th and the three new ones the rain at 1/2.
- **Model [C model, §11.25's fit, everything on, rain 1/2]:**

| ms a step | gl9 | gl10 | change |
|---|---|---|---|
| aft deck 30,810-30,890 | 164.7 | 154.0 | -10.7 (-6.5%): I-misses -6.6, D -2.5, instructions -1.6 |
| inside 33,410-33,490 | 102.2 | 97.1 | -5.1 (-5.0%): I-misses -5.3 |
| codec route 31,510-31,590 | 98.6 | 98.7 | +0.1 |

Most of the deck's saving is I-misses across the picture code (converters -2.4, translator -1.2, the texture map -1.3, texture cache -0.9): step c runs each converter over many calls in a row, and the new order packs that code tighter [I]. The codec route has no P06 calls to hold and its window is not in the layout's traces. PSP estimate [I]: about 49.7 min for the run (gl9 52.1), inside about 98.8 ms a step on the all-on windows (gl9 103.98), the waiting deck about 158 (gl9 169.26).

**On the PSP (gl10 run 1, files 2026-10-31 19:21; `psp_harness.md` "Build gl10" run 1) [C]:** to the end, exact (434 hash lines, 311 vuh, 199 picture lines = gl9's; picture totals = PPSSPP master's), in 49.6 min (est. 49.7). On gl9's all-on windows: the waiting deck 169.26 -> 160.16 ms a step (-5.4%), the deck walk -7.0%, inside 103.98 -> 96.34 (-7.3%), the calls flat. Against the model's -6.5% / -5.0%: the deck a little under, inside over. Step c alone (median against its all-on neighbours): the deck 6.0 ms, inside 7.5 (the model without PGO 6.4 / 4.9); the four switched trims: the deck 4.6, inside about 1.2, the calls 1.4. The lowest heap headroom fell to about 612 KB (gl9 936; step c's 256 KB of records): the next lanes must budget their memory.

### 11.32 Lane 4, the codec faces (P07): design (2026-10-31; task #117)
**The builder [C]** (`re/p07_builder.md`): the DG pass 0x137730 (passes 5 and 8 via 0x138ac8 / 0x138ae0), per object 0x136f38, one fixed 24-quadword slot (20 tags) per face call; a pass run is one DMA chain: a 15-qw header (P07's program REF, abs 0..11), the page change, an 18-qw object block (abs 16..31), the slots, the end. The streams, descriptors and materials are kept model data that the slots REF; the bones are double-buffered in the object and REF'd too. 309 calls a frame in 3 pass runs (168 + 4 + 137). The builder itself costs 3.32 ms a codec step [C model]: game code, which the lane does not touch.

**The lane, as lanes 1 and 2 (§11.21, §11.24):**
1. **Recognition** (`runtime/ee_io.h` `lane_p07` in `vif1_chain_fast`, tried first when the TTE word at a tag is STCYCL 7 / 1, which neither lane 1 (cl 3) nor lane 2 (cl 6) takes): the 20 tags of a slot as `p07_builder.md` §3.4 gives them, each REF taking exactly its UNPACK's data or CNT 0 with STCYCL + NOP; the CNT 4 with T + 0..3; the three material slots; the FLUSHE / NOP; the eight bone slots (V4-32 to abs 0x20 + 4i, no FLG); the MSCAL. No IRQ bit, no END / RET / CALL inside, every piece in one run of RAM or scratchpad. The lane takes consecutive slots in one go while the next tag starts another slot. Anything else (the header, the page change, the object block, the end) stays with the generic walk.
2. **State as the generic walk leaves it:** STCYCL, the last command, TOPS flipping at each MSCAL (FLG UNPACKs land at TOPS 0x040 / 0x220), the x86 command counters, the census. `ge/xlate.c` `xl_lane_p07` registers every UNPACK in own[] as `xl_vif1_span` would, so the VU memory image stays the same lazily (the persistent quadwords: the bones at abs 0x20..0x3f, the materials at T + 0xe8..0xff per VU half, and the padding vector of an odd vertex count).
3. **Direct reads** (`XlRec` as lane 2): streams A, B, C, D (V4-16 at T + 0x100..0x103, cl 7) and F (V4-8 unsigned at T + 0x105 / 0x106, cl 7 wl 2) are read from the game's data when each was sent with at least the call's vertex count (T + 0 x); `vc_p07_rec` shares `vc_p07`'s arithmetic (the int-to-float steps at the same place), so its floats are the same bits. call() decodes the other quadwords P07 reads: T + 0..3, abs 2..4 and 7..11, abs 16..18, abs 24..31, the bones abs 32..63, and on passes 2 / 3 T + 0xe8..0xff. Any unexpected input sends the call back to the decoded path (return -8, as lane 2): the cull flag T + 1 x not 0, entry +0x628, prims 0x2d / 0x2c, stream E sent.
4. **Switches:** x86 `MGS2_SB_LANE` bit for lane 4 and `MGS2_SB_CHK` (every lane P07 call also against `vc_p07` on the decoded VU memory); PSP g_opt `OPT_XL_LANE4` 0x10000000 (off = gl10's path).
5. **Memory budget:** no records held (a pass run's slots are all P07 calls already, so step c's batching adds nothing here); the per-call `XlRec` only. Limit: under 16 KB of new heap and about 12 KB of new text, against gl10's lowest headroom of 612 KB on the PSP.

**Checks [to do], lane 4 on against off, from boot (x86 tree sb5 = sb4 + lane 4):** indoor route to vb 37,800 (it holds the opening's 1.78 M P07 calls and both story calls), deck tutorial to 25,200, codec route to 40,000; all frame digests, the hash lines and the vuh lines equal, every log line equal but the lazy decode's counts; with `MGS2_SB_CHK` 0 lane calls differ; the per-call digest file on a codec window byte for byte; the share of P07 calls the lane takes on each route (the census), with every one it does not take explained. Then the model on the codec window vb 31,510-31,590 (and the deck and inside windows, which should not move), lane off against on. §11.27 expects about -5 to -7 ms a codec step of vm_all 5.65 + spans 2.43 + vif_cmd 1.52 [I].

**Later, not in this lane:** the builder's 3.3 ms could be skipped on steps that are never shown (the reduced pipeline, speed_notes §33), only after the from-boot read check shows the game never reads the packets it did not write (§34).

**Built and checked (2026-10-31; x86 tree `/home/claude/sb5` = sb4 + lane 4, saved as `tools/dev/sb5/`: `sb5.diff` against sb4's 10 files, the scripts, README):**
- **As built [C]:** items 1-4 as designed. `lane_p07` is tried where a tag's first TTE word is STCYCL 7 / 1 (lanes 1 and 2 test STCYCL 1 / 1 first, so the tests never overlap) and takes slot after slot while the next tag is STCYCL 7 / 1 again; any failed test leaves the tag to the generic walk (x86 `lane7_miss` counts the first failed test). `call()` decodes T + 0..3, abs 2..11, 16..19, 24..63 and T + 0xfe, 0xff (`vm_dec`, a pair of quadwords at a time where it can); the materials at T + 0xe8, 0xf0 and 0xf8 are read, as before, by the A+D kicks of P07's rule and of `passes()` (`kick_vm`, through `vmq`, which decodes what it reads). `vc_p07_rec` is `p07_main` with the vertices taken from the streams (`P7Rec`: halfwords sign-extended, F's bytes picked out, the same float steps in the same order); it returns -8 (the call goes to `vc_p07` on the decoded VU memory, `xc_lit`) when a stream A, B, C (and D on passes 2 / 3) is not V4-16 with the header's count of vectors, F is not V4-8 unsigned with twice that, E was sent, the vertices would wrap the VU memory, or any test sends `vc_p07` itself to `p07_any` (flags 0xd04, the cull word T + 1 x, the bone count, a constant of 2^20 or more, a negative colour, a vertex outside the fast form's ranges).
- **Checks [C], lane 4 on against off, from boot** (`chk2_x86.sh`: three runs side by side, off `MGS2_SB_LANE=3`, on 11, on with `MGS2_SB_CHK=1`; rain and splash 1/2 as gl10; `MGS2_HASH_DENSE=15700:16010` as the PSP cfgs): indoor route to vb 37,800: all 17,664 frame digests the same, 685 / 685 hash lines = `prof/hashref_x86_in_h2.txt`, the 311 vuh lines the same in all three runs; deck tutorial to 25,200: 11,766 frames, 559 / 559, 311; codec route to 40,000: 18,931 frames, 707 / 707, 311. Every other log line the same (the translator's totals, the census, the UNPACK kinds, the walk's VIF1 command counts, the textures among them) but the mode line, lane 4's own totals line, the host's timings and the lazy decode's counts: quadwords decoded at flushes 271.7 M -> 119.1 M indoors, 138.2 M -> 50.2 M on the deck, 388.1 M -> 76.9 M on the codec route (flushes 2.20 M -> 1.11 M, 0.97 M -> 0.34 M, 2.83 M -> 0.59 M); decoded when read 155.7 M -> 180.0 M, 109.0 M -> 126.0 M, 126.8 M -> 167.3 M; in all -30%, -29%, -53%. The same runs without the dense hashes (indoor, codec): 378 / 378, 400 / 400 and the same totals.
- **`MGS2_SB_CHK` [C]:** 1,104,330 + 639,618 + 2,262,731 lane P07 calls also converted by `vc_p07` on the decoded VU memory, every output compared (the batch, pass 3's vertices, the flags, UV2): 0 differ. Lanes 1 and 2 in the same runs: 0 of 4,981,078 differ.
- **The census [C]:** every P07 call came through the lane: 1,761,366 slots indoors, 1,169,126 on the deck, 2,919,767 on the codec route, each the census's count of entry 1a31e0; the lane refused none of the STCYCL 7 / 1 tags it looked at (`lane7_miss` 0 on every route; a tag left after a run of slots is looked at again as a first slot, so it would be counted). The converter calls among them (the shown frames) read their vertices from the game's data, 1,104,330 / 639,618 / 2,262,731, except 14,268 on the deck (2.2%): those are the calls on paths the captures never took (`vc_p07` -7, flags 0x804 or the cull word or the bone count), which gl10's path also leaves undrawn (the lane-off run counts the same 14,268 "paths not seen"); the lane sends them back to the VU memory and they end the same way.
- **The per-call digest file [C]** (`ge/xldig.c`: the VU memory as read and the GS registers at each call, every primitive; no checks): indoor route vb 26,061-26,458 (the PSP cfg's digest window, Otacon's deck tutorial call: `dig2_x86.sh`), 43,771 calls of which 42,016 P07 (all through the lane) and 1,755 P06, 3,700,533 bytes byte for byte; codec route vb 31,400-31,599 (`digc2_x86.sh`, the translator from vb 31,400), 32,200 calls of which 30,900 P07 (all through the lane) and 1,300 P06, 2,718,400 bytes byte for byte.
- **Model [C model, §11.25's fit]** (build `/home/claude/sb5m/d`: `mkpsp.sh`, sb5's tree on gl7's build, the picture code plain -O2, no PGO profile and no layout, EBOOT aac5add5; `model.sh`, i.e. gl3.sh's model step with gl10's cfg lines on the route as gl10's own model runs had them: rain and splash 1/2, texture pool 2,688 KB (`dl_cfg.txt`'s `MGS2_OPT_CYCLE` would put lane 4 on in both runs, so it is left out); 80 shown steps a window; lane 4 off = `MGS2_OPT=efffffff` in the one build):

| model ms a step | lane 4 off | on | change |
|---|---|---|---|
| codec, vb 31,510-31,590 | 114.4 | 103.2 | -11.2 (-9.8%) |
| aft deck, 30,810-30,890 | 179.1 | 179.1 | -0.1 (0.0%) |
| inside, 33,410-33,490 | 114.8 | 114.9 | +0.1 (+0.1%) |

  By part on the codec window (`fitcmp.py`, off -> on): the translator 18.12 -> 11.23 ms (vm_all 5.88 -> 0.14, xl_vif1_span 2.99 -> 0.75, vif_cmd 1.73 -> 0.11; the lane's own `xl_lane_p07` 1.40 and `vm_dec` 0.50; ud_new 2.31 -> 2.32, call 1.32 -> 1.40); the walk 10.22 -> 7.18 (io_dma_start 8.56 -> 5.54: the slots' tags are not walked word by word); the converters 18.46 -> 18.37 (`vc_p07_rec` 10.04 against `vc_p07`'s 9.79: the reads of the game's data, D 1.40 against 1.28 ms); the texture parts -0.58, frame and batches -0.18. Instructions 12.74 M -> 11.37 M a step. §11.27's -5 to -7 ms was for vm_all, spans and vif_cmd: they gave -9.6 ms; the walk another -3.0; the lane's own code costs +2.2. The deck and inside windows hold no P07 call (the lane's code never runs there): io_dma_start +0.11 and +0.09 ms (+22 k and +17 k instructions a step; the lane's test of a tag's first TTE word, [I]); the rest of the step -0.19 ms on the deck (converters -0.08, textures -0.08: not looked into) and +0.03 inside. This build has no PGO profile and no code order, so its lane-off figures lie above gl10's model runs (codec 114.4 against 98.7 ms, deck 179.1 against 154.0, inside 114.8 against 97.1), mostly in I-misses (codec 30.6 against 16.2 ms) [C model]; with gl10's kind of profile and order the instruction and D-miss part of the codec saving (-6.7 and -3.0 ms) should carry over, the I-miss part (-1.5) depends on the new order [I].
- **Memory and size [C]:** the lane allocates nothing (no records held; one static `XlRec`); BSS +128 bytes. Text +19,676 bytes over sb4's model build (4,030,464 -> 4,050,140; gl10's PGO build 4,040,252): `vc_p07_rec` 16,536 (p7_loop's eight forms, bones 1-4 by passes 2 / 3, again with the streams), `xl_lane_p07` 1,112, `lane_p07` in io_dma_start +1,080, xc_lit +304, `vm_dec` 264, call +128: over the design's "about 12 KB" by 7.7 KB. Emulator heap: the model runs' lowest headroom 6,140 KB up to the codec window, lane off and on, as gl10's (g10cod 6,140); 6,136 KB up to the deck window against gl10's 6,140 (g10on); 6,040 KB up to the inside window against gl10's 6,056 (g10ion); each the same off and on, the break's highest point 8 to 12 KB above gl10's (sb5's build is the larger, [I]); the whole indoor route with gl10's delivery cfg (`gl3.sh emu hw`, `GL3=/home/claude/sb5m`; lane 4 on in all of `MGS2_OPT_CYCLE`'s settings): about 5,800 KB at its lowest against gl10's 5,816 KB (hwg10; the break's highest point 8 KB higher, 0b80d000).
- **Emulator [C]:** that hw run's 434 hash lines and 311 vuh lines are hwg10's (no split from hashref), its 199 frame digests of vb 26,061-26,459 (`MGS2_GL_DIG`) are hwg10's and the x86 twin's with lane 4 on, and its 16 shots (`MGS2_GL_SHOT`, vb 26,198 among them, in the deck tutorial's codec call) are hwg10's byte for byte. The model runs: every hash and vuh line the same off and on and as gl10's model runs (g10cod, g10on, g10ion), no split.
- **For gl11 [I]:** the exactness and the census need nothing more; gl11 wants the PGO profile and code order made again with lane 4 in (sb5's `pgo_gl.sh`, T = sb5), then gl10's checks on that build. If the 7.7 KB over the text estimate matters against the PSP's 612 KB headroom, `vc_p07_rec` could keep only the (bones, passes) forms the routes use and send the rest to -8.

### 11.33 Lane 3, the effects (P12d910's sort-pass prims): design (2026-10-31; task #118; Ffm chose "Both lanes" 21:11: gl11 carries lanes 3 and 4)
**The builder [C]** (`re/mesh_packets.md` §7, `re/pipeline_hot.md` §1.5-1.6): the sort pass 0x121c30 walks the depth buckets far to near and, per sort-object mesh, 0x12d910 writes about 10 qwords: REF the positions (V4-32 floats, STCYCL cl 3 / 6 / 12 by type, wl 1, to T + 0x100), REF the attributes (V4-16 to T + 0x101, cl 3 wl 2 or cl 12 wl 4), REF 8 of the owner's block (T + 0xe8: GIF tag, CLAMP, TEX2, TEX0, ALPHA), CNT 4 with the matrix inline (T + 0x10), REF 3 (the control quadword and the GIF tag), CNT 0 with the MSCAL from table 0x17d020 by type: 1 +0x2b60 line strips (no converter yet: the rain streaks), 2 +0x25f0, 3 +0x2870, 4 +0x3740 sprites, 5 +0x3a38. The geometry is double-buffered in the sort object (the actor fills the other set and flips +180). These are the effects: the rain sheets near the camera, the foot splash and the other depth-sorted meshes and sprites; the thinning of §11.30 picks among these calls by `XlCall.src`.

**The lane, as lanes 2 and 4:**
1. **Recognition** (`lane_fx` in `vif1_chain_fast`): a tag whose first TTE word is STCYCL cl 3, 6 or 12 / wl 1 followed by UNPACK V4-32 to T + 0x100 (lane 1's streams are V4-16 there, lane 4's cl is 7); then the five other tags of the prim exactly as the builder writes them, each REF taking exactly its UNPACK's data; no IRQ bit, no END / RET / CALL inside, every piece in one run of RAM or scratchpad. The lane takes prim after prim while the next tag starts another; anything else stays with the generic walk. Prims of an entry with no converter (+0x2b60) still go through the lane's translator part and are then treated as the generic path treats them.
2. **State** as the generic walk leaves it (STCYCL, the last command, TOPS, the counters, the census), and `xl_lane_fx` registers every UNPACK in own[] (the VU memory image stays the same lazily).
3. **Direct reads:** the positions as floats and the attributes as 16-bit values from the game's data, when each was sent with at least the call's count; `XlCall.src` exactly as the generic path sets it (the thinning must pick the same calls); the converters' rec forms share their arithmetic (and `vc_clip_fast` with OPT_CLIP_FAST); call() decodes the other quadwords they read (T + 0x10..0x15, T + 0xe8..0xef as the A+D kicks read them, abs as the rules need); anything unexpected returns -8 to the decoded path.
4. **Switches:** x86 `MGS2_SB_LANE` bit 4 for lane 3, `MGS2_SB_CHK` covering it; PSP g_opt `OPT_XL_LANE3` 0x20000000.
5. **Memory:** no records held, under 12 KB of new text (lane 4 already added 19.7 KB).

**Checks [to do]** (x86 tree sb6 = sb5 + lane 3, lanes 1, 2 and 4 on), lane 3 on against off, from boot, on the three routes: frame digests, hash lines and vuh lines, every log line but the lazy decode's counts, `MGS2_SB_CHK` 0 differ, the per-call digest file on a deck window in the rain (vb 30,800-30,900), the census share, and the thinning: its totals line equal and `MGS2_THIN_CHK` 0 differ, with the rain at 1/2 and at 1/1. Then the model: lane 3 off (`MGS2_OPT=dfffffff`) against on, on the deck, inside and codec windows. §11.27 expects about -3 to -4 ms a deck step and -1 to -1.5 inside [I], and lane 4 showed the walk's own saving on top.

**Then gl11:** gl10 + lanes 3 and 4, its PGO profile and code order made again, gl10's checks, the turns everything on / lane 3 off / everything on / lane 4 off (all-on windows = gl10's), described to Ffm first.

**Built and checked (2026-10-31; x86 tree `/home/claude/sb6` = sb5 + lane 3, saved as `tools/dev/sb6/`: `sb6.diff` against sb5's 10 files, the scripts, README):**
- **The prim as the game sends it [C]** (`probe_x86.sh` at vb 30,850 of the indoor route, on the aft deck in the rain, `prims6.py`: 489 prims, every one in this form): 10 quadwords, the 6 tags at quadwords 0, 1, 2, 3, 8 and 9: REF the positions (STCYCL cl / 1, UNPACK V4-32 n to T + 0x100); REF the attributes (V4-16 signed 2 n to T + 0x101, STCYCL 3 / 2 after cl 3 **and cl 6**, 12 / 4 after cl 12); REF 8 (V4-32 to T + 0xe8); CNT 4 with the matrix (V4-32 to T + 0x10); REF 3, UNPACK 0x6c038014: **V4-32 3 to T + 0x14..0x16, not 0x13..0x15** as `re/mesh_packets.md` §7 has it (the count at T + 0x14 is what `xc_fpos` reads as the header; T + 0x15 the tag template); CNT 0 with NOP and MSCAL. The last four with STCYCL 1 / 1. cl by type: 3 for types 1-3 (+2b60, +25f0, +2870), 6 for type 4 (+3740: one position and four attribute vectors in 6 quadwords), 12 for type 5 (+3a38: one position, four attribute vectors).
- **As built [C]:** items 1-5 as designed. `lane_fx` is tried where a tag's first TTE word is STCYCL cl 3, 6 or 12 / 1 and its second UNPACK V4-32 to T + 0x100 (lanes 1 and 2 test cl 1, lane 4 cl 7: the tests never overlap) and checks the six tags exactly as above (each REF's UNPACK count = its QWC, or twice it for the attributes; no IRQ bit; the 10 quadwords and every REF's data in one run of RAM or scratchpad), prim after prim while the next tag starts one; any failed test leaves the tag to the generic walk (x86 `lane3_miss`: the first failed test; `lane3_gen`: why the generic walk took a prim tag). `xl_lane_fx` registers the five UNPACKs as `xl_vif1_span` would and makes one call with `XlRec` lane 3 (p[0] the positions, p[1] the attributes, their decode kinds, ROW). `call()` decodes, for the two entries with a converter (+25f0, +2870), T + 0x10..0x15 and abs 2..5 (`vm_dec`; the A+D block at T + 0xe8 is read as every call reads it); the other three types (+2b60, +3740, +3a38) have no converter and go on as the generic path's calls do (the renderer's "other calls"). `xc_fpos` takes the positions and attributes from the game's data (`vc_fpos_rec`: `vc_fpos`'s arithmetic on the streams, V4-16 sign-extended plus ROW where the UNPACK was MODE 1; the "all at one point" test, the colour's w and the clipper's input read the same way through `fx_attr`) when the prim is in the cl 3 layout and both streams hold the header's count of vertices; otherwise, and on -8, `vc_fpos` on the decoded VU memory (`xl_vm_sync`). `XlCall.src` is the positions' guest address, as the generic walk sets it; `vc_clip_fast` (OPT_CLIP_FAST) is unchanged after the conversion.
- **Checks [C], lane 3 on against off, from boot** (`chk2_x86.sh`: three runs side by side, off `MGS2_SB_LANE=11`, on 15, on with `MGS2_SB_CHK=1`; rain and splash 1/2 as gl10; `MGS2_HASH_DENSE=15700:16010`): indoor route to vb 37,800: all 17,664 frame digests the same, 685 / 685 hash lines = `prof/hashref_x86_in_h2.txt`, the 311 vuh lines the same in all three runs; deck tutorial to 25,200: 11,766 frames, 559 / 559, 311; codec route to 40,000: 18,931 frames, 707 / 707, 311. Every other log line the same (`cmp6.py`; the lines its timing filter leaves out compared too, `tl6.py`: only the host's two timing lines differ) but the mode line, lane 3's totals line, five new "sb census" lines (the five entries now have lane calls) and the lazy decode's counts: quadwords decoded at flushes 119.1 M -> 23.2 M indoors, 50.2 M -> 16.8 M on the deck, 76.9 M -> 24.2 M on the codec route (flushes 1.11 M -> 0.088 M, 0.34 M -> 0.072 M, 0.59 M -> 0.100 M); decoded when read 180.0 M -> 194.4 M, 126.0 M -> 135.7 M, 167.3 M -> 179.0 M; in all -27.2%, -13.5%, -16.8%. The off runs against sb5's runs with lanes 1, 2 and 4 on: the same but lane 3's new lines.
- **`MGS2_SB_CHK` [C]:** 1,024,545 (indoor) + 268,863 (deck) + 485,437 (codec) lane 3 calls of +25f0 / +2870, each one's input (position, colour and UV quadwords of the header's count) compared with the decoded VU memory and its output (count, transform, vertices, flags) with `vc_fpos`'s on the decoded memory: 0 differ. Lanes 1, 2 and 4 in the same runs: 0 of 8,987,757 differ.
- **The census [C]** (`census6.py`): every call of the five effect entries came through the lane: 5,216,812 prims indoors, 3,990,181 on the deck, 4,578,596 on the codec route, each the census's count of the five (indoor by type 1-5: 486,617 / 1,915,479 / 961,760 / 1,357,850 / 495,106; deck 442,278 / 1,379,819 / 567,040 / 1,176,184 / 424,860; codec 470,554 / 1,709,683 / 628,448 / 1,304,119 / 465,792), 100.0% of each entry; the lane refused no prim tag (`lane3_miss` 0 on every route). The +25f0 / +2870 calls the renderer converted (the shown frames': 552,833 + 471,712 indoors) all read their vertices from the game's data, none from the VU memory (no `vc_fpos_rec_ok` refusal, no -8). The prim tags the generic walk still took, 1,358,630 / 453,678 / 721,096, are all in transfers the translator does not take from the one walk (shown frames only: on the deck 65,323 transfers of 4,691 vblanks not handed over) and make no call: with lane 3 off the census's count of the five entries equals the prim tags in the one walk exactly, with it on their generic calls are 0.
- **The thinning [C]** (`thin_x86.sh`, from boot, lane 3 off and on, `MGS2_THIN_CHK=1`): at 1/2 the thinning's totals line the same off and on (indoor: 5,221 scans, 1,767 sheet and 1,273 splash blocks; deck: 1,377 scans, 1,013 and 1,013) and the check's line the same: 9,362 (indoor) and 4,296 (deck) thinned calls converted again without thinning, differing 0 (the kept strips' triangles 116,752 and 57,680 in the same classes); frames 17,664 / 11,766 and hash lines 378 / 378, 252 / 252 the same, and the rest as above (against sb5's runs without the check: also the check's own second conversions, 2 x 9,362 calls, in the converter totals and the frames' texture lookups). At 1/1 (no thinning): frames, hashes and every other line the same off and on but lane 3's own lines.
- **The per-call digest file [C]** (`dig2_x86.sh`, `run6e.sh`): indoor route, the translator from vb 30,800 to 30,900 (the aft deck in the rain: 50 sheet and 50 splash blocks thinned at 1/2), 89,897 calls of which 24,539 lane 3 prims (14,444 of them +25f0 / +2870 converted, all from the streams), the two files the same byte for byte off and on (7,820,163 bytes, 100 vblank lines; md5 37d7c34d...), and the two runs' logs the same but lane 3's own lines (309 / 309 hash lines). **A fix in the x86 harness on the way:** batch B's first digest runs stopped (SIGSEGV, lane 3 off and on alike) in `fx_thin_scan` at the first +25f0 call: `boot/xlive_chk.c` gives the thinning its RAM pointer (`glf.ram`) from `M.mem` at a frame's start, and with the translator started late the first frame begins before the first transfer sets `M.mem`; runs from boot never meet it, and sb5's digest windows (Otacon's call, the codec) had no effect in their first frame [I]. sb6's `xlive_chk.c` sets `glf.ram` at each transfer the translator takes (the same pointer; x86 only: `psp/xl_psp.c` sets it after `M.mem`). Batches A-D ran on the twin before the fix; the indoor run with lane 3 on again on the fixed twin gave batch A's log: the 17,664 frame digests, 685 / 685 hash lines and every other line the same (the host's two timing lines differ only in their numbers).
- **Model [C model, §11.25's fit]** (build `/home/claude/sb6m/d`: `mkpsp.sh`, sb6's tree on gl7's build, the picture code plain -O2, no PGO profile and no layout, EBOOT d6f888f2; `model.sh`, gl3.sh's model step with gl10's cfg lines as §11.32's runs: rain and splash 1/2, texture pool 2,688 KB; 80 shown steps a window; lane 3 off = `MGS2_OPT=dfffffff` in the one build; `run6m.sh` ran the codec pair, `run6m2.sh` the deck and inside pairs side by side; `fitcmp.py` by part):

| model ms a step | lane 3 off | on | change | instructions a step |
|---|---|---|---|---|
| codec, vb 31,510-31,590 | 104.43 | 104.35 | -0.08 (-0.1%) | 11.384 M -> 11.374 M |
| aft deck in the rain, 30,810-30,890 | 178.68 | 171.62 | -7.05 (-3.9%) | 20.075 M -> 19.204 M |
| inside, 33,410-33,490 | 114.96 | 112.75 | -2.20 (-1.9%) | 11.649 M -> 11.411 M |

  By part on the deck (off -> on): the translator 27.64 -> 22.34 ms (vm_all 3.96 -> 0.13, xl_vif1_span 2.14 -> 0.99, vif_cmd 1.58 -> 0.50: -6.06 together; the lane's own `xl_lane_fx` 1.01 and `vm_dec` 0.14; call -0.13, ud_new -0.07); the walk 13.47 -> 11.96 (io_dma_start 11.56 -> 10.05: the prims' tags are not walked word by word); the converters 28.40 -> 28.71 (`vc_fpos_rec` 2.05 against `vc_fpos`'s 1.60: 270 k instructions a step against 232 k, and the reads of the game's data, D 0.48 against 0.28 ms; xc_call -0.10, vc_xform -0.06); textures -0.29, the GS front end -0.16, frame and batches -0.15. Instructions -4.31 ms, I-misses -0.71, D-misses -2.04. Inside: the translator 17.48 -> 15.78 (vm_all 1.57 -> 0.11, xl_vif1_span 0.86 -> 0.50, vif_cmd 0.53 -> 0.16; `xl_lane_fx` 0.32, `vm_dec` 0.07); the walk 7.49 -> 7.07; the converters +0.01 (`vc_fpos_rec` 0.29 against 0.22); the game's code +0.10 and textures -0.14 (not looked into); instructions -1.17 ms, I -0.30, D -0.73. The codec window has no effect prim in the transfers the translator takes (`xl_lane_fx`, `vc_fpos` and `vc_fpos_rec` run no instruction off or on): its -9.7 k instructions a step are spread over io_dma_start (-4.3 k), gif_qw, the game's code and the VU0 model with every hash line the same, the run's waits rather than lane 3's code [I]. §11.27 expected about -3 to -4 ms a deck step and -1 to -1.5 inside [I]: the lane gave -7.05 and -2.20, the decode and span work alone -6.06 and -2.19, the walk another -1.51 and -0.42, against +1.15 and +0.39 for the lane's own code and +0.45 and +0.07 for the rec form. This build has no PGO profile and no code order, so its lane-off figures lie above gl10's model runs (codec 104.4 against 98.7 ms, deck 178.7 against 154.0, inside 115.0 against 97.1); against sb5's model runs (lanes 1, 2 and 4 on, no lane 3) lane 3 off is +1.28 ms on the codec (D-misses +0.93, I +0.30, instructions +0.04), -0.37 on the deck and +0.08 inside: the layout of a build without code order [I]. With gl10's kind of profile and order the instruction and D-miss part of the deck saving (-4.31 and -2.04 ms) should carry over; the I-miss part (-0.71) depends on the new order [I].
- **Memory and size [C]:** the lane allocates nothing (no records held; one static `XlRec`, 60 bytes, and four counters). Text +3,496 bytes over sb5's model build (4,050,140 -> 4,053,636): `vc_fpos_rec` 1,224, `xl_lane_fx` 796, `lane_fx` in io_dma_start +720, `xc_fpos`'s lane path in xc_call +616, call +60, `xlive_chk_dma` in `psp/xl_psp.c` +28 (the lane bit): under the design's 12 KB (lanes 3 and 4 together +23,172 over sb4's model build). Emulator heap: the model runs' lowest headroom 6,140 KB up to the codec window, 6,136 KB up to the deck window and 6,040 KB up to the inside window, lane 3 off and on, the same memory lines as sb5's runs (gl10's: 6,140 / 6,140 / 6,056); the whole indoor route with gl10's delivery cfg (`gl3.sh emu hw`, `GL3=/home/claude/sb6m`; lane 3 on in all of `MGS2_OPT_CYCLE`'s settings): about 5,800 KB at its lowest, sb5's run (hws5) to the line (the break's highest point 0b80d000; gl10's 5,816 KB, hwg10).
- **Emulator [C]** (PPSSPP 1.20.4, that hw run, `emuchk6.sh`): its 434 hash lines and 311 vuh lines are hwg10's and hws5's (no split from hashref); its 199 frame digests of vb 26,061-26,459 (`MGS2_GL_DIG`) are hwg10's and the x86 twin's with lanes 1-4 on (i2on: every field of the 199 picture lines, `gl1_cmp.py`); its 16 shots are hwg10's byte for byte, among them the four on the aft deck in the rain (the frames of vb 30,598, 30,698, 30,798 and 30,998: the rain sheets and the splash are lane 3's prims, thinned at 1/2). The other picture lines are hwg10's but the texture pool's address (0b47a3c0, sb5's too) and the route's total of list words, 201,949,765 against hws5's 201,949,671 and hwg10's 201,949,694; gl10's run of the same build without the profiled step (hwg10n) has 201,949,631, so that total moves between runs of one build (why: not looked into). The model runs: every hash and vuh line the same off and on and as sb5's (s5con, s5don, s5ion), no split.
- **For gl11 [I]:** the exactness and the census need nothing more; gl10 + lanes 3 and 4 with the PGO profile and code order made again (sb6's `pgo_gl.sh`, T = sb6), then gl10's checks on that build, and the turns everything on / lane 3 off / everything on / lane 4 off as designed (the deck's lane 3 off windows should show most of the model's -7 ms, the codec's lane 4 off windows lane 4's).

### 11.34 gl11: lanes 3 and 4 with the profile and code order made again (2026-11-01; task #119)
The build of §11.32 + §11.33 (tree sb6) with the picture code's PGO profile and code layout made again (sb6's `pgo_gl.sh`, rain and splash 1/2 in its training and trace runs); details, md5s and every check in psp_harness.md "Build gl11".
- **Exact [C]:** x86 with the delivery's turns (everything on, lane 3 off, everything on, lane 4 off; the twin's lanes now follow g_opt per transfer as the PSP's do, `tools/dev/sb6/gl11_laneopt.diff`) on 3 routes: content digests, picture lines and state hashes equal to the all-on runs and the references; PPSSPP master and 1.20.4 (with and without the profiled step) to the end: 434 / 434 hash lines, gl10's 199 digests and 16 shots byte for byte (master = gl10's PSP shots but the aft deck's bottom rows, #68, as before). `layout_sim -new` equals the search exactly.
- **Model [C model, §11.25's fit, everything on, rain 1/2]:**

| ms a step | gl10 | gl11 | change |
|---|---|---|---|
| aft deck 30,810-30,890 | 154.0 | 147.6 | -6.4 (-4.2%): converters -4.13, walk -1.22, translator -0.31 |
| inside 33,410-33,490 | 97.1 | 94.1 | -3.0 (-3.1%): translator -0.97, texture map -0.57, frame + batches -0.34 |
| codec route 31,510-31,590 | 98.7 | 89.5 | -9.2 (-9.3%): translator -6.22, walk -2.42 |

Against the no-PGO models (lane 3: deck -7.05, inside -2.20; lane 4: codec -11.2) the profiled build keeps most of lane 3's deck saving and all of its inside saving (the new order also packs the effects' code), and about 80% of lane 4's [C model]. Est. 47.6 min for the run (gl10 49.6), the all-on deck about 153.5 ms a step, inside about 93.4 [I]. Memory: 16 KB less heap at the lowest point than gl10 (about 596 KB on the PSP [I]).

### 11.35 Lane bookkeeping: the lanes' constants copied at once, cheaper marking (2026-11-01; task #121; Ffm chose "Lane bookkeeping" 15:26)
Tree sb8 = sb6 (gl11's code) + `tools/dev/sb8/sb8.diff` (5 files; README and scripts there). Two switches, both on by default, both exact, for the PSP's turns.
- **Where the time went [C model, gl11's code, deck 30,810-30,890]:** ud_new 5.2 ms a step (0.79 M instructions, 3,960 calls), the decode at the reads (vm_one 1.6, ud_vec 1.8 ms, inline in the readers). The lanes' constant blocks (lane 1's and 2's block and materials and their CNT headers, lane 4's k >= 6, lane 3's k >= 2) were 1,614 UNPACKs and 9,991 vectors a step on the deck, all V4-32, no mask, MODE 0, CL = WL (census); each became a descriptor and was decoded later, when read or at the transfer's end. Holding the streams back too was sized first and dropped (only a third of their decodes could go).
- **OPT_XL_EAGER 0x40000000** (MGS2_OPT=bfffffff turns it off; x86 MGS2_XL_EAGER=0): `ud_eager` copies a constant block in that form into VU memory as it arrives (4 words a quadword) and gives its quadwords back to VM (own 0, as when an UNPACK overwrites a pending quadword whole); any other form, or off: ud_new, as gl11. x86 line counts on the indoor route from boot (`tools/dev/sb8/mkcov.sh`): 44.3 M copies, the other path never taken. Lazy counts (x86, whole routes): deck route UNPACKs left to decode 70.9 M -> 38.9 M, decodes when read 135.7 M -> 0.5 M; indoor 99.2 M -> 54.9 M and 194.4 M -> 0.8 M.
- **OPT_XL_MARK 0x80000000** (MGS2_OPT=7fffffff; x86 MGS2_XL_MARK=0): ud_new marks own[] for a WL 1 stride four entries a turn, and for a WL 2 stride with CL > 2 two a turn, when the stride stays below quadword 1024 (no wrap mask per entry); otherwise gl11's loops. Indoor route, x86 [C, line counts]: of 54.9 M lazy UNPACKs, 43.0 M WL 1 strides, 6.5 M WL 2, 0.5 M other strides, 4.8 M CL = WL, 0.13 M through the table-full or ROW / COL path; no WL 1 stride wrapped.
- **Tried and dropped [C model]:** ud_new's usual case as a separate leaf function: it still saved four registers, and the gate, call and return cost 8 instructions a call (deck: 0.43 M instructions a step against 0.40 M inline); an eager decode of the other forms through the decode kinds, which never ran on the three routes (2.4 KB of code, removed: such a block waits as in gl11).
- **Exact [C]:** x86 from boot, indoor to vb 37,800, deck tutorial to 25,200, codec route to 40,000: 48,361 frames' content digests and picture lines and 378 / 252 / 400 state hashes equal to gl11's runs (sb6) and the hash references; the same with the turns (on, eager off, on, mark off every 100 vblanks, the lanes following g_opt); the whole VU memory at every call (xl_vm_logical digests): indoor vb 30,800-30,900 identical to gl11's file, codec 31,500-31,600 on = off. Every version above was checked the same way; the model runs' state hashes equal the reference.
- **Model [C model, §11.25's fit, no PGO, rain 1/2; one build, the switches off (3fffffff) against on]:**

| ms a step | off | on | change | of it, instructions |
|---|---|---|---|---|
| aft deck 30,810-30,890 | 172.1 | 169.0 | -3.1 (-1.8%) | -2.9 |
| inside 33,410-33,490 | 112.5 | 110.0 | -2.5 (-2.2%) | -2.1 |
| codec route 31,510-31,590 | 105.0 | 103.2 | -1.8 (-1.7%) | -1.5 |

Deck by switch: eager off alone 171.4 (its share -2.4), mark off alone 170.0 (-1.0). The translator: deck 23.17 -> 19.82, inside 16.24 -> 14.00, codec 11.43 -> 9.85 ms; on the deck ud_new 5.41 -> 3.16, vm_one 1.76 -> 0.16, ud_vec 1.62 -> 0.15, ud_eager 0.25 -> 2.31 (the copies: 1.10 instructions, 0.81 D-misses). Against gl11's own model build (sb6m): deck 171.6 -> 169.0, inside 112.8 -> 110.0, codec 104.3 -> 103.2 (this build's off side differs from gl11's by +0.5 / -0.3 / +0.7 ms: placement). The instructions are the firm part; the misses move by about a millisecond between builds with placement (the leaf form measured -4.2 / -2.0 / -1.0 the same way).
- **Next [I]:** gl12 = sb8 with the picture code's profile and code order made again (sb8's pgo_gl.sh with T = sb8), turns on / eager off / on / mark off; expect about -3 ms on the deck, -2 to -3 inside (+-2.5, placement), -1.5 in calls. The translator's largest parts now (deck): gif_qw 4.2, call 3.4, ud_new 3.2, ud_eager 2.3 ms a step.

### 11.36 gl12: the bookkeeping with the profile and code order made again (2026-11-01; task #122; Ffm chose "Build gl12", 18:08 / 18:14)
gl11 + §11.35's two switches, built with sb8's `pgo_gl.sh` into /home/claude/sbg12 (seed 2 of the order search; `layout_sim -new` equals the search). On the PC the same game state and pictures as gl11: 1.20.4 NOPROF and PPSSPP master give gl11's 434 hashes, 311 vuh, 199 frame digests and 16 shots byte for byte (`psp_harness.md` "Build gl12"). Model against gl11 (both with their own profile and order, everything on): deck 147.6 -> 144.8 ms a step (-1.9%), inside 94.1 -> 92.5 (-1.7%), codec 89.5 -> 88.0 (-1.7%); the translator alone 18.70 -> 15.33, 13.19 -> 10.78 and 10.01 -> 8.35, the rest within 0.3 ms (placement). Instructions -3.0 / -1.9 / -1.0 ms against the no-PGO build's -2.9 / -2.1 / -1.5 (§11.35). Turns on the PSP: everything on, copy off, everything on, marking off. Estimate about 47 min for the run [I]; delivery folder /home/claude/gl12 (EBOOT 5f906d81).
