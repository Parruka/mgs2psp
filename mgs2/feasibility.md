# MGS2 Sons of Liberty (PS2) to PSP-3000: feasibility

Written 2026-10-17. Target disc confirmed by Ffm from the ISO root (`modules/`, `mgs2/`, `SYSTEM.CNF`, `SLES_503.83`). Every fact below has a source; anything inferred is marked **(inferred)**. Personal use only: no game data or game-derived files leave this project.

## 1. Edition

| Region | Serial | Notes | Source |
|---|---|---|---|
| **PAL (Ffm's disc)** | **SLES-50383** | English/French/German, DVD-5, 8 Mar 2002, 50 Hz only, Dolby Digital. The European box also has a bonus "Making of" DVD. SLES-50384 / SLES-50385 are separate PAL entries. | [psxdatacenter SLES-50383](https://psxdatacenter.com/psx2/games2/SLES-50383.html) |
| NTSC-U | SLUS-20144 (ELF `SLUS_201.44`) | The build most public RE work targets. | [Camamell/MGS2](https://github.com/Camamell/MGS2) |
| NTSC-J | SLPM-65077 (Premium Package), SLPM-74255 (The Best), SLPM-66792 (20th Anniversary) | | psxdatacenter entries for [65077](https://psxdatacenter.com/psx2/games2/SLPM-65077.html), [74255](https://psxdatacenter.com/psx2/games2/SLPM-74255.html), [66792](https://psxdatacenter.com/psx2/games2/SLPM-66792.html) |

Consequence: every address published for MGS2 (Camamell's notes, mgsdevwiki) is for NTSC-U or unknown. **All of them must be re-found in `SLES_503.83`**; the formats should match, the addresses will not (inferred, standard for regional builds).

"50 Hz only" matters: the PAL game's timing base is 50 Hz. Whether its game logic steps per field (50/s) or per frame (25/s) is unknown; it decides how the TT target of 25 fps maps onto this game. To be measured from the ELF.

## 2. What is known about the engine and data (public sources)

**Disc files** (Camamell `re/notes/formats.md`, NTSC-U): `STAGE.DAT` (stages, enciphered archive), `DEMO.DAT` (cutscene demos, sector-aligned, not enciphered), `VOX.DAT` (3407 voice clips), `MOVIE.DAT` (IPU video + 44.1 kHz ADPCM), `FACE.DAT` (codec faces), `CODEC.DAT` (451 GCL records), `MODULE.IPK` (contains IOP driver `iop_main.irx`).

**Stage archive.** STAGE.DAT is enciphered; readers exist: Camamell's `mgs2-formats` (Rust), [Jayveer/Arsenal](https://github.com/Jayveer/Arsenal), [Joy-Division/tools-mgs](https://github.com/Joy-Division/tools-mgs) `dat-extract_enc`. Names are 24-bit "kojima hashes" with collisions; only the first letter of an extension is stored. Name dictionary: [JoyDict](https://github.com/Joy-Division/JoyDict). Stage IDs: `w00a`–`w04c` tanker, `w11a`–`w61a` plant, `d..t`/`d..p` demos, `r_tnk0` resident ([mgsdevwiki](https://www.mgsdevwiki.com/wiki/index.php/Metal_Gear_Solid_2)).

**Stage file types** (handler table, Camamell): `b` resident **code image**, `c` cv2, `e` evm (skinned cutscene mesh), `f` far, `g` gcx script, `h` hzx collision, `k` kms model, `l` lt2 lighting, `m` mar motion, `p` unknown, `s` sar, `t` tri texture set, `z` zms.

**Formats that map well onto the GE** (Camamell):
- Textures: PSMT4/PSMT8 with 32-bit CLUTs, palettes picked by `CSA`. The GE has native T4/T8 + CLUT, so no requantisation like TT's median-cut is needed (inferred).
- Models: i16 positions (mm), UVs scaled 4096, bone hierarchy, triangle strips with a restart bit. The GE takes s16 positions natively; TT's 16-bit level path applies (inferred). evm skin: 4 weights per vertex, 8-bit; GE hardware skinning takes up to 8 weights (TT `ge_model.h`).
- Collision `hzx`, motion `mar` (12-bit packed quaternions), lighting `lt2` computed per vertex by a **VU0 micro program** (190 instructions).

**Scripting:** GCL bytecode (`.gcx`), RPN expressions, two typed variable banks. Decompilers: [oct0xor/gcx_decompiler](https://github.com/oct0xor/gcx_decompiler), tools-mgs `gcx-decompile`. Like TT's Lua 4, the interpreter is EE code and is recompiled as-is (inferred).

**Sound:** sequenced music and SFX through Kazuki Muraoka's MGS1-family driver (SDX banks holding MDX sequences + WVX samples, in STAGE.DAT), running on the IOP; streamed ADPCM voices in VOX.DAT/DEMO.DAT, some 5.1 ([HCS thread 1](https://www.hcs64.com/mboard/forum.php?showthread=62094), [thread 2](https://www.hcs64.com/mboard/forumlong.php?showthread=64194)).

**Video:** cutscenes are mostly real-time demos from DEMO.DAT (evm meshes). MOVIE.DAT holds IPU (MPEG-2) video. The PSP decodes only H.264 baseline in PSMF through sceMpeg ([PPSSPP docs](https://www.ppsspp.org/docs/development/ppsspp-internals/mpeg)).

**Emulation hints** ([PCSX2 wiki](https://wiki.pcsx2.net/MGS2)): bugs fixed by switching the VU0 recompiler (camera photos, corpse dragging, aim twitch) mean game logic depends on exact VU0 behaviour, i.e. the TT VU0 float model matters here too. GS-dependent effects: thermal goggles, motion blur, codec screen warping, reflective water, blending-sensitive skin colour.

**Existing RE work:**
- [Camamell/MGS2](https://github.com/Camamell/MGS2): rebuild of the Tanker in Rust/Bevy from the NTSC-U disc; Ghidra project local-only. Its format notes are the best public reference. No licence stated, so read it for facts, do not copy its code.
- No public MGS2 PS2 decompilation or static recompilation found.
- GirianSeed/mgs2old: early matching decompilation of SLUS_205.54 (Substance NTSC-U), binary-derived; function names usable as hints (web_research.md §4).

## 3. How the TT method transfers

| TT piece | MGS2 | Status |
|---|---|---|
| R5900 to C recompiler + passes (`recomp.py`, `narrow.py`, `regcache.py`, ...) | Same CPU | Reusable; re-target load base/file offset to SLES_503.83 |
| Recompile set = call closure from level boot | **Code also lives in per-stage `b` code images in STAGE.DAT**, loaded at fixed addresses (w00a at 0x1F1480 on NTSC-U) | Biggest structural difference: recompile the ELF plus every stage's code image as separate sets, switched on stage load |
| VU0 AOT (`vurecomp.py`) + float model | lt2 lighting and probably skinning/matrix work | Reusable |
| Presentation layer stubbed, new GE renderer | Same plan | New renderer for kms/evm/tri/lt2 |
| FPU rules (round to zero, no DAZ on Allegrex) | Same CPU | Reusable as-is |
| Deterministic bench + state hash x86/emu/HW | Same | Reusable; needs MGS2 input replay |
| Lua 4 snapshot, BFF/PSWB/YO22 formats, all TT addresses | — | Not reusable |
| Sound | IOP driver, not EE | New: port or reimplement the MDX/SDX driver on PSP (TT stubbed sound) |

## 4. Main risks

1. **Memory.** PS2: 32 MB EE RAM + 4 MB GS VRAM + 2 MB IOP RAM. The TT port already ran a 32 MB guest image on Ffm's PSP-3000 with `PSP_LARGE_MEMORY` (`src/psp_game/Makefile:8`, handoff §4), but TT's generated code was 13 MB for one level. MGS2's ELF plus all stage code images may push code size past what fits next to a 32 MB guest image. Mitigation (inferred): allocate guest memory to what MGS2 actually uses (measure), load each stage's recompiled code on demand.
2. **CPU.** TT's recompiled logic took 24–52 ms per frame at 333 MHz and reached 18–25 fps; it is memory-bound. MGS2 runs many guards with vision cones, physics, and a large scripted world. Expect the same wall; the TT levers not yet built (register-argument calling convention, etc.) become required early.
3. **50 Hz PAL timing (read from the code, not yet measured; elf_notes.md).** Logic steps once per loop with no time delta and waits for one vblank, so the PAL game runs up to 50 steps/s and slows down when a frame overruns. At 25 fps on the PSP the game would run at half speed unless the logic steps twice per displayed frame (double the CPU work). Halving the logic rate is a gameplay change and needs Ffm's consent.
4. **GS effects.** Thermal/night-vision goggles, motion blur, reflections, rain, codec warping use framebuffer tricks. The GE can render to texture, but each one costs GE time and eats into the 99% fidelity target (corrected 2026-10-23, audit L10: Ffm's target is 99%).
5. **Sound driver on the IOP.** Not covered by the EE recompiler. Needs its own work (recompile the R3000 driver, or reimplement MDX/SDX playback on PSP).
6. **FMV.** IPU MPEG-2 must be converted offline from Ffm's disc to H.264 PSMF, or decoded in software. Few files are affected (MOVIE.DAT) (inferred from the format notes).
7. **Disc size and streaming.** DVD-5 data streamed from the Memory Stick: VOX/DEMO streaming needs a read-ahead thread; load times change.

**Verdict (inferred):** feasible as the same kind of project as TT, and better documented on the data side (formats already decoded publicly). Harder on code size (per-stage overlays), sound (IOP) and effects. A sensible first goal mirrors TT: one Tanker stage (`w00a`, deck) running the original logic with a new GE renderer.

## 5. First steps
1. Dump the files in §6 and record them in `.notes/inputs.md`.
2. Parse `SLES_503.83`: segments, entry point, `gp`, sizes; check for symbols. Re-target `recomp.py`.
3. Decipher STAGE.DAT (method from the public tools), list every stage's files, extract the `b` code images and their load addresses; measure total code size.
4. Find the frame loop, the stage loader and the GCL interpreter in the PAL ELF; measure the logic tick rate (50 or 25 Hz).
5. Recompile ELF + `w00a` code image; boot to the deck on x86 with a state hash, then emulator, then hardware.

## 6. What to dump from the disc (first batch)
- `SLES_503.83` (the ELF) and `SYSTEM.CNF`.
- A listing of `mgs2/` and `modules/` with file sizes (no contents yet).
- Received 2026-10-17: ELF, SYSTEM.CNF, listing (see elf_notes.md). Next: `mgs2/stage.dat`.
- Later: `DEMO.DAT`, `VOX.DAT`, `FACE.DAT`, `CODEC.DAT`, `MOVIE.DAT`, `MODULE.IPK` (contents of `modules/`).

## 7. Open questions for Ffm
- Targets (answered by Ffm 2026-10-17): **99% fidelity if possible, 25 fps minimum, 30+ fps if possible.**
- Language (answered by Ffm 2026-10-17): **English.**

## 8. Peace Walker findings applied to the plan (2026-10-18)
Source: `peacewalker_notes.md` (Konami's own PSP engine, read from Ffm's disc). Items marked **consent** change visuals, audio data or clocks and wait for Ffm's word; the rest are plan changes that keep the game exact.

| PW finding | What it changes in the MGS2 plan | When |
|---|---|---|
| Per-stage code PRX (1.0–1.3 MB native), ~80% of each stage's code duplicated across stages | Confirms: ELF resident, one recompiled code set per stage loaded on stage change (overlay), never all stages resident. Add a dedupe step: hash each recompiled function's guest bytes plus its relocation targets, keep identical functions once in a shared set. Measure on MGS2's 27 boot-path images first. | After the timing harness, before the second stage |
| PW fits in 24 MB + the 4 MB volatile block (`sceKernelVolatileMemTryLock`) | A further legal 4 MB pool next to `PSP_LARGE_MEMORY` (lost only across sleep). Candidates: IOP RAM 2 MB + SPU RAM 2 MB, which the game never sees directly. Must be tested on Ffm's PSP together with large-memory mode. | When the memory budget gets tight (now: 32 MB guest + 12.7 MB code + ~5 MB runtime) |
| Clock switched 222 ↔ 333 MHz with `scePowerSetClockFrequency` | The official call, not a register write, so it is within the rules, but the clock is Ffm's decision (**consent**). The timing harness reports the clock it runs at and changes nothing. | Ask Ffm with the first hardware timing |
| 30 fps (2 vblanks) target, timers corrected for overrun | MGS2 PAL logic is a fixed 50 Hz step with no time delta, so PW's compensation can't be borrowed without a gameplay change (**consent**). 25 fps still means two logic steps per displayed frame; the timing harness decides it. | Decided by the harness |
| GE words written inline into the list, no `sceGu*` call per draw | Renderer builds prebuilt per-model lists and writes a few words per draw (TT lesson A5–A8 agrees: CPU is the wall). | Renderer |
| 15k strips per frame with hardware skinning (`BOFS` matrices, u8 weights), hardware lighting (4 lights) and fog | MGS2's evm skinning and lt2 lighting go to the GE: send bone matrices, let the GE skin, light and fog. No CPU or ME skinning (TT measured that slower). | Renderer |
| T4/T8 textures with 8888 CLUT, s16 position/UV, s8 normal | MGS2's PSMT4/PSMT8 + 32-bit CLUT and s16 data map 1:1: repack only, no requantising, so no visual change. | Renderer |
| 5551 framebuffers + 16-bit Z + ~1.08 MB texture pool in VRAM | Starting VRAM split for the renderer. 5551 versus 8888 output is a visual choice (**consent**): 8888 costs 0x88000 of the texture pool. PW keeps vertices in RAM; TT measured VRAM vertices ~15 ms faster for recompiled code, so we keep TT's rule and take the space from the pool. | Renderer |
| 64x64 render targets sampled as textures | The GE path for MGS2's GS effects (goggles, blur, codec warp). | Effects |
| sceSas: hardware PS-ADPCM voice mixer, ADSR, pitch, reverb; 32 voices | Option to replace the software SPU2 mix (`spu2_lite`) and save CPU. SPU2 has 48 voices and its own ADSR/reverb, so it needs a measured comparison first (**consent** if it sounds different). | After the logic runs at speed |
| ATRAC3plus voice/music | Converting VOX/DEMO offline is a data change (**consent** on quality). Default: keep the original ADPCM. | Not planned |
| Movies: 480x272 PSMF at ~1 Mbit/s via `sceMpeg` | Confirms the MOVIE.DAT plan: offline conversion from Ffm's disc with `third_party/ps2rc/movies/pmfmux.py`, played with `sceMpeg`. ~1 Mbit/s is Konami's own rate for this screen. | Movies |
| Streamed audio installed to the Memory Stick; `kjfs` reads async with large sequential reads | Everything is on the Memory Stick already. Keep VOX/DEMO stream reads on their own read thread with large sequential reads (the disc pack, `tools/make_pak.py`, keeps each file's ranges contiguous). | With sound on the PSP |
| VFPU used as the vector type (`lv.q`/`sv.q`, `vmmul.q`) | VU0 macro ops may map to VFPU (`recomp/vuvfpu.py`) only where the result is bit-exact with the x86 reference (`vsqrt`/`vrsq`/`vsin` are approximations, so not those). | Codegen, after the first timing |

## 9. Code size of the full game (audit H1, measured 2026-10-23)

**Measured** (psp-gcc 15.2 with v23's `genf.txt` flags, -O2, no PGO, no function sections; the chain gives byte-identical copies of
v23's headers before the native hooks; scripts and objects in /home/claude/h1size, container only):

| Unit | Form | Functions | Guest instructions | PSP .text | Bytes per guest instruction |
|---|---|---|---|---|---|
| ELF | reduced (covered by the scripted route) | 1,119 of 2,303 | 84,387 | 2.91 MB | 34.4 |
| ELF | full | 2,303 | 126,139 | 4.20 MB | 33.3 |
| w00a (Tanker deck) | reduced | 987 of 6,657 | 101,835 | 3.45 MB | 33.9 |
| w00a | full | 6,657 | 526,972 | 16.33 MB | 31.0 |
| w11a (first Plant stage) | full | 8,156 | 697,062 | 21.88 MB | 31.4 |

- With PGO (as delivered) the covered code is 8-9% smaller: v23 gen.o 2.68 MB, ovl_w00a.bin 3.14 MB.
- Split of the full objects: w00a covered 3.13 MB, not covered 12.96 MB (5,670 functions); ELF covered 2.63 MB, not covered 1.31 MB.
- **It does not fit.** v23's image is about 43.2 MB (text 3.94 MB; bss 39.2 MB with the 32 MB guest RAM and the 4.375 MB overlay
  arena) in a user partition of about 51 MB. Full ELF + full stage would make it about 56.5 MB (w00a) or 62 MB (w11a): 6-11 MB over,
  before the renderer's and the audio's buffers. The audit's projection (w00a 18, ELF 4.0, w11a 24 MB) was close.
- **Memory stick speed on the PSP** (prof/hw_v23/log.txt, the stage overlays): 3.14 MB in 563-566 ms, 1.77 MB in 327 ms, 0.69 MB in
  128 ms (5.4-5.6 MB/s); 11.5 KB in 23 ms (a fixed cost of about 20 ms per file loaded).

**What it means.** Every timing so far is valid for the code it ran (the reduced harness is a sound timing tool). But the full game
cannot keep every stage function compiled in memory: some code must run in another form or be loaded when needed.

**Options** (sizes and speeds marked [I] are estimates, not measured):
- **A. An exact interpreter for the functions that are not compiled.** About 0 MB of code per stage (the guest code is already in
  guest RAM). Compiled code is about 8.5 PSP instructions per guest instruction, so code that runs once costs an I-cache miss every
  two guest instructions; an interpreter whose loop stays in the I-cache [I: 25-40 cycles per guest instruction] is then about as fast,
  and 2-4x slower than compiled code in loops [I]. No pauses. It must match the compiled forms exactly (the same poll points and
  clock charges, the C forms of VU0 math, not the VFPU groups), and the x86 reference must use the same form for each function.
- **B. Compiled code loaded from the memory stick on first use** into a code cache (calls through a table; functions with a frame on
  any stack stay loaded). Full speed once loaded, but each load costs about 20 ms plus 0.2 ms per KB, so a burst of new code is a
  visible pause unless a read-ahead thread loads it first (per-area lists from play profiles).
- **C. A smaller compiled form for cold code** (-Os, memory and 64-bit operations through helper calls) [I: 1.5-3x smaller, slower].
  Not enough alone (w11a's cold code would still be 6-12 MB).
- **D. A JIT on the PSP** (guest code translated at first use into a code cache). Fast after the first use, no reads; but a second code
  generator that must be bit-exact with the first. The largest effort and risk.

**Recommendation.** A, with B kept for later only if needed. Each stage overlay holds the compiled hot set (PGO and code order as
now), chosen from play profiles of several routes; everything else runs in the exact interpreter. The PSP harness counts the
guest instructions each interpreted function ran, so the next build compiles the ones that matter. This also replaces today's
`rt_missing` stop, so the game no longer stops on code the test routes never reached. Next facts before building it: the working
set of real play on the deck (x86 runs with scripted walking, actions and guard reactions: functions entered, guest instructions,
their compiled size), and the interpreter's real speed on the PSP (one probe).

**Real play measured (2026-10-23, speed_notes.md §42, x86 play probe):** 19 sessions of free play on the Tanker deck (about 22
minutes: tours, alerts, a game over and continue, menus, codec, random play) plus the timed tutorial ran 3,247 functions, 8.58 MB
of PSP code (ELF 2.31 + w00a 6.27). 99.9% of free play's instructions are in 4.01 MB, 99.99% in 5.20 MB. Compiling the hottest
functions into today's room (ELF 2.31 MB, w00a overlay bodies 4.2 MB) leaves an interpreter 21 instructions per step on average
in free play (76 at most): its speed hardly matters on the deck. New modes are what matters: a mode no profile run reached runs
whole in the interpreter until the next build (the codec would be 54 k instructions per step, about 4-6.5 ms [I]). Option A stands,
with profile runs that cover each stage's modes. Also found: free play runs 1.07-1.27 M guest instructions of logic per step
against the timed tutorial's 0.92 M, and on the PSP the logic time follows that count, so the deck in free play is likely about
78-84 ms per step on v24's code, not 70 [I: to be measured].
