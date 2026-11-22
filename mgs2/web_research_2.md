# Web research, pass 2: new PSP knowledge for the MGS2 port (2026-10-29)

Asked by Ffm (2026-10-29 01:36): "do a search in the web for repos, skill, etc, stuff that might be useful for this psp port, read about psp game development, new optimization technology, tricks. Everything psp related."

Read first, so this file lists only what is **new**: web_research.md, research_review.md, feasibility.md, peacewalker_notes.md, ark4_options.md, me_mist_study.md, plus spot checks in speed_notes.md, renderer_notes.md, codec_speed.md and the PSP code. Research only: no port code, build or run was changed (the "MGS2 port" thread is paused until Friday). No game-derived data left the container. Tags: **[S]** the source says it, **[I]** my inference or estimate, not measured. github.com and its API are blocked here; repos were read through raw.githubusercontent.com (branch main/master as served on 2026-10-29, no commit ids). ppsspp.org pages were read through the web fetcher; its download hosts are blocked from the container.

## Ranked shortlist

| # | Find | What it could buy us | Risk / cost |
|---|---|---|---|
| 1 | PPSSPP dev builds (1.21-dev): VFPU special functions **and dot products** bit-exact on every CPU backend; GE software renderer bit-exact on 125 of 132 game dumps | Ffm's PC PPSSPP might match the PSP's hashes (today they split at vb 500; one run tells); pixel checks of the GE renderer on the PC | Dev build, not a release (1.21 "planned for Christmas"); never a timing reference; needs a download on Ffm's PC |
| 2 | Measured GE timings on firmware 6.60 (psprecomp, MIT): fill 209.2 px/us, **0.85 us per sprite command**, list start 51 us, GE runs up to 43 draws behind the command parser | A real GE cost model for the codec, text and effect sprites, and for kick batching | Clock and pixel format of the test not stated; Ffm runs 6.61 |
| 3 | The GE's arithmetic in full (PPSSPP "GE bit-exact" article): truncation everywhere, no guard bits, **projection x view x world multiplied inside the GE**, integer de Casteljau for patches, "do not merge primitives" | Exact-to-PS2 transforms: multiply the matrices on the CPU in float32 and give the GE identities; rules for the Bézier sea and for merged draws | Small CPU cost per mesh [I] |
| 4 | PSP thread scheduler measured: FIFO per priority, **no timeslice**, `sceKernelDelayThread(0)` does not yield, file calls give up the CPU; a reader thread took disc reads off a recompiled game's frame (3.16 -> 0.04 ms a frame) | Design rules for the planned stream / reader thread (research_review M17) | None to read; the thread itself is a code change for later |
| 5 | Free Riders Recompiled: guest registers as C locals with a **whole-program liveness that checks the calling convention** instead of trusting it: -13 to -18 % frame time, -16 % code | A way past the limits speed_notes §591 lists for regcache's IN/OUT sets | Host is ARM64/x86 with many registers; our vu_local.py lesson (spills) applies [I] |
| 6 | psp-developer skill, the 14 references not read before (media-engine, audio, movies, file I/O, streaming, profiling, debug...) | Several hardware facts below, incl. one that bears on MIST idea B (own kernel PRX froze a PSP) | Read-only; can be installed as a skill |
| 7 | Star Fox 64 PSP (TheMrIron2): 30 Hz logic shown at **60 fps by interpolation on a real PSP**; ME audio, VME mixing experiments | Evidence for the interpolated-display pacing option (a visual choice, Ffm's) | Costs a render per extra frame; our renderer is CPU-heavy [I] |
| 8 | More GE cost numbers (psp-developer): ~0.455 us per triangle, ~1.4 ms GE for ~10 k triangles, GE skinning 7.2x faster for one character but slower for crowds, async flip with a presenter thread **no gain** | Budgets for the renderer; corrects web_research #10 | Numbers from other engines |
| 9 | ME details: psp-me-bench (ME as fast as the main CPU), psp-media-engine-reload (VME/eDRAM registers), the full sleep fix | Background for movies and sleep later | New register writes would need Ffm's OK |
| 10 | pspdev releases 2026: `MEMSIZE=2` default, weak stubs, sceSasCore library, gprof fixes | Nothing faster on ARK-4; two things to know before a toolchain update | Weak stubs change how a missing import fails |
| 11 | Skills and repos checked and not recommended | - | - |

## 1. PPSSPP: bit-exact VFPU and GE in the development builds

**Sources.** https://www.ppsspp.org/news/vfpu-math-re (2026-09-24, PR #22343), https://www.ppsspp.org/news/ge-bit-exact (2026-10-02, PR #22414), https://ppsspp.org/docs/development/known-differences, https://www.ppsspp.org/news/ (latest release 1.20.4 of 2026-05-16; 1.21 "planned for Christmas").

**Facts [S].**
- "From version 1.21 and current development builds, the VFPU's special functions **and dot products** are bit-exact on every CPU backend." `vrcp vrsq vsqrt vexp2 vsin vcos vasin vlog2` are exact over all 2^32 inputs; it is on by default. 1.20 and earlier "still have some inaccurate functions".
- GE: 125 of 132 game frame dumps bit-exact against a real PSP, `frametests.py` 29 of 30. Not exact yet: swizzled depth mirrors, lazy 8-row self-texture loads. No timing model.
- Tools: `geprobe` (display lists from a Python description, run on a PSP over PSPLink, frame and depth read back); pspautotests `gpu/exact`; `ppdmp-playback` replays a PPSSPP frame dump on a real PSP. Warning from the authors: oversized readbacks or overlapping jobs hung their PSP until a manual reset.
- From 1.21, with a firmware installed, sceMpeg, sceMp4 and sceFont run as real firmware code (not HLE).

**What it buys us [I].**
- Our notes: Ffm's own PC PPSSPP rounds the VFPU toward zero, so the harness's hashes split from vb 500 there, and the container needs the `PPSSPP_VFPU_RN=1` patch. The 1.21 notes name special functions and dot products; they do not say whether plain `vadd`/`vmul` rounding changed on the PC backends, and our checked forms are mostly that kind (speed_notes §22). So a current dev build on Ffm's PC may or may not give the PSP's hashes with no patch; one emulator run of the current EBOOT answers it. **Answered 2026-10-29 [C]: it does not; master still rounds plain vadd/vsub/vmul toward zero, so the patch is still needed (ported: see "Installed and checked" at the end).**
- research_review M2 (bit-exact GE reference) no longer needs a PPSSPP build from source: the dev build has it. The renderer's open picture questions (#83 teal codec background, #84 stale text page and the faint line at row 220, the "5-15 levels darker" 3D area) can be checked against the GE's real output on the PC. The game's own reference stays the PS2 picture.
- With a firmware dump installed, PPSSPP would run sceMpeg as real code: a closer check for the PSMF movie work later.

**Risk.** A dev build can have its own bugs; keep 1.20.4 beside it. Never use it for time (the device gives numbers). The download is a PPSSPP binary, not game data, but it is a new program on Ffm's PC, so it is Ffm's call. The container cannot reach the PPSSPP download hosts or github.com, so the container's patched 1.20.4 stays as it is.

## 2. Measured GE timings on a real PSP (psprecomp)

**Source.** https://github.com/Sifayne/psprecomp (MIT, (c) 2026 sp00nznet; fork by Sifayne), `docs/RENDERER.md` section "When the GE runs" and `tools/hwprobe/README.md`; PR #3 "Match firmware 6.60 as measured on a real PSP" (merged 2026-10-01). psprecomp is a PSP-to-PC static recompiler (Armored Core: Last Raven Portable runs on it); its value for us is its hardware probes.

**Facts [S]** (geprobe 5-6, firmware 6.60, 100 sprites between two SIGNALs then a FINISH):

| Sprites | 2nd SIGNAL | FINISH | DrawSync returns |
|---|---|---|---|
| 480x272 | 35,609 us | 62,409 us | 62,474 us |
| 64x64 | 1,130 us | 1,952 us | 1,993 us |
| 16x16 | 126 us | 138 us | 176 us |

- Drawing: 623.6 us per full-screen sprite, 19.06 us per 64x64 sprite: **209.2 pixels per us**.
- Command processing: **0.85 us per sprite** (a VADDR and a PRIM); it limits 16x16 sprites. Their model: 0.1 us per command word, 0.6 per PRIM/BEZIER/SPLINE, 0.025 per vertex (the split is fitted, not measured).
- The command parser runs up to 43 drawing commands ahead of the drawing; a SIGNAL handler runs when the parser reaches it, not when the drawing does.
- `sceGeListEnQueue` takes 51 us and the GE starts 40 us into it; words released by `sceGeListUpdateStallAddr` run inside that call when they can (within ~11 us).
- A list with a few commands has run, handlers included, before EnQueue returns.

**What it buys us [I].**
- The 0.85 us per sprite is a floor that does not shrink with the sprite. The codec screen, subtitles, the text page and the HUD draw many small sprites: 1,000 small sprites cost ~0.85 ms of GE time however small they are. Merging glyph sprites into one PRIM with many vertices (2 vertices per sprite, one PRIM) pays 0.025 us per vertex instead [I, from their model, not measured]. Worth one count of sprites per frame in the codec digests.
- The 51 us EnQueue cost backs the "kick every ~16 meshes" rule (research_review M9) with a number: a list start per mesh is expensive, a stall-address update is not.
- 209 px/us at 333 MHz (if it was 333) means a 480x272 full-screen pass costs ~0.62 ms; web_research.md's 8.3 ns/px at 222 MHz scaled to 333 gives ~180 px/us, so the two agree within ~15 % [I]. Good for budgeting the blur, goggles and codec warp passes.
- `tools/hwprobe/` (geprobe 58 scenes, threadprobe, syncprobe, sasprobe, vfpuprobe, modprobe) is MIT and builds from PSPSDK alone. A probe of our own (e.g. sprite cost by size and format at 333 MHz, CLUT load cost, which research_review §7 lists as never measured) could start from their logging pattern: the log is flushed before every step and a re-run skips the step that switched the PSP off.

**Risk.** The clock (222 or 333) and the framebuffer format of the timing scene are not stated; their PSP runs 6.60, Ffm's 6.61 [I: the GE does not depend on the firmware version, the list calls might]. Code is MIT: credit and keep the notice if any is copied.

## 3. The GE's arithmetic, as the PPSSPP team measured it

**Source.** https://www.ppsspp.org/news/ge-bit-exact; psprecomp `docs/RENDERER.md` "Depth values" (its own geprobes, 41,543 depth points fitted).

**Facts [S].**
- Floats in the display list lose their low 8 bits; the GE computes with a 16-bit significand, truncating toward zero; the adder has no guard bits.
- A matrix row is one sum: products exact, each truncated to the largest product's precision, summed, truncated once.
- **Projection, view and world are multiplied together inside the GE** in that arithmetic, then the vertex is transformed by the product (psprecomp: forming eye z first fits 939 of 3,072 points, the combined product fits all).
- No divider: perspective uses a 128-segment reciprocal table, normalisation a matching rsqrt table; triangle setup a 256-segment table.
- Depth, colour, fog and UV are fixed-point planes per triangle, anchored at a corner chosen by the long edge. Mip level is chosen per 4-pixel span.
- Specular uses a viewer direction from the view matrix's third column; specular power is a piecewise-linear approximation.
- Bézier and spline patches: de Casteljau with integer lerps (operands aligned to the larger exponent, cut to 16-bit integers, 8-bit parameter); t = 0 and t = 256 pass through uncut.
- Authors' advice for renderer writers: truncate, never round; one truncated sum per matrix row; tables, not division; **"do not merge primitives or sprites: each primitive has its own planes, and merging changes texel sampling"**.

**What it buys us [I].**
- **CPU-side matrix product.** The PS2's VU1 forms its vertex in float32. If our converters give the GE a world matrix and a projection matrix, the GE multiplies them in its 16-bit arithmetic first, which loses more than float32 does. Multiplying them on the CPU in float32 (the same order the VU program uses) and loading identity view and projection leaves the GE one truncation of the final matrix: products with 1.0 and 0 are exact, so an identity costs no precision. `ge/replay_fmt.h` carries world[12] and proj[16] separately today. Cost: one 4x4 product per matrix change (VFPU `vmmul.q` 16 cycles, or 64 FPU multiply-adds). Check on the PC with the bit-exact renderer (§1) against the PS2 frame.
- **Bézier sea** (renderer_notes §5, the 2 x 225 Hermite patches): the GE's integer de Casteljau is not the VU's float evaluation; patch vertices will differ in their low bits from the PS2's. Compare a tessellated triangle mesh made on the CPU against GE patches before choosing.
- **Merged draws** (applied in the GE replay): merging keeps the GE's cost down but changes sampling; that is fine where the merged picture is compared with the PS2 picture, not with an unmerged GE picture. Keep it in mind when a merged draw shows texel shifts.

**Risk.** None to read. Each use is a renderer change for later, checked by digests.

## 4. Threads and disc reads

**Sources.** psprecomp PR #3 and README (threadprobe v3, 164/181 steps matched; firmware 6.60); psp-developer `references/file-io-memory-stick.md` §6, `audio.md` §2, `streaming-architecture.md` (https://github.com/SandstoneDev/psp-developer).

**Facts [S].**
- One FIFO ready queue per priority, **no timeslice**: equal-priority threads never take turns on their own; `sceKernelDelayThread(0)` does not yield; `sceKernelWakeupThread` does not end a delay or semaphore wait; file calls give up the CPU.
- A reader thread just above the game thread (or `sceIoReadAsync` / `sceIoWaitAsync`) for a recompiled game's disc-image reads: mean frame 68.0 -> 64.8 ms, 99.9th percentile 183 -> 119 ms, read time on the game thread 3.16 -> 0.04 ms per frame [measured: PSP-3000].
- Audio streaming on the game's thread caused 14 spikes of 186-243 ms per session; a low-priority decoder thread plus a tiny high-priority feeder removed them (worst frame 243 -> 93 ms) [measured: PSP-3000].
- A "polite" lowest-priority loader starves under a strict-priority scheduler; raise it while the game waits for data.

**What it buys us [I].** `runtime/iop_host.h` `disc_pak_read` reads synchronously in the game thread (research_review M17), and the logs in `play/disc_reads_*.log` show VOX/DEMO streaming during play. A reader thread above the game thread, with the game thread blocking on its answer only when the PS2 would have, takes the card time off the step. The scheduler facts give the design: the reader must be at a higher priority (no timeslice would ever give it the CPU otherwise), and a busy-wait with `DelayThread(0)` would not let it run. It has to be created at boot (M15: thread stacks come from the partition). Exactness: the PS2 game sees the IOP's answers on the same vblank as today if the game thread waits for the data at the same point [I; to prove with the hashes].

**Risk.** A thread is a code change and touches the IOP timing model; one change per measurement.

## 5. Guest registers as locals, with the convention checked (Free Riders Recompiled)

**Source.** https://github.com/YuutaTsubasa/Free-Riders-Recompiled/pull/283 (Xbox 360 game, XenonRecomp-style; `scripts/register_locals.py`).

**Facts [S].** Generated code is rewritten so guest registers live in C++ locals instead of the context. Unlike XenonRecomp it "checks the calling convention instead of trusting it": whole-program liveness; callers store a register before a call only if the callee reads it on entry (4,095 exception funclets read r12); a volatile register read after a call goes through the context (200 functions); host code that reads its caller's registers is listed per call site (`SITE_READS`; a missing entry stopped the first build at boot); LR, MSR and the reservation always stay in the context; unknown control flow stops the build. Verified word for word over a scripted race (steps 760-1940) and pixel-identical screenshots. Measured: -18 % undrawn, -13 % drawn frame time on an ARM64 handheld; -16 % code on Windows.

**What it buys us [I].** speed_notes §591 names why regcache's IN/OUT sets do not cut more: IRQ_POLL counts as reading every register, indirect and host calls read and write everything, a register read and written on one line counts as read first, returns store every written register. Their per-site list for host readers is the same idea as giving each host call (rt_*, SYSCALL, the poll) its real read/write set instead of "everything", and their "volatile after call" rule is the R5900 ABI check that web_research §3 proposed. The lesson from our own vu_local.py (spills when 20-50 values are live) limits how many registers can be locals at once on the Allegrex [I]. A first step without code risk: count, on the x86 twin, how many context stores and loads per step each of the four causes in §591 accounts for.

**Risk.** Their host has 31 general registers; the Allegrex has far fewer free ones, so the gain will be smaller [I]. Exactness by construction with the hashes as the check.

## 6. psp-developer skill: the references not read before

**Source.** https://github.com/SandstoneDev/psp-developer (an Agent Skills folder: SKILL.md + 21 references; licence text is PS2RC License 1.2, source-available, free use and forks). research_review.md read 7 references; this pass read the other 14. It is the only PSP skill found. New facts for us, all [S] unless marked:
- **Own kernel PRX froze a PSP-3000:** "loading our own kernel PRX with `pspSdkLoadStartModule(path, PSP_MEMORY_PARTITION_KERNEL)` from a user EBOOT hard-froze the console; the kubridge call worked every time" (media-engine.md §2). This bears on me_mist_study.md idea B (a cheap ME wake through a small kernel module of our own): it is not a free swap for kuKernelCall. The KOMSTATION article (§9) adds that m-c/d called running kcall.prx "a bad idea" (second-hand).
- **ME sleep, full fix:** a kernel helper replaces the SceMeRpc sysevent handler for the session; on suspend (event 0x4005) the ME loop writes back its D-cache and halts, on resume (0x10005) the bus clock is re-enabled and the stub cold-started. Park acknowledged in 16 us, 0 mismatches over 3,000 blocks after wake. Only if Ffm ever wants sleep; today the power lock blocks it.
- **ME cold start:** D-cache contents are undefined after reset; clear the tags (TagLo = TagHi = 0, index store tag) instead of writing back, as Sony's ME boot code does (uofw `me_wrapper/init.S`).
- **ME CP0 Count** reads a constant until written (`mtc0 $zero, $9`), then runs at ~454 MHz and wraps every ~9.5 s. Relevant if our ME mix ever times itself.
- **ME exception = silent hang:** install a handler that stores Cause / EPC / BadVAddr and halts, plus a main-CPU watchdog with hand-back of the work.
- **gcc and ME code:** `-ftree-loop-distribute-patterns` (on at -O2) turns copy loops into memcpy calls; compile ME files with `#pragma GCC optimize("no-tree-loop-distribute-patterns", "no-jump-tables")` and check `psp-objdump -dr` for outside calls [check our ME objects once].
- **Movies vs our ME takeover:** taking the reset vector removes sceMpeg, sceAtrac and sceSas for the session; their port plays movies before the takeover. Same conclusion as me_mist_study.md A; adds the PSMF player rules (drain every registered stream, audio-master clock with a timer fallback, writeback-invalidate decoder output before each decode, RGBA8888 into RAM then GE; RGB565 straight into VRAM showed wrong colours on a PSP-3000).
- **sceAudio SRC channel:** a single channel; releasing and re-reserving it failed on hardware (fine in PPSSPP) and killed sound for the session. Reserve once and keep.
- **Profiling:** CPU load per frame from `sceKernelReferSystemStatus` idle clocks works in user mode; `sceKernelGetSystemTimeWide` called 2-4 times per instance added whole ms; a fixed 1-in-16 sample aliased with a call pattern and inflated costs 100-1000x (use a hashed 1-in-N).
- **USB copies drop silently**; an interrupted copy damaged the FAT so ARK read its flash settings and **silently turned the 64 MB mode off**. Our md5 deploy check covers the first half; the second shows as a 24 MB start.

**Install as a skill?** It is a ready Agent Skill. It could be cloned into `.claude/skills/psp-developer` on Ffm's PC for local sessions; for this project its facts are now in our notes, so it is optional.

## 7. Star Fox 64 on the PSP: 60 fps from 30 Hz logic

**Sources.** https://github.com/TheMrIron2/sf64-psp (README), https://www.generationamiga.com/2026/09/06/star-fox-64-psp-port-moves-closer-to-release-with-full-game-now-playable/, https://retrododo.com/star-fox-64-is-now-running-on-psp-at-60fps/ (2026-09-10).

**Facts [S].** Port of the SF64 decompilation by Daniel Welch (TheMrIron2). The game's logic runs at 30 Hz; "a very early implementation" shows 60 fps by interpolating in-between frames, on a real PSP. About 30 fps for most of the game with audio off; sound moving to the Media Engine, VME mixing experiments (with mcidclan), PSPGL first, a native sceGu renderer planned; a reusable library "n64psp". Targets explored: 60 fps at 333 MHz, 30 fps at 222 MHz.

**What it buys us [I].** The pacing choice (sandstone_review §2.3: 25 shown with judder, 30 shown with 50 Hz logic, or an interpolated 60 Hz display) now has a PSP example of interpolation working. For MGS2 an interpolated frame costs a full render (renderer CPU time is the larger part of a codec step today), so it only fits where the renderer has headroom. A visual change: Ffm's decision, and only after 25 fps holds.

**Risk.** Early work, no numbers; a decompilation (native code), not a recompile.

## 8. More GE cost numbers

**Source.** psp-developer `references/optimization-profiling.md`, `rendering.md` (measured on a PSP-3000 in a streaming map engine and in a recompiled PS2 game).
- ~0.455 us of GE time per triangle on props (three scene points on one line).
- ~1.4 ms of GE time per frame for ~10,000 triangles on a good path; CPU per mesh 55 us on the console vs 13.5 us in PPSSPP (the converter evicted the caches).
- GE skinning (`sceGuBoneMatrix`, up to 8 bones): one hero character 3,080 -> 428 us (7.2x); a crowd lost (112 draws 4.9 ms vs 2.0 ms for cached CPU-skinned vertices).
- GE lighting instead of a CPU relight: 1.3 ms plus 5 ms spikes -> 0.34 ms.
- Halving texture resolution: route GE time 12.7 -> 8.7 ms (a visual change).
- **An asynchronous flip with a presenter thread gave no gain** (+0.43 ms, 95 % interval -3.0..+2.6) in the recompiled game. web_research.md #10 cited ScummVM's ~4% for the same idea; for a CPU-bound recompile the measured answer is "nothing".
- `-Os` was 41 % slower despite 34 % less code; basic-block placement hints +0.9 % (worse); base-register masking -6 % (= research_review M19).

**What it buys us [I].** The codec plan (codec_speed.md) lists GE skinning for the P07 faces as "not exact"; the 7.2x figure for one character says what it would buy if Ffm ever wants it: the faces are few characters, the case where GE skinning won. Drop the presenter-thread idea from the list.

## 9. Media Engine sources new to our notes

- **psp-me-bench** (https://github.com/kan8223-dotcom/psp-me-bench, via https://www.komstation.com/news/psp-media-engine-benchmarked-2026/ and the author's note.com posts): work split between main CPU and ME gives x2.0-2.9 at the same clock (psp-developer cites the README); the ME's FPU results were bit-identical to the main CPU's on 1000 and 3000; it uses a kcall.prx plugin. Confirms ME and main CPU are the same core speed [S]. Our mix already costs the main CPU only its wake.
- **psp-media-engine-reload** (https://github.com/mcidclan/psp-media-engine-reload, README): ME memory map (4 MB ME eDRAM on 2000+, ping-pong buffers at 0x04000000 with bit 5 of 0xBC100050, VLD unit at 0x04100000, VME datapath at 0x040F8000), the CPU semaphore at 0xBC100048, and: writing `0xBCC00000 = -1, 0xBCC00030 = 1, 0xBCC00040 = 1` stops a background process the ME core runs over its eDRAM and "appears to also improve the performance of code running on the ME" [S]. Our start writes Sony's values (0xBCC00030 = 8, 0xBCC00040 = 2, psp/me/hle_me_mgs2.c). Our mix lives in main RAM, so this matters only if ME speed ever limits the mix; different values would need Ffm's OK.
- **Also listed there** (MIT samples): me-spinlock, me-soft-interrupt, me-dmacplus-transfer, me-sharing-drawing, me-avc-csc-hd (hardware colour conversion on the ME's AVC unit), me-feed-graphics-engine (already our origin), psp-media-engine-cracking-the-unknown (VME DSP).
- **psp-555mhz-plugin** (mcidclan): overclock work beyond psp-beyond-444mhz. Out by Ffm's no-overclock rule; listed only so it is known.

## 10. pspdev toolchain releases in 2026

**Sources.** https://github.com/pspdev/pspdev/releases; `pspsdk/src/base/build.mak` (master); ARK-4 `core/systemctrl/src/plugin.c` (main).
- v20260801 made `MEMSIZE=2` the default (`PSP_LARGE_MEMORY` unset -> 2; =1 still writes 1). ARK-4 treats any non-zero MEMSIZE the same (`if (use_highmem) force_high_memory = 2`), so **no extra memory for us**; our Makefile's `PSP_LARGE_MEMORY = 1` keeps working (it prints a warning on a new SDK) [S].
- v20260901 "made all stubs weak imports". [I] After a toolchain update, an import the firmware does not provide no longer fails the load; the call fails at run time instead (research_review M1 already notes "calling an unresolved import crashes"). Check the ME/kernel helper imports' return values if the tarball is ever updated.
- v20260501 added the sceSasCore library to pspsdk (relevant to the planned sceSas comparison); v20260201 GCC 15.2.0 (ours) and gprof fixes for PBP builds (a sampling profile without hardware counters; `-pg` changes the code layout, so it is not a timing tool for our I-cache-bound code [I]).
- Still no Allegrex pipeline model found in any release note.

## 11. Checked and not recommended

- **ps2-recomp-Agent-SKILL** (https://github.com/ajitmohapatr/ps2-recomp-Agent-SKILL, via skills.cat): a skill for the PS2Recomp PC pipeline; no PSP content, no runtime optimisation advice, no licence stated, and the skills.cat scan flags it high risk (a destructive shell command, "privacy exfiltration" warnings in two files). Do not install.
- **sal063/PSP-recompilation-project**, **last-raven-recomp**, **Phantasy Star Portable 2 recomp**: PSP-to-PC recompilers; nothing for a PS2-to-PSP port beyond §2.
- **ReXGlue / pinyon-shift optimisation survey** (issue #456): a list of questions without numbers.
- **nakagawa-recomp #284** (register promotion): not started.
- **PS2RC issue #2** (GTA SA on a PSP Go at 333 MHz): user feedback only, no numbers.
- ScummVM PSP memcpy commits (2010): no numbers.
- Allegrex cache-op list (fill / lock / prefetch): still no public source found; PSPTEK (daifukkat.su/docs/psptek, cited by psp-developer for the profiler block) blocks automated readers.
- 50 Hz LCD mode: still none found.

## Suggested next steps (for the "MGS2 port" thread, after Friday)

1. With Ffm's OK, a PPSSPP dev build on Ffm's PC and one emulator run of the current EBOOT: do the hashes now match the PSP past vb 500? (§1)
2. Count sprites per frame on the codec, text page and HUD; if they are in the thousands, plan merged sprite PRIMs. (§2)
3. Put "multiply world x view x projection on the CPU in float32, identities on the GE" into the renderer plan, to check against the PS2 frame with the bit-exact renderer. (§3)
4. Design the reader thread with the scheduler facts when M17 comes up. (§4)
5. Count context traffic by cause on the x86 twin before any register-locals work. (§5)

Sources used are linked in each section.

## Installed and checked (2026-10-29 02:00-02:40, Ffm: "install what is needed from the search, lets skip that ps2 recompiler")

- **psp-developer skill**: git clone works through the container's proxy (github.com pages and API are blocked, `git clone` of public repos is not). Commit 49326adf (2026-10-20), copied unchanged to `mgs2/third_party/psp-developer` (SOURCE.md, LICENSE = PS2RC License 1.2) and installed as a skill in this session (`~/.claude/skills/psp-developer`; ephemeral, so a new container needs `cp -r /mnt/project-files/mgs2/third_party/psp-developer ~/.claude/skills/`). Its `scripts/doctor.py` was read: read-only checks (runs `wsl.exe -l`, `psp-gcc -dumpversion`, `psp-config`, `flatpak info`), no downloads.
- **psprecomp hardware probes**: `tools/hwprobe` of Sifayne/psprecomp commit 167d3297 (2026-10-16), MIT, in `mgs2/third_party/psprecomp_hwprobe` (LICENSE kept; .bin/.raw captures left out; the .inc files are synthetic GE test scenes generated by their scripts, no game data).
- **PPSSPP master** (83e616ea, 2026-10-28) built headless in the container: `cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DHEADLESS=ON -DHEADLESS_CROSS=ON -DUSING_QT_UI=OFF -DLIBRETRO=OFF -DUNITTEST=OFF -DUSE_DISCORD=OFF` (needs `apt-get install libgl1-mesa-dev`; without HEADLESS_CROSS it wants SDL3), `ninja PPSSPPHeadless`. Run with `--memstick=<dir that holds PSP/GAME>` (master no longer reads `$HOME/.ppsspp`). New in master: `--me-core` ("Emulate the Media Engine for homebrew that runs code on it", Core/MIPS/MeCore.cpp), not tried yet; it could let the ME builds run in the emulator.
- **gl5 on unpatched master [C]**: the probes show plain `vadd.s / vsub.s / vmul.s` equal to the FPU's round-toward-zero 4096/4096 (the PSP: round-to-nearest 4096/4096), so the game splits at vb 500 exactly as on Ffm's PC. `vdot.q` now matches the PSP's log exactly (same three values, 632 / 631 / 2486). So §1's hope is answered: a dev build alone does not fix Ffm's PC.
- **Our rounding fix ported to master**: `mgs2/tools/ppsspp_master_vfpu_rn.diff` (InterpreterVFPU.cpp Int_VecDo3 rounds to nearest with FTZ + DAZ under `PPSSPP_VFPU_RN=1`; IR Comp_VecDo3 sends vadd / vsub / vdiv / vmul to it). With it [C]: the vadd / vsub / vmul / vdot probes equal the PSP's (prof/hw_gl4c/run3/log.txt); the two "vfpu special" NaN lines still differ (PSP 7f800001, master keeps the input NaN; harmless, NaN lanes take the exact C fallback).
- **gl5 to vb 40,000 on patched master [C]** (delivered files, only `MGS2_EXC=0` in the copy): all 438 ee hashes equal hashref.txt; the 205 codec digests all e5535d8d57747651 (= the PC's); the only difference is the VFPU fallback count (vb 15,794: 325,091 vs the reference's 325,041; vb 16,000: 329,622 vs 328,542, the same 329,622 the old patched 1.20.4 gave in psp_harness.md v15; vb 40,000: 823,319 vs 773,303), so the emulator's fallback count differs as before while the game state does not. Log and steps: `mgs2/prof/emu_gl5_master/` (split-finder register dumps removed; RAM dumps deleted).
- **Not done**: the PspProf profiler and the TT patches are still only on 1.20.4 (tools/ppsspp_mgs2.diff); porting them to master is a separate job. No Windows build for Ffm's PC (it would need the same fix; Ffm's PC VM has no internet either).
