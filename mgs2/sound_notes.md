# MGS2 sound / IOP side: facts (2026-10-17)

## MODULE.IPK
- `modules/module.ipk` (361,520 B) is not compressed, only XOR-scrambled. Format, read from `PLD.IRX` (the IOP loader the EE starts with `SifLoadModule`):
  each entry = 16-byte header + body; key starts at 0x7e8a6b4c for every header and continues into the body, each 32-bit word is XORed with the key, then key += 0xa84e6b2e. Header word 0 = body size (0 ends the list).
- The EE (0x100310) reads MODULE.IPK into EE RAM at 0xC00000 and passes that address to PLD.IRX as an argument string (`%08x`).
- 8 modules, decoded to `/mnt/project-files/mgs2/disc/modules/ipk/` (game data, personal use only):

| File | Module name | Size | Notes |
|---|---|---|---|
| sio2man.irx | sio2man v2.04 | 6,176 | Sony |
| padman.irx | padman v4.02 | 43,872 | Sony |
| mcman.irx | mcman v2.1a | 90,544 | Sony |
| mcserv.irx | mcserv v2.0e | 7,376 | Sony |
| libsd.irx | Sound_Device_Library v1.04 | 26,320 | Sony libsd |
| jsifman.irx | KCEJ_SIF_MANAGER v1.01 | 4,912 | Konami SIF layer, exports `jsifman` (18 functions) |
| cdbios.irx | KCEJ_DISC_BIOS v1.01 | 7,152 | Konami disc layer, exports `cdbios` (8 functions) |
| iop_main.irx | (no name) | 175,024 | **the game's IOP driver**: sound (libsd), disc streaming (cdbios), timers; text 0x1a410, data 0x11b0, bss 0xc7b24 |

## iop_main imports (from the module's stub tables, PS2RC `ps2rc.iop info`)
libsd 4 5 6 7 9 10 11 17 18 19 20 23; jsifman 4 9 12 17; cdbios 4 5 6 7; intrman 6 9; ioman 4 5 6 8; loadcore 5; stdio 4; sysclib 12 14 23; thbase 4 6 8 24 25 26 33 34 39; timrman 4 20 22 23; vblank 4 5.

## PS2RC IOP recompiler on these modules (third_party/ps2rc, Ffm's "Reuse sound/movie" choice)
- `info`: iop_main 162 functions, 58 callbacks, no errors (45 `break` warnings: runtime traps, usually divide checks); jsifman 19 functions, no issues; cdbios 23 functions, no issues.
- `gen`: all three translate to C (iop_main 33,018 lines) and compile with gcc on x86 against `rt/`.
- Imports the PS2RC runtime does **not** implement yet (to add on our side):
  - jsifman needs `sifcmd` 4 8 10 12 13 and `sifman` 7 8 (SIF command setup / send, IOP→EE `sceSifSetDma` / `sceSifDmaStat`);
  - cdbios needs `cdvdman` 24 28 37 39 75;
  - iop_main needs `intrman` 6 and `loadcore` 5.
- libsd: all 12 used functions exist in the runtime, but BlockTrans in AutoDMA mode only fires the handler, and the SPU2 model has no reverb (the driver calls SetEffectAttr, 23). Both matter for MGS2 (streamed voice, audible reverb).
- Function names for import numbers come from the standard export tables; check them against the IRX export tables before relying on a name.

## Next
1. EE side: find the EE code that talks to jsifman (SIF RPC / command IDs) and the sound/stream request format.
2. Add the missing HLE functions; build the lockstep harness (one RPC per frame, reply hashes) as in PS2RC.
3. Reverb and streamed-voice DMA in the SPU2 model.

