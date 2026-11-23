# Native GE renderer, step 0: results (container only)

2026-11-23, STEP 0 agent. Design: `mgs2/native_ge_renderer_design.md`. Notes as I went: `attic/ge_native/state.md`.
Nothing went to the PSP, and no PSP build was packaged. All runs are x86 twin runs under `nice -n 10`, one at a time.
The cooker sources are in `attic/ge_native/cooker/`. They contain code only, no game data.

Marks: [C] read from code or data; [C twin] counted on the x86 twin; [C model] computed from a model; [I] inferred, with its basis; UNKNOWN.

## 0. Summary

| Question | Answer |
|---|---|
| Cooker | Written, C99. Builds clean with gcc (x86, `-Wall -Wextra`) and psp-gcc (the port's flags). Ran on the twin on every ship mesh of the route [C twin]. |
| Today's ship pictures identical? | **Yes, for every P04+df0 ship group from vb 24,000 to 37,800:** 1,658,466 conversions, **0 differences** of any kind [C twin]. Three exceptions must stay on today's path; see §2.3. |
| vb 25,944 / 25,974 | **LEFTOVER (R5).** 918 reads differ, all in the w lane, with one reader and one writer in the scratchpad. No later read differs [C twin]. |
| Static lighting (0x12cd78) | **0 calls** on the route. No cooked byte went stale at a draw [C twin]. |
| Census (w00a, per picture, max / mean) | 868 / 766 P04 meshes queued, 553 / 115 drawn; 70,883 / 59,380 P04 vertices queued; 79 / 62 models; 736 joints; 24 model pages (62 in the page list) [C twin] |
| Cooked ship memory | Drawn set per picture: 861 KB max, 198 KB mean. Queue set (pre-lit per object + lit per model): 1.07 MB max. Whole w00a, everything drawn at least once: 2.01 MB [C twin + cooker format]. |
| EVM builder | 0x136f38, called at 0x13846c inside 0x137730 [C gen_elf.h]; that it is the face mesh builder is [I]. |

## 1. The cooker (`cooker/`)

| File | What it is |
|---|---|
| `nge_fmt.h` | The cooked format: GE command numbers, vertex types, `NgeGroup` (28 B), `NgeMesh` (48 B), `NgeArena`. |
| `nge_cook.h/.c` | Cooks one mesh from guest RAM into the arena. Also: `nge_mesh_same` (the check at each use), `nge_relocate` (move the arena to RAM or VRAM), `nge_mesh_bytes` (size only, for the census). |
| `nge_list.h/.c` | The per-frame words. `nge_xf_block` computes the transform from the mesh's constant block, exactly as `ge/vu1conv.c vc_xform` does (same operations, same VU float reading). `nge_put_world` gives the 13 WORLD words. `nge_put_call` gives BASE, CALL, BASE per group. |
| `nge_twin.c`, `hooks.diff`, `mk.sh`, `run.sh` | The x86 harness (§2), its hooks into a scratch copy of t39 (all under `-DNGE_TWIN`), the build and the run scripts. |

**What a cook makes, per group [C: the code].**
- Pre-lit groups, the deck's ship (mesh+0xC0 bit 0): 16-byte vertices, vertex type 0x11e. They are byte for byte what today's df0 converter writes: u|v, then r g b and alpha (alpha doubled), then x y z and a zero pad.
- One display list per group: VTYPE, BASE, then VADDR (only where a strip does not follow the last one) and one PRIM per strip, then RET. The strips are today's `vc_strips` runs, taken from the ADC bits.
- A box of the positions, for the cut.
- Flags for the cases today's path draws nothing or keeps its own path: no strip, negative UV, a stream not sent, too many vertices.
- Lit groups (entries 780 and a70) are cooked for GE lighting: 20 B, or 24 B with two 8-bit weights. **The design's §2.1 says 16 B; with a (d, d, d, a) colour it is 20 B, and 24 B with weights [C: the GE's vertex layout].** These lit groups are not today's picture; they are judged by shots in step 1 or later.

**Builds [C].**
- psp-gcc, `-O2 -G0 -fwrapv -fno-strict-aliasing -ffp-contract=off -ffixed-23`: `nge_cook.o` is 4,944 B of text, `nge_list.o` 672 B. The only outside call is `memset`.
- No allocation, no system calls. Reads guest memory only, writes only the arena. A cook is host-only work, so it never changes a guest write. The twin's state hashes confirm this: refcmp against hashref_gl38, 685 lines, 0 differ, in both runs [C twin].

