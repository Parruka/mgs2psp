# LOTR: The Two Towers (PS2) → PSP port — complete handoff

Written 2026-10-17 at the end of a long working session. It covers the goal, the user's rules, what was learned about the game's files and engine, the architecture of the port, every decision and measurement, the current state (build v19), and the next steps. Read it fully before touching anything.

---

## 1. Goal, scope, user rules

- **Goal:** a literal port of *The Lord of the Rings: The Two Towers* (PS2, NTSC-U **SLUS-20578**, EA 2002, single-player only) to the user's **PSP-3000 on ARK-4 CFW**, built from the user's own disc.
  - Only the prologue level **00isul1** (Isildur, the Last Alliance battle) has been done so far.
- **Fidelity target:** at least **95% replica** of the original game.
- **Frame-rate target:** **25 fps minimum, 30 fps would make the user very happy.** An earlier target of ~45 fps was dropped.
- **Gameplay changes the user has approved:**
  - LOD tricks with "almost zero effect on gameplay":
    - NPCs farther than 3 m from the player update every 2nd frame (`lod.h`);
    - 4 elves; 2 followers;
    - Kirk/Spock (the two follower brains) think every 3rd frame;
    - awake-enemy cap of 10.
  - Character/prop textures converted to 256 colours with mipmaps. The user tested it ("character look ok"); it is the default.
  - Use of the Media Engine (the second CPU). It was tried for the skeleton pass and lost to cache flushes, so it is not used for that.
- **User preferences:**
  - answers in **English**, direct, no wasted tokens;
  - **no guesses**: use real data and sources, deep-search when needed. "You are the expert";
  - game-derived files are for **personal use only**; never publish them.
- **Safety constraints from the user (verbatim intent):**
  - nothing may ever open `flash0:/` for writing or touch the NAND hardware;
  - no stray writes to hardware registers (power/clock): keep kernel/ME code minimal and pointed only at our own buffers;
  - hardware counters stay **off** unless the user consents (see §6.4);
  - overclocking needs the user's consent, and only through the CFW's own clock call;
  - the user approves any change that alters gameplay or visuals.
- **Workflow with the user:**
  - Claude builds EBOOTs in a cloud container and sends zips (`TTGame_vNN.zip`: `normal/EBOOT.PBP`, plus `config_*/ttgame.cfg` folders and a README).
  - The user copies them to `ms0:/PSP/GAME/TTGame/`, runs them and sends back `game.txt`, the log next to the EBOOT.
  - The user must keep the **level.plv from v15** (`level_v15.plv`, with the strip trailer) on the stick.
  - Other files on the stick that the game reads, all generated from the user's disc:
    - `exe_main.bin`, `pj_guest.bin`, `pl_guest.bin`, `bff_isul1.bin`;
    - `00isul1.bfa` (the original level file);
    - `level.plv`.
  - Bench runs are deterministic: no input, fixed dt, 900 frames, and the game-state hash at frame 900 is printed ("chk fr 900 hash ..."). On hardware the default config gives **3650c540** (hero hp 7426).

---

## 2. Environment and where things are

All of this is in the ephemeral cloud container of the old session; it may be gone. `TT_port_source.zip`, delivered with this document, holds the code: no game data, no generated game code.

- `/home/claude/proj/TwoTowers_PSP_viewer/` — the project:
  - `demo_src/` — runtime headers (guest machine, renderer, game glue);
  - `model/` — Python tools: recompiler, exporters, code-generation passes;
  - `psp_game/` — the PSP game: `main.c`, `Makefile`, `run_emu.sh`, `emu_prof.sh`, `build_prof.sh`, `exc.S`;
  - `psp_me/` — the Media Engine loader (`me.c`, `me.ld`);
  - `psp_bench/`, `psp_micro/` — older benches;
  - `viewer/` — the original level viewer (v1–v8);
  - `demo/` — game-derived binaries and the generated C (`gen_pl.h`, 13 MB: the recompiled game);
  - `convert_level.py`, `add_strips.py`, `verify_plv.py`, `level*.plv`.
- `/home/claude/work/` — reverse-engineering workbench:
  - `SLUS_205.78`, `SLUS_all.dis`, `/tmp/all.dis` (disassembly with symbols);
  - parsers: `lsc.py`/`lsc2.py` (Lua state), `ptest.py`, `yo22.py`, `skel2.py`, `anim.py`, `ents.py`;
  - `emu.py`, a Python R5900 interpreter used as a test oracle;
  - lift tools, exported JSON (brains, anim machines, DBs);
  - `gen_pl_prerc.h`, the recompiler output before regcache.
