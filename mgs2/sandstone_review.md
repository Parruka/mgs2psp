# SandstoneDev repos: what MGS2 can learn and apply

Written 2026-10-17. Sources, cloned read-only (not into the shared folder):
- `github.com/SandstoneDev/PS2RC` @ fac73d4 (PS2RC 0.1.1, ~84k lines). It is a PS2-to-PSP static recompiler with a full GTA San Andreas PAL pack.
- `universal-ps2-recompiler` @ 1a7b196: an agent kit plus `knowledge/`, about 40 field notes with PSP-3000 numbers.
- `psp-developer` @ c4354ed: a PSP homebrew skill with 20 reference files, every fact tagged by source.

References look like `PS2RC/<path>:<line>`, `K/<note>` (a knowledge note) or `PD/<file>` (psp-developer). Anything marked **(inferred)** is my conclusion, not something the repos say. No code was copied anywhere.

TT handoff §8 already took these facts from the repos:
- the +0x50 counter crash;
- exit order;
- round toward zero;
- off-screen passes;
- serial GE timing;
- texture fetch traffic;
- 365 MHz only through the CFW call;
- 17–21 fps outdoors;
- interleaved A/B.

This file covers what is new, or what matters differently for MGS2.

## 1. Licence (PS2RC License 1.1, identical in all three repos)

It is source-available, not open source. Plain reading of `LICENSE`:

| Question | Answer | Clause |
|---|---|---|
| Read and study it | Yes | 1(a) |
| Modify it and use it to recompile a game from a disc you own, then run the result yourself | Yes, free of charge | 1(b), 1(c) |
| Put their code into our tools for Ffm's personal port | **Allowed.** The copied parts stay under their licence. | 1(d), 2(c) |
| Distribute anything containing their code | Only free of charge; credit "Based on PS2RC by SandstoneDev" plus links; keep LICENSE and NOTICE.md; no game data | 2(a)–(d) |
| "Listed Disc" restriction | Lists only SLES-52541 (GTA SA). **MGS2 SLES-50383 is not listed.** | 2(e), `games/SLES-52541_v2.01/DISTRIBUTION.md` |
| Licence ends | Automatically, if the terms are broken | 5 |

What this means:
- Ffm's port is personal use and never published, so the licence allows using their code, not only their findings.
- The cost comes only if something is ever shared. Their parts would then carry their licence and credit, and the port's own "never publish game data" rule already matches 2(d).
- **The project default is unchanged: findings only.** Whether to reuse any of their modules is Ffm's call (see the question at the end).

Third-party notes (`PS2RC/NOTICE.md`):
- The ME bring-up derives from mcidclan's MIT-licensed me-feed-graphics-engine.
- The signature database for Sony library naming is **not** shipped; you supply your own (`docs/PORTING_GUIDE.md:107-121`).

## 2. Ranked: what to apply to MGS2

### 2.1 Sound: recompile the IOP driver and run it in lockstep (new; this closes an open problem)

MGS2's music and SFX driver is `iop_main.irx` inside MODULE.IPK. Our TT method had nothing for the IOP. PS2RC solved the same problem for GTA.

**IOP recompiler** (`PS2RC/ps2rc/iop/`): translates MIPS-I IRX to C.
- It parses `.iopmod`, REL relocations and the import/export stub tables (`iop/irx.py:1-8`).
- It refuses what it cannot translate exactly (load-delay hazards, jumps out of code) instead of guessing (`iop/analysis.py:90-205`).
- The runtime HLEs these libraries (`iop/rt/iop_rt_libs.c:497-558`):
  - thbase, thsemap, thevent, intrman, timrman;
  - sifman/sifcmd RPC;
  - sysclib, ioman (read-only), cdvdman reads;
  - libsd register interface, vblank.
- **Missing:** thmsgbx, fpl/vpl, modload, IOP→EE `sceSifSetDma`, and libsd BlockTrans/AutoDMA, which only fires the handler (`iop_rt_libs.c:429-434`). Unknown imports log and return 0 (`iop_rt.c:179-184`).

**Lockstep pattern** (`games/SLES-52541_v2.01/iop/snd_glue.c`, `K/hle/sound-driver-lockstep-me.md`):
- IOP threads are coroutines on the game thread.
- Each per-frame RPC first advances the SPU2 engine by the *virtual* time since the last call, in 64-sample chunks. IRQs are delivered and IOP threads run between chunks.
- Every reply is FNV-hashed and logged every 1,024 RPCs, so the x86, emulator and PSP runs can be compared.
- The generated module was bit-exact against a hand model over 3,958 replayed RPCs.
- This is the same discipline as our state-hash rule, applied to the IOP.