## Streams in iop_main (found 2026-10-18 on the x86 probe)
- EE sound commands are 32-bit words handled by iop_main 0x509fc (byte 3 = command). Stream commands: 0xf0 / 0xf2 = total size of stream 0 / 1 (low 24 bits << 4); 0xf1 / 0xf3 = "block ready" for stream 0 / 1 (the IOP then fetches 16 KB from the EE into 0x110f70 / 0x114f70 (stream 0) or 0x118f70 / 0x11cf70 (stream 1)); 0xf4 / 0xf5 = stream attributes, low 16 bits = sample rate (0xac44 = 44100 for VOX, 0xbb80 = 48000 for the demo).
- VOX voices (0xf5, mode 0): ADPCM on core 1 voices 20 / 21 (key-on mask 0x300000), double buffer in SPU RAM 0x5020 / 0x5820 and 0x6020 / 0x6820 (0x800 bytes each), refilled by sceSdVoiceTrans while the driver polls NAX. Needs a voice model: spu2_lite (third_party/ps2rc/audio), bound in runtime/iop_host.h, state-only for now.
- Demo audio (0xf4 with bits 20-23 = 1): a separate engine at iop_main 0x583f0 (state at IOP 0x1230e0, 2..8). It plays PCM through sceSdBlockTrans(core, 0x10 = write + loop, 0x122030, 4096) (the core's AutoDMA input) and polls sceSdBlockTransStatus for the DMA address to refill the idle 2 KB half. The PS2RC runtime returned "finished" (1), so the engine never left state 6 and the Tanker opening (d00t) hung. iop_host.h now models the loop: 192 bytes per ms, address steps of 512 bytes (assumed).
- Not done yet: mixing the AutoDMA PCM and the voices into an audio output; reverb.

## AutoDMA position model (audit M7, 2026-10-23)
- **libsd v1.04 (disassembly of the disc's libsd.irx):** sceSdBlockTransStatus (export 20, 0x1c80 -> 0x2944) returns `(BlockTransBuff[core] << 24) | (MADR & 0xffffff)` while the core's ADMA bit is set (0xBF9001B0 + core x 0x400); the DMA channel is 4 (core 0, MADR 0xBF8010C0) or 7 (core 1, 0xBF801500). Loop mode: one DMA per half of the buffer in 64-byte blocks (CHCR 0x01000201); the interrupt handler flips the half and restarts at buffer + half x size/2.
- **Our model (A, runtime/iop_host.h sd_bt_status):** the playback position in 512-byte steps, 192 bytes per ms from the start. An assumption.
- **PCSX2's model (B; pcsx2/SPU2/ReadInput.cpp and Dma.cpp, GPL, read as reference only):** the core reads 1,024 bytes (256 samples left, then right) from the DMA at each 256-sample boundary while a request is pending, and once at the start; MADR rises 0x180 bytes per sample as those bytes are counted. So the DMA address runs 256-512 samples (5.3-10.7 ms) ahead of playback. The hardware must run ahead, since the core plays from its input buffer after the DMA fills it [I].
- **x86 test** (private copy `/home/claude/m7`, ephemeral; `MGS2_ADMA_PCSX2=1` = B; deck route):
  - A is the reference (v21 refs 252 / 252). In B, up to vb 15,539 the only EE word that differs is the demo-audio position 0x1c6d8c (B ahead by 0x200 at about half the vblanks, from the first stream at vb 4,256).
  - Vb 15,540: the stream ends (status 0x1c6d84 0x40 -> 0, position 0xa59300 in both). Its 256-sample block counter 0x1c6d94 stops at 0xb04e in A and 0xb04d in B: B saw the end one block earlier.
  - Vb 15,542-15,544: the next scene starts differently. From vb 15,544 about 62,000 words differ, mostly in the heap (0x13e0000-0x1810000, 0x1b10000), pointers that differ by multiples of 0x10000: allocations at other addresses. Not a time shift: in the still stretch at vb 15,698-15,703, A and B differ by about 61,100 words at every offset of -2 to +2 vblanks.
  - It never comes back together: the VFPU fallback count first differs at vb 15,738; at vb 25,200 vfb 461,571 (A) against 461,616 (B).
  - Visible effect: none found. The game prints nothing (no kputs in either run), so the frame where the next scene appears was not compared [I: at most a frame apart].
- **What it means:** the position model is part of the exact game state, and neither model has been checked on a real PS2. Recommendation: keep A until the audio output is built. The mixer needs the core's input buffer then anyway, and the hardware-like timing (B) can come in with it, so the reference hashes change once.

## SPU2 features the game uses (audit H4 census, 2026-10-23)
- **Method:** the same private build with `MGS2_SD_CENSUS=2`: every libsd call that sets something other than a voice's volume, pitch, envelope, start or loop address, and each vblank the voices sounding (ENVX > 0). Deck and crew deck routes to vb 25,200; hashes unchanged.
- **libsd's init** (export 4) sets VMIXL / VMIXR / VMIXEL / VMIXER to all 24 voices on both cores, MMIX core 0 0xff0, core 1 0xffc.
- **The game at vb 0:** both cores reverb mode 0x105 (Hall, work area cleared), effect enable on both, EEA core 0 0x01ffff and core 1 0x1fffff, EVOL 0x2000, MVOL 0x3fff; PMON 0 and NON 0 (no pitch modulation, no noise); MMIX core 0 0x0f0c (voices dry and wet), core 1 0x0fcc (voices dry and wet, core 0's output dry, its own input dry); AVOL core 1 0x7fff; BVOL 0 on both cores; core attribute 0xa (SPDIF) 0x880.
- **During the run:** BVOL core 1 0x7fff while a cutscene stream plays (vb 4,256-15,539 and 20,145-22,994), 0 otherwise. The reverb sends VMIXEL / VMIXER change 74 times (deck: core 0 voices 5-13, core 1 voices 8-12; crew deck: core 0 voices 0-13 from vb 23,354); EVOL core 1 0x1800 on the crew deck from vb 23,340. VMIXL / VMIXR never change (every voice dry). No IRQA write: the game uses no SPU2 interrupt.
- **So the output needs:** 48 voices with the hardware's Gaussian interpolation, Hall reverb on both cores with per-voice sends, core 1's AutoDMA input (the cutscene streams), core 0's output into core 1, and the volumes. Not needed: noise, pitch modulation, the IRQ, core 0's input.
- **Voices sounding** (per vblank, VOX voices included): deck vb 24,000-25,200 mean 19.3, max 24 (core 0 always 14, core 1 mean 5.3, max 10); deck vb 22,994-24,000 mean 13.8, max 21; crew deck vb 24,000-25,200 mean 2.5, max 6; the opening after the first stream (vb 15,540-20,145) mean 7.4, max 11; during the streams none (the stream alone).

## Audio output plan (audit H4, first cut, 2026-10-23)
- **Cost estimate [I, to measure]:** per sounding voice and 48 kHz sample (decode share, Gaussian, envelope, volume, dry and wet sums) about 40-60 cycles in C on the Allegrex: the deck's 24 voices x 48,000 x 50 = 58 M cycles per second, 17% of 333 MHz (about 7 ms per 40 ms frame). Reverb in PCSX2's form (14 work-area accesses, 12 multiplies, the down- and up-sampling filters) about 150 cycles per core per sample: 14 M cycles per second for two cores (4%). About a fifth of the CPU on the deck, where the logic is already the wall.
- **Options:**
  1. Mixer on the Media Engine (PS2RC's spu2_split: the state on the main CPU, the mix on the ME): about no main-CPU time. Needs a kernel module to start the ME: to be told to Ffm before it ships.
  2. Mixer on the main CPU: about 20% of the CPU on the deck [I].
  3. sceSas: 32 voices, 44.1 kHz only, PS-ADPCM (PSP VAG) data, its own reverb types (named like the SPU2's); loops follow the data's flags (no LSAX writes); where it runs on the PSP is not established. To measure.
- **Sync:** the IOP model runs on the game's virtual clock. When the PSP runs slower than real time it makes less than a second of sound per second, so the output needs a policy (time-stretch, e.g. PS2RC's audio_stretch, or slower playback). To design with the measurements.
- **Next:** an audio cost probe on the PSP that replays the deck's libsd call stream through options 1-3, after v24's runs are read.

## Mixing cost, first numbers (2026-10-23, audit H4)
- **Full mix keeps the game exact (x86):** `MGS2_SPU_MIX=1` makes spu2_lite render every sounding voice (PS-ADPCM decode, linear interpolation, envelope, volumes; not modelled: reverb, Gaussian filter, effect sends, the output) instead of the state-only engine. Deck route to vb 25,200: 252 of 252 hashes (every 100 vblanks) equal to `prof/hashes_x86_deck_v21.txt`, and equal to a run without it. So the v24 EBOOT can time the mix on the PSP with one cfg line.
- **Model (PspProf, the v24 EBOOT, deck vb 24001-24100, "busy only" as the m24d reference):** 4.92 -> 6.70 M instructions per step, 41.9 -> 47.5 ms: **+5.6 ms per step** (x_slow_span 3.95 ms, spu2_adpcm_block 1.26 ms, fast_span 0.38 ms; render_block's quiet path -0.19 ms). That is about 96 instructions per sounding voice and 48 kHz sample (19.3 voices x 960 samples a step). The model tracked the PSP within 3-7% for MGS2.
- **What it means [I, from the model]:** at 25 fps (2 steps per shown frame) the main-CPU mix alone takes about 11 ms of each 40 ms frame, before reverb (2 cores x 960 samples a step, est. 0.9 ms a step), the Gaussian filter and the effect sends. My first estimate (17%) was low.
- **Media Engine facts (PS2RC, `third_party/ps2rc/hle/psp/hle_me.h`):** started through ARK-4's KUBridge (kuKernelCall runs one function of the EBOOT in kernel mode); the stub writes the ME reset bit (0xBC10004C), the ME's memory range (0xBC100040) and its bus clock enables (0xBC100050, not the clock speed), then the ME runs our code from an uncached control block; sleep is held off with scePowerLock while the ME is ours (a suspend with the OS's ME driver waiting forever was seen to power the console off); proven on a PSP-3000 by PS2RC's author. Not emulated by PPSSPP. Writing those registers needs Ffm's word (rules: kernel module, power / clock registers).
- **Emulator check of the PSP folder ("MGS2T_v24s": v24's DATA.PSP byte-identical, title "MGS2 timing harness v24 sound mix", MEMSIZE=1; `prof/emu_v24s/`):** PPSSPP JIT, deck route to the end: 370 / 370 hashes, 63 / 63 with x86, vuh 311 / 311, no split; PPSSPP's clock 42.14 ms per deck step against 36.54 without the mix (IOP+sound 6.55 against 0.94: +5.61 ms, the same as the model).
- **Next:** time the mix on the PSP (v24 EBOOT + `MGS2_SPU_MIX=1`, after the v24 runs are read), then put the choice (ME with all effects at about no main-CPU cost, or the main CPU) to Ffm with the measured numbers.

## Mixing cost on the PSP (v24s, Ffm 2026-10-23 18:14, `prof/hw_v24s/`)
- **Run:** MGS2T_v24s (v24's EBOOT, cfg `MGS2_SPU_MIX=1`), deck route to the end. Every hash equal: 370 / 370 against hashref, 63 / 63 with x86 in ee / iop / vfb, vuh 311 / 311, no split. So the full voice mix keeps the game exact on the PSP too.
- **Deck, 1,200 steps:** 77.57 ms per step (v24 without the mix 70.04) = logic 33.45 (33.45) + draw 28.73 (28.74) + IOP+sound 10.37 (2.84) + GS 5.02 (5.02); p50 77.79, p99 94.54, max 150.78. Step by step against v24's steps.csv (the same game work, the hashes being equal): the mix adds **7.53 ms per step** (1,188 steps without pack reads: median 7.42, p10 7.24, p90 8.20, min 6.23, max 9.64); logic, draw and GS move by at most 0.004 ms.
- **Against the model:** PspProf said +5.6 ms (PPSSPP's clock +5.61): the PSP is 1.34x the model for this code [I: the mixing loops are multiply-heavy, and the model counts every instruction alike]. Per sounding voice and 48 kHz sample (19.3 voices x 960 samples a step): 0.41 us, about 135 cycles.
- **What it means:** at 25 fps (2 steps per 40 ms frame) the mix alone takes 15 ms of each frame on the main CPU, before reverb, the Gaussian filter and the effect sends (est. +1-3 ms a step [I]); the deck's logic alone is 33 ms a step against a 20 ms per-step total at 25 fps. The crew deck sounds 2.5 voices on average: about 1 ms a step there [I, scaled by voices].
- **Decision card to Ffm (2026-10-23 18:18):** Media Engine (recommended: about no main-CPU cost, all effects; one kernel-mode call through ARK-4's KUBridge writing the ME's reset, memory-range and bus-enable registers; sleep held off; tested on the PSP only) / main CPU / time sceSas first. No ME code goes to the PSP before the answer. **Ffm chose the Media Engine (2026-10-23 18:28, card option A).** Next: an ME sound probe; what it does is told to Ffm before it goes to the PSP.

## The voice mix split, PC reference (v26me, 2026-10-23)
- **What runs:** `boot/spu_split.c` and `boot/spu_split_b.h` on PS2RC's spu2_split (`third_party/ps2rc/audio`). Engine A is iop_host.h's state-only engine, as in every build. With `MGS2_SPU_ME=1` each register write and render block of A is also recorded as an 8-byte command for engine B, a copy of A run as the full engine (decode, interpolation, envelope, volumes). B's samples go into a CRC; nothing is played yet. An SPU RAM write gets a barrier before it (B has applied every earlier command) and a RAMW command after it. On the PC each command is applied at once; on the PSP B runs on the Media Engine (`psp/me_mix.c`, `psp/me_mix_me.c`). It is linked through `--wrap`, so the game's code and iop_host.h are unchanged.
- **Game unchanged (x86):** deck route to vb 25,200: 252 / 252 hashes equal to `prof/hashes_x86_deck_v21.txt`. Free route (v25f) to vb 36,500: 365 / 365 equal to `prof/hashes_x86_free_v25f.txt`.
- **B follows A:** every 10 s of sound (480,000 frames) a checkpoint compares A's voice-state hash with B's at the same render: deck 50 / 50 equal, free 73 / 73. `prof/mix_ck_x86_{deck,free}_v26me.txt` hold frames, commands, B's output CRC and B's state hash per checkpoint: the reference for the PSP's "mix: ck" lines. A field-by-field check of every voice after every render (`MGS2_MIX_CHECK=1`) finds no difference on either route.
- **The one field that differed is dead:** before the rule below, 3 of 50 deck and 6 of 73 free checkpoints differed, all in env_counter of stopped voices. When an end flag stops a voice in the middle of a block, A ticks the envelope over the whole block before it moves the position, while B stops at the sample, so A's counter runs a few ticks ahead (voice 1 at frame 1,017,960: A 7,684, B 7,664). Nothing reads the counter while the voice is stopped: key-on sets it to 0, key-off skips stopped voices, and no tick reads it. So the state hash (`split_vhash`) leaves env_counter out while a voice is stopped.

## The voice mix split on the PSP (v26me, v26me2; 2026-10-24)
- **v26me on Ffm's PSP-3000 (2026-10-24 12:10, `prof/hw_v26me/`):** the Media Engine never reached the loop: "mix: off (Media Engine not started: heartbeat DEAD (boots 0->0, state 0, heartbeat 0->0, reset reg was 00000040); power callback 0)". The kernel call ran (k_start read the reset register: 0x40 = only the USB block held in reset, the ME's bit 2 clear, so the system's own ME program was running). The game ran as v26: 431 / 431 hash lines identical to v26's, free play 88.76 ms/step (v26 88.72).
- **Most likely cause [I]:** PS2RC's reset stub reads its boot block from main RAM (uncached) before it writes the ME's memory range register 0xBC100040 = 2; in the port the boot block lies at 0x0B14D940, above the 32 MB line. Sony's ME boot code (uOFW `src/kd/me_wrapper/init.S`: it writes the main CPU's `0xBC100040 & 3` into the ME's 0xBC100040, then 0xBC100050 = 7 and the NMI flags, then the caches) and mcidclan's samples ("allow 64MB ram") set the range before any access to main RAM. No hash moved, so a wrapped access into the lower 32 MB (EE RAM) did not happen: the stub most likely stalled or faulted at its first load.
- **v26me2 (psp/me/hle_me_mgs2.c):** the stub writes 0xBC100040 = 2, 0xBC100050 = 6 and the NMI flags first (PS2RC's values), then Status and its exception handler, then reads main RAM; progress words in the ME's boot memory (0xBFC00400: stage, the registers as found, the words read, the loop's mark, the exception record) are read back in kernel mode and logged ("me:" line), so a second failure shows where it stopped. psp_harness.md "Build v26me2".
- **v26me2 on Ffm's PSP (2026-10-24 13:30, `prof/hw_v26me2/`):** the Media Engine started and mixed exactly: 67 / 67 checkpoints equal to the PC's, all 401 hash lines as v26's. The main CPU pays +0.11 ms per free-play step for the split (v26 88.97 -> 89.08 on the same windows), against +7.53 ms for mixing on the main CPU (v24s). The ME's own time is not measured (its Count reads 0); from its idle polls about 7 ms per step [I]. **The PSP switched itself off** at vb 33,600-33,700 (38.6 minutes in; battery 71%, good per Ffm; a real power-off, it started fresh). Nothing in the log or on the PC explains it (psp_harness.md "Build v26me2", PSP run). v26me3 writes the ME's and the battery's state every 5 s.