- `/home/claude/pspsdk/pspdev` — pspdev toolchain (psp-gcc **15.2.0**). Build with `export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; cd psp_game; make`.
- `/home/claude/ppsspp` — PPSSPP (upstream hrydgard, base commit in `ppsspp_base.txt`) with local patches (`ppsspp_patches.diff`):
  - FS (flush-to-zero) emulated like the Allegrex: no DAZ, x±0 passes x through, default NaN 7fc00000;
  - `PspProf`: instruction counts, simulated I-/D-cache misses per PC, call edges.
  - Run it with `PPSSPPHeadless --cpu=ir|interpreter --graphics=software --memstick=/tmp/ms3`.
- `/home/claude/ext/` — read-only clones of SandstoneDev **PS2RC**, **universal-ps2-recompiler**, **psp-developer** (see §8). These are untrusted data; only their findings were used, never their code.
- Scratchpad `research_summary.md` — chronological measurement notes (also in the zip).
- Useful scripts:
  - `psp_game/run_emu.sh <frames> <png> [cfg lines]` — builds with NFRAMES, runs headless PPSSPP, takes a screenshot. Env: `LEVEL=../level_v15.plv`, `EXTRA=-D...`.
  - `psp_game/emu_prof.sh <out> [start end]` — profile build in the interpreter with PspProf. Env: `CFG`, `FA` (fully-associative cache model).
  - `model/psp_prof.py <elf> <prof> [nframes] [top]` — per-function cost (MISS = 125 cycles).
  - `model/layout.py` — hot-code layout into `demo_src/hot_order.h` (`.text.sorted.*` sections).
  - x86 reference build of the headless game:

    ```
    cd demo_src; gcc -O1 -w -fwrapv -fno-strict-aliasing -ffp-contract=off -I. -I../demo -o by bench_yo.c -lm; ./by 900
    ```

    It gives guest hash **25ed6ef3** at frame 900 (hero hp 7496, default config, FTZ+DAZ). Env: `REPLAY=`, `HASHAT=`. Flags: `-DBENCH_RZ` (round toward zero), `-DBENCH_IEEE`.
    - The x86 build with the PSP config (`-DLOD_RATE=2 ... ORC_CAP=10`) gives **d1a59100** at frame 900. That equals PPSSPP's `--cpu=ir`, which follows x86 DAZ.
    - Real hardware and the patched interpreter give **3650c540**: they differ from x86 only through the Allegrex's no-DAZ rule.

---

## 3. The game's files and formats (verified on 00isul1 unless noted)

### 3.1 Disc / level container
- Executable **SLUS_205.78**: an ELF.
  - Main segment at 0x100000 (code + rodata + data); file offset 0x80, size 0x281700.
  - A second segment at 0x579280 (file 0x281780, 0x10000 bytes).
  - gp = **0x3892f0**; the main thread's stack is around 0x3f00000.
  - `model/export_exe.py` → `demo/exe_main.bin`.
- Level files live in `Worlds/`: `00isul1.bfa` / `.bfl` / `.bfg` / `.bfi` (04balin1 is another level, partly inspected).
- **BFA/BFF container** (ClMissionLoader image):
  - `u32 version (9)`, `u32 directory offset` (+4);
  - directory = `u32 count`, then `count × {u32 type tag, u32 offset, u32 id/hash}`;
  - an entry's size runs to the next entry offset (or the directory).
  - The first resource, at offset 8, is the level's **Lua logic block**.
- Directory types seen:

  | Type | Content |
  |---|---|
  | 0x12c | textures (ClPbi) |
  | 0x15e / 0x578 | render-only assets |
  | 0x1f4 | entities/objects (CreateProp / CreateActor records: kind at +0x104, id +4, name +8, record at entry+0x28: position +0, rotation +0xc) |
  | 0x38e | enemy DB |
  | 0x3ca | projectile DB |
  | 0x389–0x38b | text tables |
  | 950 | path rooms |

  - `model/export_bff.py` writes the BFF with the render-only types emptied (`demo/bff_isul1.bin`). The game boots from that file; the original accessors work unchanged.
- **'PSWB' blob** inside the .bfa: the world geometry, PS2 VIF packets.
  - UNPACK V3-32 + GIF tag, 9 floats per vertex; triangle strips.
  - "batch" tables say which texture each chunk uses.
  - The world header +0x4c lists the 6 sky-face textures.