## 2. Equivalence with today's ship batches

### 2.1 Method [C: harness code]

The run is `st0/bld/boot`: the gl39 twin (build19's gen.o, t39's ge/ and boot/) plus hooks. It uses h1/tr.sh's gl39 settings with `MGS2_NGE=1` and runs to vb 37,800.

1. **At each 0x1214a0 call the stub records.** The mesh is cooked, or its earlier cook is reused if `nge_mesh_same` passes. The check covers the descriptor array, the three stream bases, the group count, the pre-lit bit and a sum of the descriptor words. WORLD and pa/pb are computed from the constant block that SPR 0xc54 points to, at stub time, as the native path would do it.
2. **At each P04+df0 conversion of a walked group.** The harness takes today's input to the batch (`vc_df0_rec`: the same bytes as the batch) and compares it with the cooked group:
   - every vertex word: positions, UVs, colours, alpha;
   - the strips, decoded from the cooked list's VADDR and PRIM words;
   - the 12 WORLD floats and pa/pb, bit for bit. Equal floats mean equal WORLD and PROJ words;
   - the cut, computed from the cooked box and the stub-time WORLD;
   - the negative-UV case.
   If a group differs, it is cooked again from current memory. If the fresh cook matches, the difference is STALE; otherwise it is FORMAT. For every batch made, the batch itself is also compared with the reference.
3. Two more hooks: the dz arms in `gelist.c xf_draw`, and a wrapper around 0x12cd78 that compares each mesh's colour stream before and after the call.