**SPU2 model** (`PS2RC/audio/spu2_lite.h`):
- Has: 48 voices, 2 MB SPU RAM, bit-exact PS-ADPCM, ADSR, IRQ address, integer mix.
- **No reverb, noise, pitch modulation or AutoDMA** (`spu2_lite.h:7-9`).

**Media Engine split** (`audio/spu2_split.h`):
- The main CPU keeps a state-only engine (exact ENVX/NAX/IRQ for replies, about 40% of the full cost).
- The ME renders samples from a command ring.
- Measured: −5.8 ms/frame, shadow mode 0 mismatches over 53,852 blocks (`K/hle/sound-driver-lockstep-me.md`).

**WSOLA time-stretch** (`audio/audio_stretch.h`): the game runs slower than real time on the PSP, so audio is stretched rather than dropped. It never plays faster than 1×.

For MGS2:
1. Extract `iop_main.irx` from MODULE.IPK, list its imports, then decide recompile vs model.
2. **(inferred)** Expect gaps: reverb (audible in MGS2), BlockTrans/AutoDMA for streamed voice (VOX/DEMO), per-core IRQ, and maybe msgbox/modload.
3. 5.1 streams must be downmixed **(inferred)**.

### 2.2 Stage code images: PS2RC's overlay model, adapted to "one stage resident at a time"

**PS2RC handles Metrowerks overlays like this:**
- One translated unit set per overlay, each with its own sorted dispatch table (`ps2rc/pipeline/translate.py:567-650`).
- A hook on the game's loader calls `hle_overlay_loaded`, which switches the current table (`hle/core/hle_overlay.c:5-27`).
- `rc_dispatch` sends window addresses to the current overlay's table (`hle/core/hle_main.c:369-389`), behind a 2048-entry direct-mapped cache tagged with the overlay id.
- The width/ABI fixpoint runs across main and overlays in both directions (`translate.py:169-209`). 41 main-program functions needed facts from overlays (`K/translation/width-liveness-bugs-at-boot.md`).
- Overlay static constructors after `.text` are found through the constructor table, or they are silently never translated.

**How MGS2 differs:**
- GTA's 8 overlays are ≤95 KB and statically linked into one EBOOT, with a fixed `RC_MAX_OVERLAYS 16`.
- MGS2 has 123 relinked images of 1–3 MB each.

**What maps over (inferred):**
1. One translation per stage, loaded on demand. Only one stage is resident: 2–3 MB guest × 2.3–3.6 ≈ 5–11 MB native.
2. Switch the stage table when the loader 0x1060f0 installs the export pointer at 0x1cc8c0.
3. Seed function discovery from each image's (hash, address) export pairs, plus PS2RC-style data-pointer and lui/addiu scans (`ps2rc/analysis/functions.py:113-146`) for actor `act` callbacks.
4. The ELF's wide-argument and return sets are the union over all stages.

The dedup idea in stage_notes.md (≈5.9 MB of distinct code) has no counterpart in PS2RC. It would be our own layer.

### 2.3 Pacing: the 50 Hz double step, and a warning about 25 fps

**PS2RC has no "N logic steps per shown frame" mechanism.** GTA did not need one: it has a variable timestep (1.0 = 20 ms, clamped 0.5–3.0, `games/.../src/hle_bl.c:18-19`). MGS2 has no timestep (elf_notes.md), so this remains our own design.