- **Textures (ClPbi):**
  - `u32 magic 0x00046270`, +4 data size, +8 `w | h<<16`, +0xC format, pixels at +0x80;
  - formats: 0 RGBA32, 1 RGB24, 2 RGB555, 3 grey 8, 4 = 8-bit paletted (1 KB CSM1 palette, then PS2-swizzled PSMT8 pixels);
  - alpha 0x80 = opaque on the PS2 (×2 for the PSP);
  - CSM1 palette order: entries 8–15 and 16–23 swap in every group of 32;
  - PSMT8 unswizzle formula in `convert_level.py unswizzle8_offsets`.
- **DBDB tables** (enemy, projectile, weapon, upgrade, blast, linked-anim):
  - header `[magic 'DBDB', ver=1, size, nrows, ncols, nrows, strpool_off, 0]`;
  - per row: `u32 n`, then `n × (u32 hash, u32 value)`;
  - column hash: `h=0; for each signed char c: h += c + (h << (c & 7))`;
  - the column names are string literals in the executable, read by each `::load` (exported with the blob).
- **YO22 models** (CModelBin, the VU1 "Noam" packet of a figure):
  - +0x08 u16 qwc of the VIF stream; +0x12 u16 offset/16 where the shadow mesh starts;
  - +0x40 offset (3f) and +0x50 scale (3f): position = int16 × scale + offset;
  - batches: UNPACK V4-32 GIF tag at 0x8000 (x & 0x7fff = vertex count; w = type: 0 two-bone, 1 one-bone, else static), then UNPACK V3-16 with 4 qwords × 3 int16 per vertex:
    - q0: position;
    - q1: weight×4096, 4×boneB, 4×boneA;
    - q2: u×4096, v×4096, ADC bit 15 = no triangle / strip restart;
    - q3: normal;
  - two-bone blend `p = p·B[A] + (p·B[B] − p·B[A])·w`;
  - VU bone slot i = palette matrix i+1 (RenderDiffuse 0x1a2a90 skips matrix 0).
- **Skeleton ('hier'):** at +0x40, `'hier'` + 7 u32 (nb at [2], offa [4], offb [5]); 4×4 matrices from 0x80; parent array found after the `00 00 80 3f 00 01 02` marker (`work/skel2.py`).
- **Animations ('anim' clips inside ClAnimBin 'aniz'):**
  - clip size at +0x14, end time at +8, per-bone table at +0x20;
  - per channel: `nq, nt`, quaternion keys (u16 time with sign bit, i16 xyz/32768, w rebuilt), translation keys (i16/128);
  - per-clip trigger nodes `{u16 time ticks, u16 bone}` at clip +0x30/+0x34;
  - OBB arrays: `ClAnimBin::getOrientedBBoxArray` 0x1bbf10, 172-byte records {u32 bone, 8 corners, 6 normals}, bone-local. Bone groups: `getBoneIndex` 0x1bc160.
- **Static props ('prps', ClStaticPropData):**
  - 256-byte header (+0x0c sub-object count, +0x10+4i stream offsets);
  - stream = DMA RET tag, then UNPACK V3-32 batches (GIF tag + per vertex position, normal, (u, v, ADC));
  - lit per vertex by the microcode (sun, fill and one dynamic light).
- **Sky (ClSkyDome):**
  - a 6×6 quad grid in the plane y = +10000, drawn 6 times with face matrices from 0x35e160 / 0x35e1b0 / 0x35e110;
  - faces 0–1 are 256×256, faces 2–5 512×256;
  - Z at the far plane (it replaces the screen clear);
  - every face has ≤115 distinct colours, so a T8 version is exact.
- **Fog (ClFog):**
  - a full-screen sprite textured with the Z buffer through a 256-entry CLUT;
  - alpha = density × max(0, 1 − start/d);
  - ported with the same trick: depth buffer read as T16 through VRAM mirror 0x04200000 (`ge_fog.h`).
- **Lua logic block:**
  - a relocatable snapshot of a whole **Lua 4** `lua_State`: `u32 N`, N relocation offsets, then the image;
  - `ClScriptVM::SetScriptData` 0x1ce9a0 relocates it in place; `luaM_realloc` returns 0, so nothing is allocated at run time;
  - the VM is at 0x107990–0x10dd30;
  - event codes: 1 = trigger, 2/3/51 = timer callbacks;
  - full logic semantics in `docs/logic_semantics.md`: trigDelay, expanders, spawners, emitters, activators; jump opcodes 29/31/32/34/36/39.
- **Path rooms** (type 950): tile collision maps (`pathroom.h`), checked against `work/emu.py`.
- **World collision:** a ClWorldCollisionNode tree, sector packets and VU0 microcode. `pipeline.py wcslice` → `wc_isul1.bin`.

