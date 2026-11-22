# Metal Gear Solid: Peace Walker (PSP): what its engine tells the MGS2 port

Written 2026-10-18 from Ffm's own disc. Findings only: no Peace Walker code or data is stored in the project (personal use, never publish). Facts below were read from the disc; anything inferred is marked **(inferred)**. Working copies of the modules live only in the ephemeral container (/home/claude/pw).

## 1. Disc

- ISO: `Desktop\mg2 port\Metal Gear Solid - Peace Walker.iso`, 1,646,002,176 bytes. PARAM.SFO: `ULES01372`, disc version 1.01, PSP_SYSTEM_VER 6.20. Bundled firmware update in `SYSDIR/UPDATE` (ignored).
- `SYSDIR/EBOOT.BIN` (5,069,472 bytes) is encrypted (`~PSP` header, module name `mgp_main`, ELF size 0x4D5950). `BOOT.BIN` is zero-filled. **The main executable cannot be read until it is decrypted** (see §7).
- `USRDIR/prx/*.prx`: 98 game modules, **all plain (unencrypted) relocatable PRX**, readable. Built with `GCC 4.0.4+allegrex-2.2.3-psp-2.11.1` (Sony SDK toolchain).
- `USRDIR/modules/`: Konami modules `kjfs.prx`, `sound3.prx`, `zlibdec.prx` are plain; the Sony libraries (libfont, libpsmfplayer, psmf, libsha*, ...) are encrypted.
- Data containers `STAGEDAT.PDT` (89 MB), `SLOT.DAT` (505 MB, with `SLOT.KEY`), `BGM.PDT` (89 MB), `VOICE*.PDT` (~200 MB), dlc `*.PDT` are obfuscated: in the PDT headers a single repeated byte stands where zeros are expected, a different byte per file **(inferred: per-file XOR-style key)**. Not decoded; Peace Walker's data formats are of no use to MGS2.
- Movies: `mpg/*.pmf`, PSMF 0015, **480x272** (stream header 0x1E,0x11 x16). `pw_op_ENG.pmf`: 10,420,224 bytes over 80.0 s (PTS 0x15F90 to 0x6DE150 at 90 kHz) = **1.04 Mbit/s** total.

## 2. Code layout: per-stage code modules linked against the main executable

This is the same scheme MGS2 uses on PS2 (a relinked code image per stage in STAGE.DAT), carried over to the PSP by the same studio.