What transfers:
- **Virtual time:** fixed charges per syscall, register access and back-edge. When all threads wait, jump to the next VBlank. Interrupts are delivered only at safe points (`hle/core/hle_sched.c:6-10, 629-683`). Runs are bit-reproducible on host and PSP (milestone 40.157 vs 40.156 s).
- **Forgive lag.** Pacing that only ever waits made GTA run 1.3× fast after a slow boot. Virtual time is pulled to real time, and jumps over ~250 ms are forgiven (`hle_bl.c:236-265`, `K/hle/virtual-time-pacing.md`).
- **New for MGS2, Ffm's 25 fps target:** on the PSP's ~60 Hz LCD, 25 fps shows frames for 2-3-2-3 refreshes and judders. Caps that divide 60 (30, 20, 15) are smooth (`K/hle/virtual-time-pacing.md`; they flip with `sceDisplaySetFrameBuf` and never use the display's own vblank, `hle/psp/hle_psp.c:17-20`). **(inferred)** For MGS2 that means two choices:
  - 25 fps shown, with 2 logic steps per frame: regular, but judders;
  - 30 fps shown, with 50 logic steps/s: 5 steps per 3 frames (2-2-1), smooth display but uneven motion per frame.

  Either way the logic stays at 50 steps/s, so gameplay is unchanged. Which feels better is a hardware question for later.
- Halving the logic rate is still the only way to *not* pay double CPU. That is a gameplay change and needs Ffm's consent (memory rule).
- Pattern to copy for the skipped render on odd steps: GTA's pass skips are hooks that return early but keep the bookkeeping the game reads (`games/.../src/hle_au.h:3-12`, `K/engines/offscreen-passes-double-work.md`).

### 2.4 Back-end levers: code size is speed, and more so for MGS2

Every stage is 2–3 MB of guest code. TT expanded 3.6×; PS2RC reached **2.3×** with these levers, each measured on a PSP-3000 (`K/translation/backend-levers-measured.md`):

| Lever | Result |
|---|---|
| Whole-program width analysis | 96.8% of wide ops are really 32-bit |
| Register caching | −31% |
| Branchless clamp, applied once at load | 3.89× → 3.38× |
| 128-bit, HI/LO and ACC kept in locals | → 3.00× |
| Dead save/restore removal (94–97% of pairs qualify) | → 2.57× |
| Mask only the base register, 64 KB guard areas | → 2.31×, −6% frame time |
| Return values written only where read | −8.25% code, −2 ms |
| Cold attribute plus call-chain ordering | −1.9% |

Not worth doing: `-Os` (41% slower).

Not done anywhere yet: register arguments between translated functions, which are ~17% of instructions, estimated −3 to −10 ms.

Compared with TT §6.3, the new items are the measured ratios and these mechanisms: base-register masking with guard areas, DSR, return demand, and the cold/order files (`games/.../layout/`). **(inferred)** At 2.3× a stage is ~5–7 MB native instead of ~8–11 MB. That is the difference between fitting comfortably and not fitting (§2.8).

### 2.5 Testing without the PSP (useful now, since Ffm can't test)

- **PCSX2 as oracle over PINE** (`ps2rc/oracle/pcsx2.py`, `pine.py`): read registers and memory of the original game at a breakpoint, then compare with our x86 build. The first divergence is the bug (`K/hle/reference-run-first-divergence.md`). It needs PCSX2 and Ffm's own BIOS on a PC. PCSX2's interpreter and recompiler disagree on some FPU edges, so compare against both. **(inferred)** This is the strongest available check for MGS2's VU0-sensitive logic (PCSX2 wiki: camera photos, corpse dragging).
- **Bit-exact GE software renderer:** PPSSPP PR 22414, merged into master on 2026-10-05, not in release 1.20.4 (`PD/ge-arithmetic.md:171-203`). Replay a `.ppdmp` frame headless and get device-accurate pixels and depth on a PC, without a PSP. Timing stays hardware-only.
- **Test ladder:** decoder vs objdump → synthetic programs vs PCSX2 → real functions → VU0 vectors → mutation (each rule switched off must turn a test red) → host milestone gate.

### 2.6 Renderer and GS effects

- **Boundary:** replace the renderer at its lowest stable software layer; convert VIF UNPACK streams once per mesh into a GE vertex block plus a CALLed sub-list; cache keyed by content, not address (`K/hardware/ge-3d-backend-costs.md`). VU1 is never emulated (`docs/PORTING_GUIDE.md:281-292`), the same choice TT made.
- **GE rules TT did not record:**
  - guard band |x| ≤ 8w, |y| ≤ 9.5w (2730 px Y errata);
  - skin weights must sum exactly to 128 or 32768;
  - ≤8 bones per draw;
  - a camera cache key must include the view matrix (mirrors);
  - kick every ~16 meshes, each kick being a syscall;
  - display 5650.
- **Textures:** keep T4/T8 indices, 8888 CLUT, CSM1 reorder, alpha min(255, 2a) with a doubled alpha-test reference. Load-time conversion costs 0.27–0.6 ms/frame (`K/hardware/texture-fetch-traffic.md`). This fits MGS2's PSMT4/PSMT8 + CLUT textures.
- **Effects are the weak spot for 99% fidelity.** GTA *drops* camera rasters used as textures, such as motion blur (`games/.../engine/render/psp/hle_ge.c:35, 1129`). Its colour filter is done analytically, and its glow is a native 3-pass render-to-texture.

  MGS2's thermal goggles, motion blur, codec warp and reflections need native GE copy/RTT passes. Rules from `PD/rendering.md:251-267`:
  - never sample the target you are drawing to: copy and `sceGuTexSync` first;
  - a 5551 copy has a garbage alpha bit, so use `GU_TCC_RGB`;
  - disable dither on additive glows.

  Measured: a set of post passes cost 1.27 ms per frame.

### 2.7 VU0 micro programs

- `ps2rc/vu/micro_gen.py` resolves pipeline timing at translation time: flags 4 cycles, Q 7 or 13 cycles.
- Exact over 11,264 vectors; 3.35 µs per call (`K/hle/vu0-macro-micro.md`).
- **It refuses** VU data memory (LQ/SQ/ILW/ISW), XGKICK, EFU, MAX/MINI, FTOI and status-flag instructions (`micro_gen.py:23-25`).

MGS2's lt2 lighting program (190 instructions) almost certainly reads vertices from VU memory **(inferred)**, so it falls outside their translator. TT's own `vurecomp.py` is the base. Their static-timing rules and their mutation tests are what to compare against.

### 2.8 Memory (64 MB mode)

- Plan for ~51 MB; PPSSPP shows ~5 MB more. With a 28.4 MB guest window and a 14 MB EBOOT, 7 MB were left (`K/hardware/large-memory-mode-check.md`).
- The 64 MB mode is only a request to the CFW. Check free memory at boot and show a message if it is 24 MB.
- **The code hole:** the guest's own code region is never read as data, so it can hold display lists, the texture arena and vertex caches.
  - **(inferred)** For MGS2 this is large: ELF 0x100000–0x17cf78 plus the stage image 0x1F1C00–~0x4F0000 ≈ 3.5 MB. Verify first with PCSX2 memory breakpoints that nothing reads it as data.
- The volatile 4 MB block is usable for GE lists, textures and the movie decoder. It must be locked correctly; doing so freed 4.1 MB and saved 3.8 ms.
- Guest-thread coroutine stacks must be carved from the PSP thread's own stack; a separate block powered the console off (`K/hardware/coroutine-stacks-in-thread-stack.md`).

### 2.9 Disc streaming (VOX.DAT, DEMO.DAT)

- Use a reader thread just above the game thread's priority, and answer the game's status and sync polls the way the drive would.
- Measured: mean 68.0 → 64.8 ms, 99.9th percentile 183 → 119 ms (`hle_cdasync.c`, `K/hardware/hitches-log-and-disc.md`).
- `sceIoOpen` costs 17–34 ms and at most ~8 files can be open at once, so keep the large .dat files open.

### 2.10 Movies (MOVIE.DAT)

- **Open path verified on a PSP-3000:**
  - Video: ffmpeg/libx264 H.264 Main@2.1, 480×272, 29.97 fps, no B-frames.
  - Mux: their stdlib PSMF muxer.
  - Audio: a separate IMA-ADPCM WAV that is the movie's clock, because no open ATRAC3plus encoder exists.

  Exact x264 settings are in `PD/movies-mpeg.md:23-43`.
- The ME audio offload stops the firmware's video decoder, so attach it after movies, or pause it around them.
- Their demuxer only reads PSS with PCM audio (`ps2rc/movies/pss.py:67-68`). MGS2's MOVIE.DAT is its own container, so it needs our own demuxer **(inferred)**.

### 2.11 Smaller items

- **Sony library naming by signature hash** (`tools/ps2elf/sce_sigscan.py`); no database is shipped. About 120 of GTA's hooks were library functions.
- **Trap set:** only functions that touch hardware through computed pointers get trapping accessors.
- **Safe points:** at loop back-edges, a counter with a slow path every 1024 iterations.
- **Boot strictness:** run strict, fix the first fatal line, and use named experiment switches that never become defaults. Stop after 3 identical failures.
- **Profiler:** its own cost is 3.2–4.4 ms per frame, so keep profiling builds separate.

## 3. What does not transfer

- GTA-specific pieces: the RenderWare driver hooks, its timestep, the water, sky and far-LOD code.
- Their converter GUI, EBOOT asset packer and agent-kit scaffolding: useful only if the port were ever packaged for others, which it is not.
- Their fps numbers do not predict ours. GTA's logic is ~13–15 ms per frame, and MGS2 must run it twice per 25 fps frame.

## 4. Decision (Ffm, 2026-10-17): reuse PS2RC's sound and movie code

Ffm chose to reuse PS2RC's sound and movie code. That covers:
- the IOP recompiler and its runtime (`ps2rc/iop/`);
- the SPU2 model, the ME split and the time-stretch (`audio/`, plus the ME parts of `hle/psp/`);
- the PSMF muxer (`ps2rc/movies/`).

Everything else stays findings-only. Our own recompiler (TT tools) remains the core.

Conditions:
- Copied code goes in its own folder together with PS2RC's `LICENSE` and `NOTICE.md`, and stays under that licence.
- Personal use only, no game data, as before.
- The thread that owns the mgs2 tools does the import.