### 3.2 Engine facts (addresses are EE vaddrs in SLUS_205.78)
- **Frame:**
  - `ClGameModePlay::tick` 0x164f80 runs ClClock::tick 0x1cf760 (EE timer 15720 counts/s), ClPad::tick 0x17e6c0 and **CYoGameData::Update** 0x11a0f0;
  - then camera (doCameraControl 0x106d10, ClCamera::tick 0x17ab70);
  - water-reflection pass 0x11a090 (only when game+0x484d is set; **never runs in isul1**);
  - ClFrustumList::reset 0x26f8c0, ClWorld::draw 0x2087a0 (portal visibility);
  - renderObjects 0x2411e0 (marks visible objects: +0x101 bit7, copied to +0x49d bit0);
  - **ClDisplayList::draw** 0x255af0 (computes skeletons; its draw calls are stubs in the port).
- **Level start:** ClGameModePlay::enter 0x165e60 → CYoGameData ctor, initAndLoad, ClMissionLoader::load, parseBFFEntries (loadObjectInstance for every object), HandleDeferredMessages, finishParseBFFEntries, playLevelStartCinematic 0x10e350.
- **Display list** (ClDisplayList::ClData at 0x52d330): 64 animation nodes of 96 bytes; per-kind list heads at 0x52eb3c onwards; texture pairs at 0x52ee0c (`render.h`).
- **Skeleton pass:** ClNoamFigurePs2::ComputeSkeletalHierarchy 0x19e1b0, using VU0 micro programs. The hot one, `vucf_82a952bf`, is 256 VU instruction pairs.
- **Services:** ServiceManager at 0x3c6d90 (request pool 0x500 × 0xa0); HandleServices 0x15b150. Covered:
  - Obj2World 0x13e410, Obj2Pathroom 0x1377e0, Brain2World, Weapon2World 0x13c900;
  - FaceZ 0x143c10, BrainGoto 0x1329d8, Range 0x12f830.
- **AI** (full notes in `docs/ai_semantics.md`):
  - brain rules: AddBrainRule, conditions `[AND] (OR) <NOT>`; input tables 0x33f520 (anim) and 0x33f630 (brain);
  - CBrain::Update 0x12d880; ClEncounter (crowd director) 0x276090..0x278780;
  - target detector 0x2787f0 ff.
- **Player:** ClPlayer::update 0x157bc0 and the update* input functions, with the pad mapping tables at 0x33f780 / 0x36b3c0.
- **Enemy IDs** (SetEnemyID): OrcMelee 26, OrcShield 2, Elven warrior 78, Isildur 77, archer (Rupert) 59.
  - Projectile IDs: boulder 4, lava bomb 26.
  - Actor classes at +0x104: 2 fellowship, 3 enemy, 7 prop, 24 siege ladder.
  - The player pointer is at gp−0x7a44.
- **Level-1 reference video:** user upload `202610111522.mp4`, 3:18; timeline in `docs/level1_video_timeline.md`.

### 3.3 FPU rules
- **R5900 (PS2):**
  - **rounds toward zero** (EE Core manual: guard/round/sticky bits ignored; GCC R5900 discussion, gcc-patches 2013-07 msg00325);
  - exponent 0 operands count as zero (DAZ) and denormal results flush;
  - no Inf/NaN (overflow → ±Fmax); x/0 = ±Fmax (our `FDIV`).
- **Allegrex (PSP) measured on hardware** (our FPU self-test):
  - with FCR31.FS = 1, denormal *inputs* are used exactly (**no DAZ**), denormal results flush to signed 0;
  - x ± 0 returns x unchanged; inf − inf = 7fc00000.
  - Round toward zero (FCR31 RM = 1) works. Its differences from x86 RZ are only the same no-DAZ cases.
- The port runs FS + round to nearest by default.
  - `fpurz` in ttgame.cfg switches to round toward zero, the PS2-faithful mode. It was tested on hardware: 6060 frames, stable, same speed.
  - It changes the game-state hashes. Not yet the default: the user's decision is pending.
- The VU float model in `vu0.h` follows the VU: denormals flush; Inf/NaN clamp to ±Fmax; MAC/status flags per lane.

---

## 4. Architecture of the port ("TTGame")

**The original game logic runs as statically recompiled code.** The renderer is replaced at the display-list boundary.