This compares what the GE reads at each PRIM: the VTYPE value (0x11e in both), the vertex bytes, the PRIM word, WORLD and PROJ. The state words (material, blend, tests, fog, viewport) are the same code in both paths. That equal inputs at every PRIM give an identical picture is [I: the GE's output depends only on its registers and the vertex bytes].

### 2.2 Result [C twin]

| | Count |
|---|---|
| df0 conversions of walked ship groups compared (vb 24,000-37,800) | **1,658,466** |
| of which made a batch / were cut | 1,067,308 / 591,158 |
| Differences: vertex count, positions, UVs, colours, alpha, strips, WORLD, PROJ, cut, negative UV | **0 each** |
| STALE / FORMAT | 0 / 0 |
| Batch ≠ reference (sanity) | 0 |
| Compared without a stub-time transform | 0 |
| Fog (abs 2), viewport (abs 4/5) or alpha (abs 3) differing between ship calls of one picture | 0, over 4,836 pictures |
| Cooks / reuses / re-cooks after a pointer or descriptor change | 2,032 / 425,908 / 710 |
| Groups that went stale after a reuse | 0 |

The vertex alpha was cooked with the value seen at cook time and never differed at a draw. It was constant over the whole route [C twin].

### 2.3 What is not covered, by kind [C twin]

| Kind | Count | What step 1 does |
|---|---|---|
| DZ: ship batches drawn by the depth-pair X arm (`MGS2_GL_DZ`, paired decal vertices instead of the converter's) | 31,483 of 1,067,289 batches (2.9%), from vb 29,069 to 32,000 | Keep today's path for these batches, or pair on cooked data. Y, W and B arms: 0. |
| Non-df0 ship groups the stub recorded (VU1 entry = MSCAL halfword × 8) | 780 (lit): 203,499; a70 (selector): 84,971; 1088 (pre-lit with back-face test): 18,996. In all, 307,466 of 2,233,651 groups (13.8%) | Today's path (VU emulation). Native lit = GE lighting, judged by shots, not identity. |
| A stream not sent (count 0) | 67 conversions | Today's path. |
| df0 groups with streams in the packet area (not INPL) | 275 of 1,926,185 | Compared equal on this route. Step 1 should keep them on today's path [I: packet memory is rewritten each frame]. |
| df0 conversions with no walked group | 74,082, all before vb 24,000 (tanker stages, stub not armed) | None. From vb 24,000 every ship df0 conversion came from a walked group. |

### 2.4 Rules the list writer must keep [C: gelist.c]

The pictures are only identical if the native writer keeps these rules.
- **gelist's memos.** gelist memoizes WORLD by pointer (`s_xf`), VTYPE (`s_vtype`) and BASE (`s_base`). After a native WORLD or CALL, the writer must clear `s_xf` and `s_vtype`, and must either restore BASE to `s_base` (as `nge_put_call` does) or clear it. Otherwise the next non-ship batch may skip its WORLD or VTYPE.
- **Arena bounds.** One BASE serves the arena only if the arena does not cross a 16 MB boundary of the GE's address space.

### 2.5 List words [C twin]

Over 1,067,289 ship batches, counting what the main CPU writes per frame (state words are the same in both paths and not counted):

| | Words | Per batch |
|---|---|---|
| Today: WORLD 13 + VADDR/PRIM | 21,146,247 | 19.8 |
| Native: WORLD once per change (30,948 writes) + 3 per group | 3,713,055 | 3.5 |

The cooked lists the GE executes from the arena total 10,582,221 words. They are written once, at cook time.

## 3. The design's open items

### 3.1 vb 25,944 and 25,974 [C twin]

- **Runs.** native/xt/rh.sh's binary (copy in st0/xt) at gl28 game settings, base and V3a (`MGS2_LITE_ALL=0 MGS2_LITE_FN=1214a0,12e8b8`), with `MGS2_READ_LOG` on those two vblanks.
- **Reads.** 599,919 reads paired; 0 reads made by only one run; 918 reads see other bytes.
- **Reader and writer.** All 918 are w-lane reads by S_d01t_P329668 (+377, +477, +56b, +65c), of words written by S_d01t_P299e90 in the scratchpad 0x70000c00-0x70000fff.
- **Verdict: LEFTOVER (R5).** No later read differs.

### 3.2 Census of w00a [C twin]

w00a means vb 24,000-31,399: 2,119 pictures with stub calls. w01a+ means vb 31,400-37,800: 3,070 pictures. Each figure is the max / mean per picture. It comes from the draw queue (chanl 0x1df950, layouts from re/dg_objects.md), walked as gen/census.py does.

| | w00a | w01a+ |
|---|---|---|
| Objects queued / drawn | 219 / 175 · 56 / 21.6 | 128 / 124 · 55 / 25.2 |
| P04 meshes queued / drawn (= stubbed) | 868 / 766 · 553 / 115 | 377 / 376 · 110 / 60 |
| P04 pre-lit / lit meshes queued | 506 / 502 · 362 / 264 | 145 / 145 · 232 / 231 |
| P04 groups queued; groups stubbed | 3,125 / 2,690; 2,079 / 469 | 2,053 / 1,995; 726 / 404 |
| P04 vertices queued / drawn | 70,883 / 59,380 · 41,709 / 9,575 | 51,689 / 49,951 · 19,158 / 10,824 |
| P06 meshes / vertices queued | 524 / 142 · 65,384 / 19,367 | 266 / 200 · 41,972 / 27,419 |
| Models (distinct defs) | 79 / 62.4 | 63 / 63 |
| Joints: sum over models / largest model | 736 / 668 · 273 | 326 · 55 |
| Model texture pages / page-list count ([0x18e8f0]) | 24 / 12.7 · 62 / 46.1 | 15 · 34 |
| Materials (distinct desc+0x14) | 375 / 349 | 481 / 479 |

Whole stage, by mesh address:
- w00a: 2,988 distinct P04 meshes queued (247,067 vertices); 1,107 of them stubbed (96,618 vertices).
- w01a+: 782 queued; 215 stubbed.

**Static lighting staleness.** 0x12cd78 ran 0 times from vb 0 to 37,800. The wrapper was linked as the symbol every caller reaches, and the stage units hold no reference to it [C]. Re-cooks came only from pointer or descriptor changes (710), which happen when object slots are reused. The check at each use caught every one: 0 stale at a draw.

### 3.3 The EVM builder

- 0x137730 calls 0x136d18 at 0x138464, then **0x136f38 at 0x13846c** [C gen_elf.h].
- 0x136f38 writes REF DMA tags with UNPACK 0x6d008100 (V4-16 to T+0x100, like the P04 template), plus V4-32 and V4-8 unpacks and MSCAL 0x14000000|entry. It writes them into the scratchpad packet buffers at 0x700003f0 + 384·k [C].
- It is the face mesh packet builder [I: from these words]. Stubbing it is step 5's job.

### 3.4 The constant block [C twin]

The stub's `blk` (SPR 0xc54) was never mesh+0xFC itself (0 of 427,940 calls, even with the address masked). It points to a separate per-mesh block; examples: mesh 0x01488da0 → 0x01b2bff0, mesh 0x0127dba0 → 0x01b1dd70. Reading WORLD from it at stub time gave the walk's matrix in every compared group. 296 stub calls had `blk` outside main RAM; none of their groups reached a df0 conversion.

## 4. Memory

Cooked bytes use this cooker's format: groups, vertices and lists. Pre-lit is cooked per object, because the colours are per object; lit is cooked per model.

| Set | w00a max / mean | Mark |
|---|---|---|
| Drawn set per picture (pre-lit + lit) | 860,536 / 198,010 B (pre-lit 726,136 / 160,334; lit 134,400 / 37,676) | C twin |
| Queue set per picture: pre-lit per object + lit per model | 801,544 + 264,220 = 1,065,764 B max; about 955 KB mean | C twin |
| Every mesh drawn at least once in w00a, no eviction | 2,010,260 B (1,107 meshes) | C twin |
| Every queued mesh, by address | 5,310,856 B (2,988 meshes; counts lit per object and reused addresses, an upper bound) | C twin |
| Harness arena, append-only (re-cooks leave their old bytes) | 2.98 MB at vb 31,001; 3.72 MB at vb 37,800 | C twin |

**Against the design's budget.**
- **Design.** It estimated 742 KB for the ship (640 KB pre-lit + 102 KB lit), and 1.2-1.4 MB for the deck total, people and lists included, against about 1.1 MB freed [I there].
- **Measured.** Cooking the whole queue takes 1.07 MB for the ship alone, about 0.3 MB more than estimated. Two reasons: lit vertices are 20/24 B, and the group lists and records are included. That would put the deck total near 1.5-1.7 MB [I: design's people and list figures + this ship figure], beyond the freed memory.
- **Fix.** Cook only what is drawn, with eviction. The live set is 198 KB on average and 861 KB at the worst picture [C twin]. A miss is cooked in the stub (host-only work). Re-cooks were rare on the route: 2,032 cooks in 13,800 vblanks.

**Where it lives [I].**
- Home: the RAM arena, in the frame-arena space the native path frees. The arena moves with `nge_relocate`.
- VRAM (688 KB free): the mean drawn set fits, the worst picture (861 KB) does not. VRAM can hold a hot subset; probe P1 decides between RAM and VRAM.

## 5. What step 1 needs next

1. **The PSP stub.** In 0x1214a0's stub: cook on a miss, check with `nge_mesh_same` on reuse, and compute WORLD from blk at stub time (§3.4). Then write the WORLD, BASE, CALL and BASE words instead of today's per-batch words, for df0 groups only.
2. **Keep today's path for the exceptions in §2.3:** DZ-armed batches, non-df0 entries, groups with a stream not sent, and packet-area streams.
3. **Keep gelist's memos right** (§2.4).
4. **Memory.** The arena with eviction, sized from the drawn set (about 0.2 MB mean, 0.9 MB max), placed in the freed frame-arena RAM; log the arena fill per frame (design P5).
5. **The PSP check.** Show the native list equals today's on the PSP twin: the same harness (`nge_twin.c`) can run as the step 1 build's check mode. Then probe P1 (VB in RAM against VRAM).
6. **Open.**
   - Lit groups (780/a70: 288,470 recorded groups) and 1088 (18,996): no identity is possible; judge them by shots when their steps come.
   - The source of the vertex alpha (VU1 abs 3) is UNKNOWN. It was constant over the route [C twin], and the native path can take it from the first ship call of a picture.

## 6. Re-running (scratch: `/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st0/`)

- Build: `st0/mk.sh` builds `st0/bld/boot` from `st0/t` (t39 + `hooks.diff`) and `st0/nge`; about 1 min.
- Run: `st0/run.sh <tag>` writes `bld/nge_<tag>.txt` (the "nge:" lines) and `bld/cmp_<tag>.txt`; about 3.5 min.
- Runs done: nge1 and nge2 (nge2 = nge1 + classification counters); both refcmp 0 differ.