- One PRX per stage: `w00s01a.prx` ... `w08s01a.prx` (57 files), module name `mgp_stage`. Main stages have **1.0–1.32 MB of .text**; with rodata/data/bss each needs at most **1.42 MB** of RAM. `*n.prx` variants are ~0.36 MB.
- Stage modules **import nothing from the firmware and export only module_start/stop**. Of the 19,840 `jal` in `w01s01a.prx`, 8,833 carry a normal PRX relocation (calls inside the module) and **11,007 have none** and point at addresses that are not function starts inside the module (for example 0x324A14, beyond the module's 0x138660 bytes). Checked against the decrypted main module: these targets are function entry points in `mgp_main`'s .text, given as offsets from its load address (0x324A14 and 0x73E00 both start with a stack-frame prologue there). So Konami's stage loader adds the main module's base to every unrelocated `jal` **(the fix-up step itself is inferred; it is not a firmware relocation)**.
- About **76–82% of a stage's code is shared** with other stages (16-instruction blocks of `w01s01a` found in `w01s01b` 81.7%, `w03s02a` 76.5%, `w05s07a` 78.0%; in `title.prx` only 2.2%). So each stage carries its own linked copy of the common actor library, exactly like MGS2's per-stage images.
- `r_*.prx` (title, prologue, main01/02, lobby, net...) are tiny 1,276-byte modules: resident setup that hands a table of pointers to the main module **(inferred from the disassembly of r_title.prx)**.
- Firmware imports by module (plain ones):
  - `kjfs.prx`: Konami file system over UMD with zlib (`zlibdec`), async IO (`sceIoReadAsync`, `sceIoLseekAsync`), message pipes and FPL pools, `sceKernelLoadModuleByID` (loads stage PRX from inside its archive), power lock during reads.
  - `sound3.prx`: see §4.
  - Movie modules (`title`, `flashdemo`, `ending_flow`, `w04s06b`): `sceMpeg` (23 calls incl. `sceMpegAvcDecode`, `sceMpegAtracDecode`, ring buffer), `scePsmf`, `sceAudio` channel output.
  - No plain module imports `sceGe_user` or `sceDisplay`. **The renderer is entirely inside the encrypted main executable.**

**For MGS2:**
- Konami shipped the same overlay design on this hardware with ~1.1–1.3 MB of native stage code per stage, loaded on stage change. Our plan (recompile the ELF plus each stage's code image as separate sets, switched on stage load) is the same shape. Budget note **(inferred)**: our recompiled code is larger than native compiled code (TT: 13 MB generated for one level), so a per-stage recompiled image will be several times PW's 1.3 MB and must be loaded per stage, not kept resident.
- Per-stage images duplicate ~80% of their code. If our recompiled stage images duplicate the same way, deduplicating identical functions across stages (hash the function bytes, keep one copy in a shared module) would cut the per-stage load size **(inferred, to be measured on MGS2's images)**.

## 3. CPU code style: VFPU as the vector type, little scalar FPU

Mnemonic counts in `w01s01a.prx` (283,047 instructions):

| Kind | Count | Share |
|---|---|---|
| VFPU (all `v*`, `lv/sv`, `mtv/mfv`) | 13,448 | 4.8% |
| of which `lv.q` / `sv.q` | 4,284 / 3,323 | |
| `vmov.q` 792, `vrot.q` 317, `vdot.t` 289, `vsqrt.s` 264, `vscl.q` 247, `vmmul.q` 180, `vhtfm4.q` 53, `vtfm3.t` 23, `vsin/vcos` 32 | | |
| Scalar FPU (`*.s` non-VFPU, `lwc1/swc1`, `mtc1`) | ~4,447 ops + 3,761 loads/stores | |
| `cache` instructions | 0 | |

- Game objects keep positions/vectors as 16-byte aligned quads moved with `lv.q`/`sv.q`; matrices go through `vmmul.q` and `vhtfm4.q`. The scalar FPU is used mainly for constants (`mtc1`) and scalar compares.
- `w01s01a.prx` contains no `cache` instructions (other stages not checked); any cache maintenance is in the main module or firmware calls.

**For MGS2:** MGS2's game logic uses VU0 macro mode the same way (vectors and matrices). Mapping VU0 macro ops onto VFPU quads (`recomp/vuvfpu.py`) follows the pattern Konami itself used on PSP. The TT rule still holds: results must stay bit-exact with the x86 reference, which decides whether a VFPU path is usable for a given op (VFPU `vsqrt`/`vrsq`/`vsin` are approximations).

## 4. Audio: sceSas voice mixer + ATRAC3plus streams, in its own module

Read from `sound3.prx` disassembly:

- `__sceSasInit(core, grain 256, 32 voices, output mode 0 (stereo), 44100 Hz)`. Uses SAS ADSR (`SetADSR`, `SetADSRmode`, `SetSimpleADSR`, `SetSL`), pitch, noise, PCM voices (`SetVoicePCM`), envelope readback, and the SAS reverb (`RevType`, `RevEVOL`, `RevParam`, `RevVON`). Mixed output goes through `__sceSasCoreWithMix`/`__sceSasCore`.
- Output: `sceAudioOutput2Reserve(512 samples)` + `sceAudioOutput2OutputBlocking` (the SRC output channel).
- Music and voice: ATRAC3plus through `sceAtrac*` streaming API (`SetDataAndGetID`, `AddStreamData`, `SetSecondBuffer` for loops, `DecodeData`).
- Two threads: priority 0x11 with 4 KB stack, priority 0x10 with 14 KB stack. Memory from a VPL; file reads via `sceIoOpenAsync`/`sceIoWaitAsyncCB`.

**For MGS2:**
- sceSas is a PS-ADPCM (VAG) voice mixer with ADSR, pitch and reverb, i.e. close to the SPU2 model MGS2's IOP driver targets. It is a possible replacement for the software SPU2 mix (`third_party/ps2rc/audio/spu2_lite`) that costs no main-CPU time for decoding/mixing **(inferred; SAS's exact ADSR and reverb behaviour versus SPU2 must be compared before choosing it, and SAS has 32 voices against SPU2's 48)**.
- Streams (VOX/DEMO ADPCM voices) could stay ADPCM through SAS PCM/ADPCM voices, or be converted offline to ATRAC3plus like PW's voice files **(inferred; an offline conversion is a data change, needs Ffm's consent on quality)**.
- PW keeps the sound driver in a separate module with its own threads at higher priority than the game, which matches the TT lesson (ME/async only for small independent jobs).

## 5. Video

PW plays 480x272 H.264 PSMF at about 1 Mbit/s with `sceMpeg` + `sceMpegAtracDecode`, from inside the module that owns the scene (title, flash demo, ending). This confirms the plan for MGS2's few MOVIE.DAT files: offline conversion to 480x272 PSMF (`third_party/ps2rc/movies/pmfmux.py`) and playback through sceMpeg. About 1 Mbit/s is the bitrate Konami found acceptable for this screen.

## 6. Main executable (`mgp_main`, decrypted)

Decrypted 2026-10-18 with PPSSPP's EBOOT dump (`Documents\PPSSPP\PSP\SYSTEM\DUMP\ULES01372_EBOOT.BIN` on Ffm's PC; the dump setting was switched back off afterwards). Plain relocatable PRX, same toolchain (GCC 4.0.4 allegrex, SDK 2.11.1; also objects from GCC 3.3.6 SDK 1.15.3).

- Size: .text 3.31 MB, .rodata 0.20 MB, .data 0.47 MB, .bss 1.42 MB, about **5.6 MB** loaded.
- **Memory map (read from the code):**
  - Main game heap: one FPL in user partition 2 of `0xED3000` bytes (**14.82 MB**) minus a resizable carve-out that becomes a second FPL (`mpo_main` / `mgp_main` blocks, function at 0x6E610).
  - The **4 MB volatile memory** (`sceKernelVolatileMemTryLock`, the block at 0x08400000 used by the firmware for sleep) is locked at boot and split into Konami heaps (heap 1 = rest of the block minus 0xC0000, heap 3 = 512 KB; function at 0x88D30).
  - libc heap left at the 64 KB default (`sce_newlib_heap_kb_size` absent).
  - Total ~5.6 + 14.8 + 4 = about 24 MB plus the sound/file modules: Peace Walker fits in the PSP-1000's 24 MB user RAM + 4 MB volatile, and does **not** use the PSP-2000/3000 extra 32 MB.
- **CPU clock:** a helper (0x6D9D0) sets 333/333/166 MHz (mode 2) or 222/222/111 MHz (mode 1) through `scePowerSetClockFrequency` (NID EBD177D6), each guarded by a `scePowerGetCpuClockFrequencyInt` check. So the game switches between 222 and 333 MHz at run time (which scenes use which is not traced).
- **Frame pacing:** a VBLANK sub-interrupt handler (interrupt 30, `sceKernelRegisterSubIntrHandler`) counts vblanks. The frame wait (0x776E4) loops on `sceDisplayWaitVblankStartCB` until the count reaches a target; the target setter (0x76418) maps modes 0/1/2/3 to **1/2/3/4 vblanks (60/30/20/15 fps)**, and the default set at init is **2 vblanks = 30 fps**. The number of extra vblanks a frame took (capped at 6) is stored and read at 33 sites; at least two (0x28170, 0x1155B0) subtract `base + 5 × extra vblanks` from timers, so those timers keep real time when a frame overruns **(inferred from two sites; not all 33 traced)**.
- **GE submission:** Sony's libgu is linked in (0x33D000–0x3415FF: `sceGeListEnQueue`, `EnQueueHead`, `UpdateStallAddr`, `Sync`, finish/signal callbacks, `sceDisplaySetMode`/`SetFrameBuf`). Konami's own code also **writes GE command words straight into the current list** (example at 0x7DCD4: `VTYPE`, `BASE`+`IADDR`, `BASE`+`VADDR`, `PRIM` written with `sw` and the list pointer advanced in a global context), i.e. no per-draw library call. `sceKernelDcacheWritebackInvalidateRange` is called from 73 sites (data handed to the GE or DMA).
- **No firmware import for the Media Engine** in any module (user code can't reach it directly); ME use, if any, is inside firmware codecs (sceAtrac/sceMpeg/sceSas).
- Framebuffer format, VRAM layout, vertex/texture formats per draw and draw counts are set from runtime structures, not constants; they come from the frame dump (§7).

**For MGS2:**
- 30 fps (2 vblanks) is Konami's own target on this hardware for a stealth game of this scale, with 60/20/15 available. MGS2's PAL logic is fixed-step per 50 Hz frame (elf_notes.md), so it can't borrow PW's overrun compensation without a gameplay change.
- PW lives in 24 MB + 4 MB volatile. ARK-4 on the PSP-3000 gives our port the extra memory (TT used `PSP_LARGE_MEMORY`), and the **4 MB volatile block is a further, legal pool** for the port (lock it with `sceKernelVolatileMemTryLock`; it is lost only across sleep) **(inferred: its use alongside large-memory mode must be tested on hardware)**.
- Writing GE words inline instead of calling `sceGu*` per draw is the low-CPU path Konami took; for TT-style recompiled logic where the CPU is the wall, the renderer should build lists the same way (prebuilt per-model lists + a few inline words per draw) **(inferred)**.
- Konami switches the clock down to 222 MHz when it can; the port runs at 333 MHz (no overclock, per Ffm's rule).

## 7. One real frame: PPSSPP GE frame dumps

Three dumps recorded by Ffm in PPSSPP 1.19.3 on 2026-10-18 (`ULES01372_0001..0003.ppdmp`, format version 6), parsed in the container. Dump 1 is a lighter view; dumps 2 and 3 are a heavier in-mission view and give near-identical numbers, so the table uses dumps 1 and 2. Only numbers are kept here.

| | Dump 1 | Dump 2 |
|---|---|---|
| Draw calls (`PRIM`) | 1,754 | **15,471** |
| Vertices | 11,510 | 76,285 |
| Vertices per draw | mostly 4–7 | 32% have ≤3, 59% have 4–7 |
| Primitive | 92% triangle strips, rest sprites/fans | 99% triangle strips |
| Skinned draws (vertex weights) | 30 | **14,071** |
| Bone matrices uploaded (`BOFS` + 12 floats, one matrix each) | 32 | **2,684** |
| Morphing | none | none |
| Hardware lighting on | 1,753 | 15,470 (90% with 4 directional lights) |
| Fog on | | 15,001 |
| Texture changes between draws | 205 | 1,029 |
| CLUT loads | 346 | 1,662 |
| Unique vertex bytes read by the GE | 375 KB | 507 KB |

**Formats.**
- Vertices: positions **s16**, UVs **s16**, normals **s8**; skinned meshes have no vertex colour and **u8 weights, 2/4/8 per vertex** (7,525 / 4,194 / 1,968 draws in dump 2). Static scenery uses s16 position + s8 normal + 4444 colour. HUD/2D uses f32 or through-mode s16 with 8888 colour. Indices are used almost nowhere (64 draws, u16).
- Textures: almost all **CLUT T4 and T8 with 32-bit (8888) palettes**; sizes 32x32 to 256x256 (a few 512x512 T4). Texture function modulate with alpha, alpha test GEQUAL on all draws.
- **Vertex data stays in RAM** (every `VADDR` points into main RAM).

**VRAM map (from the frame state; offsets from 0x04000000):**

| Range | Use |
|---|---|
| 0x000000–0x088000 | Two framebuffers, 512-wide, **16-bit 5551** (0x44000 each), swapped every frame |
| 0x088000–0x0A8000 | Small 64x64 offscreen render targets (8888 at 0x88000, 0x8C000, 0xA2000; 5551 at 0x90000), drawn to and then sampled as textures |
| 0x0A8000–0x0EC000 | 16-bit depth buffer, 512-wide |
| 0x0EC000–0x200000 | **Texture pool, ~1.08 MB**: 38–47 distinct textures per frame live here; another 6–21 textures (90–280 KB) are read from RAM |

Two draws per frame sample a framebuffer as a 512x512 texture (a full-screen effect).

**For MGS2:**
- **Formats line up.** MGS2's PSMT4/PSMT8 textures with 32-bit CLUTs, s16 positions, 4096-scaled UVs and 4-weight evm skinning map onto exactly what Konami used here (T4/T8 + 8888 CLUT, s16 pos/UV, s8 normal, u8 weights up to 8). No requantising, no format conversion beyond repacking (inferred, matches feasibility.md §2).
- **16-bit framebuffer and depth.** Konami chose 5551 colour + 16-bit Z to fit two buffers, Z and a 1 MB texture pool in 2 MB. Our renderer should start from the same split; 32-bit colour would cost 0x88000 more (inferred).
- **Draw count is not the limit Konami worried about:** 15k tiny strips per frame at a 30 fps target. Hardware skinning with per-matrix `BOFS` uploads and hardware lighting/fog on every draw: the GE does the transform and lighting work. For a CPU-bound port this is the model: send matrices and let the GE skin and light, instead of skinning on the CPU (TT lesson: ME/CPU skinning was slower).
- **Vertices in RAM.** PW keeps vertices in RAM and textures in VRAM, the opposite of TT's measured best (vertices in VRAM saved ~15 ms of CPU). PW's native code has more CPU headroom than recompiled code, so the TT rule still applies to us; VRAM is the trade: whatever we put there comes out of PW's ~1 MB texture pool.
- PW's 64x64 offscreen targets show the GE render-to-texture path is in use for effects; MGS2's GS effects (goggles, blur, codec warp) would use the same mechanism.

## 8. Data install

The game's "install" (seen on Ffm's PPSSPP memory stick, `PSP/SAVEDATA/ULES01372DAT` etc.) copies the streamed files byte-for-byte to the Memory Stick: `BGM.PDT` (89 MB), `VOICEBF/PS/RT.PDT` (202 MB), `BRIEFING.DAT`, plus DLC packs. Stage data (`STAGEDAT.PDT`, `SLOT.DAT`) stays on the UMD. So Konami moved **the continuously streamed audio** off the UMD to avoid seeks during play. The game also offers a bigger install (849 MB, done by Ffm the same day) that adds `STAGEDAT.PDT` (89 MB) and `SLOT.DAT` (505 MB); both have the disc file's exact size and match the disc byte-for-byte in their first and last 4 MB, so they are plain copies (no repacking or decompression). The file layer (`kjfs`) therefore serves any archive from either medium, and the install is purely about seek and load time. For MGS2 on Memory Stick everything is already on one medium; the lesson is to keep stream reads (VOX/DEMO) on their own read thread with large sequential reads, like `kjfs` does with async IO (inferred).