1. **`model/recomp.py`** (with `recomp_jt.py`, `tailfix.py`): R5900 → C, one C function per guest function (`P<addr>`, `O<addr>`).
   - Global register file `R[]` / `RH[]` (upper 64 bits) / `F[]`, guest memory accessors `RD*/WR*`.
   - COP2 macro ops go through `vu0.h` (`VUM*`), specialised by `vuemit.py`.
   - VU0 micro programs are AOT-compiled by `vurecomp.py` → `gen_vu.h`. A VFPU form (`vuvfpu.py`) exists, guarded and exact-checked.
2. **`model/yo_set.py`:** the recompile set is the call closure of level boot, the virtual methods of the object/service classes, and the Lua wrappers.
   - The presentation layer (render, sound, pfx, hud, fmv, debug UI) becomes no-op stubs (`gen_stub.h`).
   - The camera stays game logic.
3. **Code-generation passes after recompilation** (`model/gen_pl.py` → `gen_pl_post.py`):
   - **narrow.py:** exact 64→32-bit narrowing (dsll32/dsra32 idioms). 194 of 293 wide functions became 32-bit.
   - **regcache.py:** guest GPRs live in C locals inside a function; `R[]` is the interface at calls and returns.
   - **qsr.py:** runs of callee-saved saves/restores become `qsr_save` / `qsr_rest` helper calls with tables. It excludes `model/hot_fns.txt` (74 looping functions plus the hot_order groups). The helpers are in `.text.sorted.00020/00021`.
   - Pinned guest memory base in `$s7` (`-ffixed-23`, `GUEST_PINREG`), no address mask (`GUEST_NOMASK`).
   - Hot layout (`layout.py` → `hot_order.h`).
   - Output: **`demo/gen_pl.h`** (13 MB). `gen_co.h`, `gen_wc.h`, `gen_pr.h`, `gen_ow.h` and `gen_me.h` are other recompile sets.
4. **`demo_src/yo.h`:** the headless original game.
   - `exe_main.bin` and `bff_isul1.bin` are loaded into a 32 MB guest memory, then the original level start runs.
   - `yo_frame(dt)` = one frame of the original loop (§3.2), timed per part as "update / camera / visibility / renderObjects / skeletons".
   - `YO_BOOT_TRACE` prints a hash at each of the 15 boot steps; hardware matches the patched emulator.
5. **Renderer** (`psp_game/main.c` with `ge_*.h`): uses the game's camera and the display-list nodes.
   - **Level:** `ge_level.h` with PLV4 data (§5); batch culling by AABB.
   - **Figures:** `ge_model.h` turns YO22 data into GE skinned meshes:
     - GE hardware skinning: `sceGuBoneMatrix` (custom fast uploader), `GU_WEIGHTS(n)`;
     - **GM_POS16:** int16 positions, bones ×32768;
     - textures from `ge_tex.h`: median-cut **T8 + box-filter mipmaps** (`GU_TEXTURE_AUTO`, `LINEAR_MIPMAP_NEAREST`); single-colour palette entries use the GE expansion `x<<3|x>>2`.
   - **Props:** `ge_prop.h`, with the original lights reproduced (`ge_prop_lights`).
   - **Sky:** `ge_sky.h`, T8 (exact).
   - **Fog:** `ge_fog.h`.
   - **Overlay:** fps / logic / GPU numbers.
   - Framebuffer 5650, 512 stride; double buffer plus a 16-bit depth buffer = 816 KB of VRAM. Depth range 65535..0 with GEQUAL.
   - The display list is built with **GU_SEND** and sent at the end of the frame, without a sync. The GE draws frame N while the CPU runs frame N+1.
6. **Other runtime pieces** (all original code recompiled, except brain/animmach, which are exact ports from exported tables):
   - `lua.h`: the original Lua VM;
   - `player.h` / `hero_pl.h`: ClPlayer;
   - `co.h`: collision objects;
   - `pj.h`: projectiles;
   - `blast.h`, `struck.h`, `trig.h`, `svc.h`, `target.h`;
   - `wc.h` + `vif.h` + `vu0.h`: world collision on the original VU0 microcode;
   - `pr_move.h` / `ow_move.h`: path rooms and world movement;
   - `brain.h` / `animmach.h`: state machines;
   - `skel_pass.h`;
   - `lod.h`: the approved LOD levers.
7. **Media Engine:** `me_image.c`, `me_skel.h`, `gen_me.py`, `psp_me/` can compute the skeleton pass on the ME.
   - Measured: it **loses** to the cache flushes, so it is off.
   - SandstoneDev saw the same: render work on the ME made the main CPU slower through bus contention.
