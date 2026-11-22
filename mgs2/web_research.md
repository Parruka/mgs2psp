# Web research: ways to make the MGS2 PSP port faster (2026-10-18)

Asked by Ffm: "keep learning stuff useful for this project, so we can optimize it, think out of the box". This file has only what is **not already** in `tt_port/PS2_to_PSP_optimisation_lessons.md`, `feasibility.md`, `sandstone_review.md` or `peacewalker_notes.md`. Findings only, no third-party code copied. "(inferred)" marks our own reasoning; everything else has a source. Nothing here was measured on Ffm's PSP yet.

Columns: **Exact** = game state and visuals unchanged. **Consent** = needs Ffm's word first (clock, visuals, gameplay, CFW).

## 1. Ranked shortlist

| # | Idea | Expected gain | Type |
|---|---|---|---|
| 1 | ~~Real overclock above 333 MHz~~ (**ruled out by Ffm 2026-10-18: no overclock on their PSP**) with the 2026 `psp-beyond-444mhz` plugin (works on ARK-4) | Clock +30–40% at 433–463 MHz; no published fps numbers | **Consent** (Ffm's PSP, stability per unit) |
| 2 | Cutscenes already run at half rate on PS2 (PCSX2 "unlock cutscene fps" patches hit our wait word 0x18DB74) | Cutscenes need 1 logic step per 25 fps frame, not 2 | Exact (a fact to use) |
| 3 | Interprocedural liveness → guest args/returns as C parameters, callee-saved guest regs never stored to ctx | Literature: ~50% fewer stores, 10–30% faster; XenonRecomp: "several ms", ~20 MB smaller | Exact |
| 4 | Block/edge counts from the x86 build written by the recompiler as `__builtin_expect`, hot/cold split and function order | Profile-guided layout: 1–20% in published tools | Exact |
| 5 | `musttail` (GCC 15) for guest tail jumps | Fewer frames and returns; unmeasured | Exact |
| 6 | Check hot layout for 2-way I-cache set conflicts (8 KB alias) | Part of the I-miss count; unmeasured | Exact |
| 7 | GE-side culling: BOUNDINGBOX + BJUMP before each object's sub-list | Moves frustum tests off the CPU; ~22 GE cycles per test vertex | Exact |
| 8 | Bone matrices in a shared CALLed block; ≤ 2 CALL levels | Fewer list words written by the CPU | Exact |
| 9 | Full-screen effect blits/copies in 32–64 px strips; big effect textures in VRAM | Full-width blit 7.5× slower than strips | Exact |
| 10 | Frame finish via GE finish callback + display thread, never waiting for vsync in the game thread | ScummVM PSP: ~4% | Exact |
| 11 | `-fipa-pta`, LTO, `-fipa-ra` with hot call chains in one unit | pspdev: 2–5% for `-fipa-pta`; kernel: −0.4% text for `-fipa-ra` | Exact |
| 12 | Generation-time outlining / function merging of repeated guest idioms | Outliners 4–6% size, merging up to 25% | Exact |

## 2. PSP hardware

**Overclock beyond 333 MHz: ruled out by Ffm (2026-10-18). Kept for reference only.**
- `psp-beyond-444mhz` by m-cid, MIT, archived 2026-09-28: plugin plus stress tester that writes `ms0:/overconfig.txt`, 5 MHz steps from 333, theoretical max 471 MHz. Works with **ARK-4** and PRO; tested on 1000/2000/3000/Go; not PSP Street. "Stability is not guaranteed." Sources: https://github.com/mcidclan/psp-beyond-444mhz, https://www.biteyourconsole.net/2026/04/17/psp-beyond-444mhz-il-plugin-sperimentale-che-spinge-la-cpu-della-psp-oltre-i-333-mhz-fino-a-471-mhz/
- The same method is built into ARK-5 (`cpuclock` in Settings.TXT, overclock tester): https://github.com/PSP-Arkfive/ARK-5
- Reports: 433–463 MHz worth trying on 3000/Go (https://www.techbang.com/posts/129014-psp-overclock-plugin); a PSP-3004 crashed at 443 but was stable at 423 (https://www.komstation.com/news/ark5-true-overclock-performance/); Peace Walker / GTA "a stable 30 fps" where often 20 (https://wololo.net/2026/04/17/true-overclock-is-now-possible-on-psp/). **No source gives benchmarks or the bus/RAM clock at these speeds.**
- Fit with our rules: it is the CFW-side plugin, not our code writing PLL registers, but it is still a clock change and a new plugin on Ffm's PSP → **Ffm's decision**. (inferred) TT's 365 MHz via the CFW call gave only −8.6%; since our wall is memory stalls, the gain depends on whether the bus/RAM clock rises too. Measure with the timing harness at 333 vs. one stable higher clock.
- Also on: Wikipedia confirms licensed games got 333 MHz only from firmware 3.50 (https://www.gamedeveloper.com/game-platforms/psp-firmware-update-unlocks-full-cpu). Syphon Filter: Logan's Shadow spent the extra clock on physics and AI (https://blog.playstation.com/2007/07/27/syphon-filter-logan%E2%80%99s-shadow-taps-into-all-333mhz-on-psp/).

**Caches.**
- I- and D-cache 16 KB each, **2-way set associative, 64-byte lines**, write-back D-cache, no L2, no coherency: https://en.wikipedia.org/wiki/PlayStation_Portable_hardware, http://uofw.github.io/upspd/docs/hardware/PSP_cache-howto.html, https://www.psdevwiki.com/psp/Cache
- (inferred) With 2 ways, three hot functions 8 KB apart (mod 8 KB same set) evict each other. The TT layout model was fully associative, so it could not see this. Add a 2-way, 64 B line model to the layout tool and place the per-frame hot set to avoid set conflicts. Exact.
- Range flushes for small ranges; above ~16 KB a full writeback is cheaper (cache HOWTO above). Uncached alias = address | 0x40000000; mixing cached and uncached access to the same data is a known bug source.
- VFPU `sv.q` has a write-through/write-back bit (https://hitmen.c02.at/files/yapspd/psp_doc/chap4.html). (inferred) Write-through stores for data the GE reads (bone blocks, dynamic vertices) avoid a later flush. Exact.
- No user-usable cache lock or prefetch op is documented. No source for mult/div/load-use latencies; https://github.com/pspdev/vfpu-docs has latency test generators we could run on hardware.
- Bus is capped at half the CPU clock (166.5 MHz at 333): https://playstationdev.wiki/pspdevwiki/index.php/Tachyon

**Allegrex instructions.** ext, ins, wsbh/wsbw, seb/seh, rotr, bitrev, clz/clo, madd/msub, min/max are present; ll/sc, 64-bit ops and traps are missing: https://github.com/uofw/upspd/wiki/CPU. Binutils has upstream Allegrex support since 2023. No source says psp-gcc has an Allegrex scheduling model. (inferred) Grep our PSP objdump for min/max/ext/ins: if GCC does not emit them, the recompiler can emit `__builtin` or inline-asm forms for the R5900 MMI/clamp idioms (fewer instructions, exact).

**Scratchpad** (16 KB at 0x00010000) is off the system bus and slower than cache (Tachyon page, Wikipedia). No benchmarks or user-mode notes found. Not a priority (TT: D-misses ≈ 5 ms total).

**Media Engine.** Every success found is a loosely coupled job, mostly audio: DaedalusX64 (https://www.exophase.com/10367/daedalusx64-beta-2/), Star Fox 64 PSP "won't magically double performance" (https://www.generationamiga.com/2026/07/28/star-fox-64-psp-port-shows-impressive-progress/). New 2026 libraries `psp-media-engine-custom-core` / `psp-media-engine-safe-task` run ME code from user mode with the hardware spinlock (https://wololo.net/2026/06/16/unlocking-the-psps-dual-core-setup/), no numbers. In pspautotests, 8 SAS voices on the ME took 2.3 ms of each 11.6 ms and slowed RAM texture fetches 10–20% (`tests/gpu/timing/blittiming.c`). Matches TT: ME only for audio.

**LCD refresh.** Fixed 480×272 at 59.94 Hz (https://pspdev.github.io/pspsdk/pspdisplay_8h.html); the controller also has 119.88 and 29.97 Hz modes, no 50 Hz (SilverSpring blog, uofw upspd). No source runs the panel at 50 Hz. So 25 fps judders (2-3 vblanks) and the pacing choice in sandstone_review.md §2.3 stands.

**Extra 32 MB.** No source says the upper 32 MB is slower. Volatile memory is partition 5 at 0x08400000–0x08800000 (https://github.com/hrydgard/ppsspp/issues/16052).

## 3. Recompiler (code size is speed)

- **No public console recompiler passes guest arguments in host registers.** N64Recomp (ctx struct, `LOOKUP_FUNC` for `jalr`, `switch` for jump tables; https://github.com/N64Recomp/N64Recomp), PS2Recomp (https://github.com/ran-j/PS2Recomp), XenonRecomp, psxrecomp all use a ctx. So this lever is ours to build; the evidence for it:
  - Register liveness for binary translation: ~50% fewer stores, 10–30% faster (Probst et al., WCRE 2002, https://www.complang.tuwien.ac.at/andi/papers/wcre_02.html).
  - XenonRecomp keeps non-volatile, CR, CTR, XER registers in locals; its README calls it the largest win: ~20 MB smaller and "several ms" faster in Unleashed Recompiled (https://github.com/hedge-dev/XenonRecomp).
  - mctoll recovers parameters/returns and gets near-native speed after LLVM optimisation (https://dse.in.tum.de/wp-content/uploads/2022/01/translating_x86_binaries_into_llvm_intermediate_representation.pdf); rev.ng `detect-abi` does the same per function (https://docs.rev.ng/references/analyses/).
  - (inferred) For MGS2: per function, live-in GPRs (read before written) become C parameters, one live-out becomes the return, ctx is written only for registers an indirect caller or the uninitialised-stack readers can see. Classify by R5900 ABI role (s0–s7, fp, gp, sp callee-saved) as XenonRecomp does, but verify the ABI holds per function: MGS2 already showed a callee-saved float register whose stack copy must be exact.
- **x86 profile → PSP layout (out of the box, inferred).** Our generated C has the same control flow on x86 and PSP, so block and edge counts from the deterministic x86 run (vb 0–25200) are exact for the PSP too. GCC `.gcda` files will not transfer from x86 gcc to psp-gcc (profiles merge only "as long as the source file and the compiler options are unchanged", https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html). So let the recompiler emit the profile itself: `__builtin_expect` on guest branches, cold paths into `.text.unlikely`, function order by first use. Published gains for profile layout: AutoFDO ~10.5% (https://research.google/pubs/pub45290/), Propeller 1.1–8% beyond PGO (https://research.google/pubs/propeller-a-profile-guided-relinking-optimizer-for-warehouse-scale-applications/), BOLT up to 20% (https://arxiv.org/abs/1807.06735). Daedalus PSP did the same separation with a second code buffer so cold code does not pollute the I-cache (https://www.gamebrew.org/wiki/Daedalus_PSP, R9). Exact.
- **`musttail`** statement attribute in GCC 15 (https://gcc.gnu.org/gcc-15/changes.html): guest `j` to another function becomes a guaranteed sibling call. Watch GCC bug 124532 (two returns in one scope). Exact.
- **Size passes with numbers:** LLVM machine outliner 4.4% geomean size (https://lists.llvm.org/pipermail/llvm-dev/2018-April/122750.html); SEGGER linker outlining ~4.5% on RV32 (https://blog.segger.com/?p=3080); function merging by sequence alignment 6% average, up to 25% (https://eprints.whiterose.ac.uk/152091). No GCC MIPS machine outliner found, so do it at generation time: shared helpers for repeated guest sequences (call protocol, memory-access patterns), and merge identical functions across stages (already planned as dedup). Exact.
- **Compiler flags:** `-fipa-pta` 2–5% and LTO suggested by pspdev maintainers (https://github.com/pspdev/pspdev/discussions/57); `-fipa-ra` lets callers save only what the callee clobbers (−0.4% kernel text, https://lkml.iu.edu/hypermail/linux/kernel/1512.0/00018.html). Cheap to try, one at a time, on the harness. Exact.
- Superblocks: big ILP gains on wide machines but 23–355% code growth (https://www.cs.princeton.edu/courses/archive/spring18/cos320/lectures/hwu_jsuper93.pdf). (inferred) Wrong fit for a 16 KB I-cache; skip.

## 4. MGS2-specific

- **Checked against Ffm's ELF.** PCSX2's bundled patches (https://github.com/PCSX2/pcsx2_patches) give SLES-50383 widescreen at 0x11F230. In `disc/SLES_503.83` that word is `0x3C013F80` (`lui at,0x3F80` = 1.0f), exactly what the patch replaces: the **projection aspect setup** is at 0x11F230.
- **Cutscene half rate.** The "unlocked 50 fps in cutscenes" patch for SLES-50385 (another PAL Sons of Liberty) zeroes 0x18DB74; PAL Substance 0x191A34; NTSC-U Substance "60 fps for cutscenes" 0x1914F4. In our ELF, 0x18DB74 is the word elf_notes.md already found: if non-zero, the end-of-frame wait runs until the vblank counter reaches it, written only by stage code. So **in-engine cutscenes run at 25 logic steps/s on the PAL PS2**. (inferred) The PSP pays one logic step per 25 fps frame there, not two; the timing harness should report cutscene and gameplay steps separately.
- **Frame rate on other hardware.** NTSC PS2 gameplay 60 fps (https://www.engadget.com/2011-07-29-metal-gear-solid-hd-collection-will-run-at-60fps.html). PS Vita HD: 60 fps dropping to 30 with enemies on screen (https://www.thesixthaxis.com/?p=144454). Switch Master Collection: 30 fps with 16/50 ms alternating frame times (https://www.nintendolife.com/news/2023/11/video-digital-foundrys-technical-analysis-of-metal-gear-solid-master-collection-vol-1).
- **Effect timing trap.** MGSHDFix restored "tons of effects… running at double speed" in the PC and HD ports to PS2 timings, and says PCSX2 also gets some effect durations wrong (https://newreleases.io/project/github/ShizCalev/MGSHDFix/release/4.0.0). (inferred) If the renderer draws once per two logic steps, effect timers must stay tied to the logic step, and only real PS2 footage is the reference for effects.
- **Engine features to expect** (same MGSHDFix notes): motion trails, "concentration blur", god-rays/lens flares tied to the cutscene camera, a model-lighting bound calculation, and an **NPC LOD system**. (inferred) The game's own LOD may give a faithful lever: whatever LOD distances the PS2 already uses are exact by definition.
- **PCSX2 GameDB for SLES-50383** (https://raw.githubusercontent.com/PCSX2/pcsx2/master/bin/resources/GameIndex.yaml): motion blur reads the render target it draws into (`autoFlush`), skin colour needs accurate blending, frames are presented by a blit. Renderer must copy the target before sampling it (already a rule in sandstone_review.md §2.6).
- **CPU budget hint:** the team aimed to spend 30% of CPU time on AI; lead programmer Kazunobu Uehara (https://www.gamespot.com/articles/the-final-hours-of-metal-gear-solid-2-sons-of-liberty/1100-6376810/).
- **GCL:** LibGCL carried from MGS1 to MGS2 and on to Portable Ops/Acid/Peace Walker (https://github.com/metalgeardev/KCEJ_Wiki/blob/master/common/gcl/readme.md). Jayveer/Gcx says GCX commands resolve to C functions found in the ELF (https://github.com/Jayveer/Gcx): a way to name script command handlers in our code.
- **No MGS2 symbol map found.** TCRF lists only the Trial Edition (https://tcrf.net/Proto:Metal_Gear_Solid_2:_Sons_of_Liberty); its ELF has unexplained extra data at 0x54–0x1000 (https://hiddenpalace.org/Metal_Gear_Solid_2:_Sons_of_Liberty_-_Trial_Edition_(Feb_7,_2001_prototype)).
- **GirianSeed/mgs2old checked (2026-10-18):** a splat-based matching decompilation built from the binary `SLUS_205.54` (Substance NTSC-U, sha1 0dce210e...), not Konami's source. Early: 13 of 684 source files hold code, 245 named functions + 677 library symbols (`config/symbol_addrs.txt`, `lib_symbol_addrs.txt`). Useful as names only: libraries libgv, libdg, libhzx, libgcl, libmt, libfs, libmc, GM; boot order `Main` → ResetIOP, LoadModules, DG_InitGraph, GV/FS/DG/HZX/GCL/MT/GM_StartDaemon; source tree by developer (kira, okajima, shibata, kano...). Addresses are for SLUS_205.54, so they must be re-found in SLES_503.83 by signature. No licence stated: read for facts, don't copy code. https://github.com/GirianSeed/mgs2old

## 5. GE

Most numbers below come from comments in PPSSPP's source describing tests on real PSPs (`GPU/GPUCommon.cpp`, `Core/HLE/sceDisplay.cpp`, https://github.com/hrydgard/ppsspp) and pspautotests `tests/gpu/timing/blittiming.c`.
- **GPU culling:** BOUNDINGBOX (n vertices tested against the clip volume) then BJUMP skips the draw if not visible; ~22 GE cycles per test vertex. (inferred) An 8-corner box per object costs ~176 GE cycles and no CPU. Exact.
- **CALL nesting is 2 levels on hardware**; a third nested CALL is skipped (PPSSPP models it for Soul Calibur). Design: main list → object list → shared block, never deeper.
- Games CALL a small block of 12 BONEMATRIXDATA words + RET; PPSSPP even has a fast path for it. (inferred) Write each bone matrix once per frame, CALL it from every draw that uses it.
- **Stall-address streaming:** enqueue the frame list early and raise the stall pointer as draws are added, so the GE starts before the CPU finishes the frame. (inferred) Overlap within the frame, on top of TT's frame-N/N+1 overlap.
- **Textures:** 1:1 16-bit fill at 222 MHz: VRAM 8.3 ns/px vs RAM 34.9 ns/px (8888: 13.0 vs 66.3). The texture cache thrashes above ~160 texels wide: a full-width sprite costs **7.5×** the same area in 32 px strips (Tekken 6 scissors to 64 px). GE time scales to 2/3 at 333 MHz. (inferred) TT's "RAM textures cost little" was for small character/level textures; full-screen effect sources (blur, goggles, codec warp) belong in VRAM and must be drawn in strips.
- Clears at 222 MHz: 0.49 ms (16-bit), 0.69 ms (8888), 1.02 ms (8888 + depth); stencil clear is free.
- **Morphing** (GE vertex blend up to 8 weights) and Bézier/spline patches exist; no cost data. (inferred) Use morphing only where MGS2 blends key shapes linearly (faces), and only if it matches.
- **Finish callback instead of vsync wait:** ScummVM PSP ~4% (https://lists.scummvm.org/pipermail/scummvm-git-logs/2010-May/051540.html).
- Vblank lasts ~730–770 µs at 59.94 Hz (sceDisplay.cpp comments). `PSP_DISPLAY_SETBUF_NEXTFRAME` flips on the next vblank.

## 6. Out-of-the-box ideas, ranked by what they could change

1. ~~Overclock plugin~~ (ruled out by Ffm): would have been the only lever that speeds up everything at once, including the memory-bound part if the bus rises. Consent; one harness run at 333 and one at a stable higher clock answers it.
2. **Logic twice, render once** stays the plan, with a new fact: cutscenes are already 25 Hz logic (§4), so the double step is only for gameplay.
3. **The game's own LOD and visibility** (§4): faithful levers that already exist in the code; find them before inventing new ones.
4. **Profile from x86, apply on PSP** (§3): free, exact, deterministic.
5. **GPU-side culling and bone blocks** (§5): turn CPU work in the renderer into GE work.
6. **Interpolated 60 Hz display (consent, visual):** logic stays at exact 50 Hz; the renderer draws frames between logic states. Only worth it if the GE has headroom, which TT says it does (GE 24 ms vs CPU 40 ms). No source tried this on PSP (inferred idea).

## 7. Searched, not found
Measured CLUT load, TEXFLUSH, vertex cache or index cost; sceGu vs raw list numbers; Allegrex instruction latencies; any user-mode scratchpad benchmark; any 50 Hz LCD mode; any MGS2 debug symbols; PAL MGS2 frame rate from a primary source; technical postmortems for GoW CoO, Daxter, Killzone Liberation, Crisis Core, Peace Walker on CPU use.

## 8. Round 2 (2026-10-18, after Ffm's decisions)

**Ffm's decisions:** no overclock above 333 MHz, ever; stock 333 MHz through `scePowerSetClockFrequency` is approved. Binary-derived decompilations are fine.

**Clocks.** The bus is tied to the PLL at half the CPU clock: 333/166 and 222/111 (`psppower.h` "busfreq*2 <= pllfreq", https://github.com/pspdev/pspsdk/blob/master/src/power/psppower.h; PPSSPP `Core/HLE/scePower.cpp`; ARK-5 `SystemControl/src/syspatch.c` `sctrlHENSetSpeed(333,166)`). So 333 also raises memory speed by 1.5× over 222, which matters for our miss-bound code (inferred).

**Measured Allegrex timings** (pspdev/vfpu-docs, `docs/reference-outputs/inst_cycles.txt` on a PSP-3000 and `mem_cycles.txt` on a PSP-1000, both at 333/166; spot-checked in the clone):
- `mult` throughput 1, `mult`→`mflo` ~7; `div` ~20 throughput, ~21 latency.
- FPU `add.s`/`mul.s` throughput 1, latency 5; `div.s` 30/31; `sqrt.s` 29/30.
- VFPU `vadd.q`/`vmul.q` 1/5, `vdot.q` ~1.2/7, `vdiv.s` 14/17, `vmmul.q` 16/22; results retire in order.
- Taken `j`/`jr` ~3 cycles, taken `beq` ~4.
- Memory: L1 hit ~3 cycles per dependent load; random access above 16 KB jumps to 39 cycles (32 KB) and ~72 cycles (≥1 MB); sequential ~6.4 cycles per word once past the cache (≈100 cycles per 64-byte line, inferred), `lw` streaming 0.88 B/cycle, `lv.q` streaming 2.0 B/cycle. Matches TT's ~125-cycle I-miss estimate in size.
- Not found anywhere: uncached access cost, branch-likely penalty, a measured I-miss penalty.

**What psp-gcc 15.2 does** (pspdev/gcc branch `allegrex-v15.2.0`, `gcc/config/mips/allegrex.md`, `config.gcc`):
- Default `-march=allegrex`, MIPS II, EABI, single float. It emits min/max, ext/ins, seb/seh, clz/clo, rotr, wsbh/wsbw, movz/movn and branch-likely by itself. bitrev, ctz, float ceil/floor/round, cache and sync only via `__builtin_allegrex_*`. 32-bit `x += a*b` is `mult/mflo/addu` (madd only for the 64-bit widening form); no 3-operand `mul`.
- **No Allegrex pipeline model.** It falls back to `generic.md`: `mult` 17 cycles not pipelined (real: 1 per cycle, ~5–7 latency), `div` 38 (real ~21), no 5-cycle FPU latency. (inferred) The scheduler orders multiply- and float-heavy code badly. Cheap exact experiment: build the generated code with a few `-mtune=` values whose models are closer (or `-fno-schedule-insns`), keep the hashes identical, time each on the harness. A proper fix is a small Allegrex DFA file in our own gcc build.

**MGS2 engine map** (MGS1 decompilation https://github.com/FoxdieTeam/mgs_reversing + mgs2old): one `GV_ExecActorSystem()` call per frame, nine actor lists in fixed order, DG first/last actors wrap the frame, draws submitted one frame late. 112 mgs2old names were moved onto SLES_503.83 by matching code-offset runs: `decomp_names_sles.md` (candidates; `DG_ActFirst` = 0x11d9d0 confirmed against elf_notes.md). Inferred renderer cut: `DG_ActLast` (0x11da90 candidate) and the draw pipeline it runs; actors and DG object queues stay game logic.