8. **Diagnostics in `main.c`:**
   - exception trap via a kubridge kernel call (registers a default exception handler; prints registers and stack);
   - watchdog thread;
   - input record/replay (`input.rec`, TTREC1; 8 bytes per frame);
   - deterministic bench;
   - "chk" hash lines every 900 frames;
   - GE timing (send → FINISH callback);
   - per-part times; heap log with real malloc headroom (v19);
   - FPU self-test at boot, both rounding modes.
9. **ttgame.cfg keys** (one per line; a BOM or a `.txt` name is accepted):

   | Group | Keys |
   |---|---|
   | Tests | `bench`, `gebench`, `replay` (input.rec is recorded during normal play) |
   | Rounding | `fpurz` |
   | Level layout (v19) | `lvram` (old layout), `lv16`, `lvstrip` (only with lvram) |
   | Textures | `figtex16` (original 16-bit character textures), `figtex8` (default), `sky16` |
   | Render toggles | `nolight`, `nolevel`, `nofig`, `nosky`, `noprop`, `nofog`, `nooverlay`, `4:3`, `cullN`, `fogmirrorN` |
   | Game levers | `lod1` (no LOD), `elves5`, `followers3`, `brainN`, `orccapN`, `realtime` (variable dt) |

---

## 5. Level data format used on the PSP (PLV4) and the v19 layout

- `convert_level.py <00isul1.bfa> level.plv` produces **PLV4**:
  - `'PLV4'`, then `u32 numVerts, numIdx, numTex, numBatches, vertOff, idxOff, dataOff`;
  - `f32 eye[3], target[3]`; at 0x38 `texTableOff, batchTableOff`;
  - **texTable:** `{u32 fmt (0 T8+CLUT, 1 5650, 2 8888), w, h, dataOff, clutOff, swizzled, numMips, f32 lodSlope}`;
  - **batchTable** (40 bytes): `{texIndex, firstIndex, indexCount, vertexBase, aabbMin[3], aabbMax[3]}`;
  - **vertices** (24 bytes): `{f32 u, v; u32 ABGR; f32 x, y, z}`;
  - **indices:** u16 triangle list;
  - textures PSP-swizzled with mip chains.
  - 00isul1: **40445 vertices, 62274 indices, 84 textures (79 T8 + 5 5650), 204 batches**; textures ≈ 1216 KB.
  - The level is drawn with `GU_TEXTURE_SLOPE` per texture (`sceGuTexSlope`), like the PS2's Q-based LOD.
- `add_strips.py` appends a strip trailer (`'STRP'`, ..., `u32 offset`, `'STR1'`): 59964 strip indices instead of 62274 list indices. **No gain** (the data is mostly quads), but `level_v15.plv` has the trailer and is the file in use.
- **v19 default layout** (the big win, §7):
  - The level's **16-bit vertices** (LvVert16, 16 bytes: `u16 u, v` unsigned; `u32 colour`; `s16 x, y, z, pad`) and indices sit **in VRAM**: 753 KB right after the framebuffers.
    - Positions are on one global grid with a per-batch origin in the model matrix.
    - UVs: minus floor(min) per batch, scaled by a power of two into 0..32768.
    - **The GE reads 16-bit UVs as unsigned** (/32768); positions are signed. Signed UVs were the cause of the earlier "LOD bug".
  - The 30 most-used textures stay in VRAM; the other 54 (738 KB) move to RAM.
  - `lv_compact()` replaces the level file in RAM with one 828 KB block: tables, all palettes, the RAM textures. That frees about 300 KB compared with v18.
  - `lvram` restores the old layout: float vertices in RAM, every texture in VRAM.
  - The emulator image matches the float version within 42 pixels (edge noise).

---

## 6. Hardware performance — what was measured (PSP-3000, 333 MHz)

### 6.1 Frame budget (v15–v17 benches, typical 900-frame window)
- Frame ≈ 49–55 ms: game logic ≈ 47–52 ms + draw-list build 1.5–2 ms.
  - Game update (CYoGameData::Update) ≈ 32–38 ms; skeletons ≈ 8–10 ms; renderObjects ≈ 3–5 ms.
  - About 18–21 fps in the bench; heavy scenes reach 14 fps with 18+ figures.
- GE ≈ 38 ms, running in parallel with the CPU:
  - level ≈ 20 ms; characters ≈ 13 ms; fog ≈ 3 ms; props ≈ 1.3 ms; sky < 1 ms; lighting ≈ 0.
- **The CPU is the wall, and it is memory-bound:**
  - about 125 cycles per cache miss on average (~160 while the GE draws, ~64 when it is idle);
  - about 87k I-misses per frame and about 30k D-misses;
  - about 800 KB of code touched per frame; function boundaries cause about 33% of I-misses.
  - The recompiled code is about 3.6× the PS2 code: 921 KB → 3345 KB.

### 6.2 GPU/CPU contention (the key finding)
- v17 gebench (frames 601–900):
  - GPU idle: game update 24.0 ms; GPU drawing everything: 38.9 ms. **About 15 ms of the CPU's frame is lost to the GE's main-memory traffic.**
  - The level causes 9.3 ms of it, characters 4.2 ms.
  - The GE is also slowed about 11 ms by the CPU.
  - **Pixels cost nothing:** level without pixels −0.1 ms, characters without pixels −0.1 ms, no clipping 0. So **lower-resolution rendering is dropped**.
- v18 gebench:

  | Level vertices | GE ms | CPU update ms |
  |---|---|---|
  | float, in RAM | 38.1 | 36.3 |
  | 16-bit, in RAM | 36.4 | 35.3 |
  | **16-bit, in VRAM** | **24.6** | **28.9** |
  | no level at all | 18.5 | 29.0 |

  **Where the data lives matters, not its size.** From VRAM, the level costs the CPU nothing (the GE reads VRAM over its own bus).
- Earlier GE levers:
  - T8 + mips for characters: GE 46 → 38 ms, frame 58.6 → 55.6;
  - sky to T8: frees 640 KB;
  - strips: none; level untextured: none (its textures were already in VRAM).

### 6.3 CPU levers history (I-cache work)
- v5 → v8: heavy-scene logic 95.8 → 59.8 ms, from:
  - pinned memory base register (78 → 35 instructions per typical field access);
  - 64→32-bit narrowing;
  - regcache;
  - hot layout;
  - VU0 AOT.
- qsr helpers: about −1.7% (estimated). `-Os`, global or per cold function: worse.
- The fully-associative cache model shows layout could gain at most about 14%.
- Pending levers, measured by SandstoneDev on their recompiled game:
  - register arguments between translated functions (call protocol ≈ 17% of instructions; estimated −3 to −10 ms);
  - write return values back only where a caller reads them (≈ −2 ms);
  - cold attribute plus call-chain order (≈ −1 ms);
  - **dead save/restore removal (DSR)**, which is blocked by the game reading uninitialised stack in `createPickupObject`. Idea: keep the saves only in the functions whose stack frames that read can see (the sibling frames of the same parent), and remove them everywhere else.

### 6.4 Stability lessons
- **Hardware counter block 0xBC400000:** reading **word +0x50** kills the reading thread on a PSP-3000.
  - Our v10/v11 read 21 words, which crashed the game and switched the PSP off. Confirmed by SandstoneDev.
  - Words 0x00–0x4C are safe. `HWP_N` is now 20 and the counters stay **off** (`g_hwp_on` is never set).
  - Re-enabling needs the user's consent. If they agree: 20 words only, never reset, restore PROFEN, short runs, memory-stick backup first.
- **Exit order (v17+):** HOME sets `g_quit`; the main loop finishes the GE list, calls `sceGuTerm`, gives the exception handler back to the kernel, then calls `sceKernelExitGame`.
  - The callback thread does it itself after 2 s if the loop hangs.
  - Bench runs wait for **X** before exiting; never exit automatically.
  - Cause: a handler left pointing into freed memory, and automatic exits, caused power-offs in other ports.
- Watchdog lines during the first 7 s of boot are normal (slow loading).
- PPSSPP gives booleans, not costs. Its GE timing is near zero and its CPU numbers are about 2.5× optimistic. Take every number from hardware.

---

## 7. Version history (this and earlier sessions)
- **Viewer v1–v8:** level viewer (PLV1→PLV4). v8 added batch culling and reached 60 fps on the level alone.
- **TTDemo:** an earlier hand-written demo (`demo_src/main.c`, `game.h`, `world.h`). It was replaced by the recompiled original game (yo.h).
- **TTGame v5–v9:** recompiled game plus GE renderer; CPU levers (§6.3).
- **v10:** input record/replay; FPU self-test; boot hashes; hardware counters (crashed).
- **v11:** qsr; GE-sync alternation, which proved bus contention.
- **v12:** counters removed; stable for 6120 frames.
- **v13:** GM_POS16; figtex8 option.
- **v14:** figtex8 and sky T8 by default; gebench introduced.
- **v15:** level texture RAM copy released (+1.2 MB heap); strip trailer; lv16 tried (looked wrong; it was the UV sign bug).
- **v16:** gebench pixel/clip modes.
- **v17:**
  - ordered exit; bench waits for X; HWP_N = 20;
  - `fpurz` option and the two-mode FPU self-test;
  - gebench "CPU waits" mode.
  - Hardware: contention ≈ 15 ms; pixels cost nothing.
- **v18:** bench-only test of level vertices in VRAM: the win (§6.2).
- **v19 (current, sent, not yet run by the user):**
  - level 16-bit vertices and indices in VRAM by default; UV sign bug fixed; most-used textures in VRAM, the rest in RAM;
  - `lv_compact`; `lvram` fallback; heap headroom in the log.
  - gebench modes: everything / RAM textures off / level float in RAM / level indices in RAM / no level / no characters / everything with the CPU waiting.

---

## 8. External references studied (SandstoneDev, 2026)

Repos: `github.com/SandstoneDev/PS2RC`, `universal-ps2-recompiler`, `psp-developer`. All three use the "PS2RC License 1.1", which is source-available, not open-source. We used **findings only, no code**, so their licence does not bind this port. Their "Listed Disc" rule doesn't list SLUS-20578 anyway.

Useful facts taken from them:
- the counter +0x50 crash;
- exit-order and power-off causes;
- R5900 rounds toward zero;
- skipping off-screen passes was their biggest win. We checked: none of those passes run in our port;
- GE time must be measured from send to finish (as we do), not with a busy sampler;
- texture fetch traffic slows the CPU;
- overclock to **365 MHz only through the CFW's own clock call** gave −8.6% frame time; writing the PLL register yourself switched the console off;
- their recompiled open-world game reached **17–21 fps outdoors, 30 fps indoors**;
- "one-run interleaved A/B": our deterministic replay already gives low-noise A/B runs.

---

## 9. Current status (end of session)
- **v19** has been sent to the user (`TTGame_v19.zip`). Their next run:
  1. `config_gebench` → send game.txt (renamed);
  2. normal play without a cfg for a few minutes → send game.txt;
  3. report whether the ground or walls look different from v17.
- Expected: the CPU game update about 7 ms faster than v17 (≈ 29 ms instead of ≈ 36 ms), GE about 13 ms lower, frame ≈ 40–43 ms (≈ 23–25 fps).
- Open questions v19 answers:
  - what the 738 KB of RAM textures cost ("RAM textures off" mode);
  - whether indices can stay in RAM, which would free 122 KB of VRAM for textures ("level indices in RAM");
  - the real heap headroom on hardware.
- **Decisions pending from the user:**
  - make `fpurz` (PS2 rounding) the default?
  - hardware counters again (20 safe words)?
  - 365 MHz overclock as an option?

## 10. Next steps (ranked)
1. **Analyse the v19 logs.**
   - If RAM textures cost CPU time: choose which textures stay in VRAM by a better metric (screen coverage or texels fetched, not triangle count), put indices in RAM if that is free, or shrink rarely seen textures (the user approves visual changes).
   - Confirm memory headroom.
2. **Characters' GE traffic** (≈ 4 ms of CPU contention, 13 ms of GE time).
   - Options: move character vertex buffers or textures to VRAM once space allows (VRAM is full: 2047/2048 KB); reduce skinning weights per vertex; group draws.
   - Measure with gebench first.
3. **Fog pass:** 3 ms of GE time; check whether a cheaper form exists.
4. **CPU code generation:**
   - register-argument calling convention (biggest estimated gain);
   - return-value demand;
   - DSR with the sibling-frame exception;
   - cold marking.
   - Verify each by x86 hash equality (25ed6ef3, or d1a59100 for the PSP config), the emulator hash, and a hardware bench (3650c540).
5. **With the user's consent:** fpurz default (new reference hashes); 365 MHz option (CFW API only, crash-safe state file); safe counters for a diagnostic run.
6. Later fidelity work: sound and music (none yet), HUD, cutscenes/FMV, particles (presentation stubs today); the rest of the levels.
   - The user wants to revisit what the original does with actors over the collision hole at the raining-orc spawn (−101, −158) in level 1.

## 11. Method rules that worked
- One change per measurement. Use the deterministic bench (no input, fixed dt, 900 frames) and compare hashes.
- Every code-generation change must keep the x86 and hardware state hashes **identical**.
- The emulator is only for "does it run / render / crash" and for screenshots. Use `NFRAMES` builds with `run_emu.sh`; they skip the X wait.
- gebench mode cycling (a mode per 2-frame pair, reported every 300 frames, CPU update time on the odd frame) is the instrument for GE questions.
- Never exit automatically on hardware. Keep kernel code minimal. Never touch flash0, NAND, or clock and power registers.
