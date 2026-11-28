# PS2 → PSP optimisation: lessons learned (2026-10-16 → 2026-10-18)

Source: a working port of a PS2 action game (static recompilation of the EE code into C, original game logic running headless, renderer replaced at the display-list boundary with a sceGu/GE renderer) to a **PSP-3000 on ARK-4, 333 MHz**.
Every number below was **measured on real hardware** unless it says "emulator" or "estimate".

Part A has the rules that carry over to any PS2 → PSP port. Part B has the evidence and history behind them. Part C has the method. Part D has the open levers.

---

## Part A — Rules that transfer to any PS2 → PSP port

### A1. Find out what the wall is before optimising
- In this port the **main CPU (game logic)** was the wall, not the GE and not the pixels.
- The GE draws frame N while the CPU runs frame N+1, so the frame time is roughly max(CPU, GE), not the sum.
- Measure both separately before choosing a lever (see C3, "gebench").

### A2. Recompiled code is memory-bound: the I-cache is the main enemy
- Recompiled code was **3.6× the size of the PS2 code** (921 KB of PS2 functions → 3345 KB on the PSP; median function 4.6×).
- The PSP and the PS2 both have a 16 KB I-cache. The original was tuned for it; the recompiled code is not.
- Measured per frame: about **87–90k I-cache misses** and about 30k D-cache misses.
  - About 125 cycles per miss on average.
  - About 800 KB of code touched per frame.
  - Function boundaries cause about 33% of the I-misses.
- Memory stalls were about **46% of the game-update time**. The bottleneck is code size, not the amount of work.
- Profiles are **flat**: there is no single hot function to fix. Gains come from code-generation passes applied everywhere.

### A3. Code-generation levers that worked (exact: game state unchanged)
These took heavy-scene game logic from **95.8 ms to 59.8 ms**, then lower:
1. **Pin the guest-memory base in a callee-saved register.**
   - `register uint8_t *gmp asm("$23")` with `-ffixed-23` everywhere.
   - A typical field access went from **78 to 35 instructions**: with a static array, GCC folds every offset into its own relocated `lui+addiu+addu`.
   - A local copy of the pointer does not help (GCC propagates the constant).
2. **Narrow 64-bit to 32-bit.**
   - The PS2 compiler's idioms (`dsll32 x,k; dsrl32 x,31` bit tests, `dsll32/dsra32` sign extensions, 8-byte struct copies) are not real 64-bit math.
   - A peephole turned **194 of 293** "64-bit" functions into 32-bit code, including most of the hot ones.
3. **Register cache:** guest GPRs become C locals inside a function; the global register file is only the interface at calls and returns.
4. **Hot layout:** order functions so the per-frame hot set is contiguous.
   - A fully-associative cache model showed layout can gain at most about 14%.
5. **Save/restore helpers:** runs of callee-saved saves and restores become calls to shared helpers (about −1.7%, estimate). Exclude looping or hot functions.
6. **VU0 micro programs compiled ahead of time** instead of interpreted.

### A4. Things that did NOT help, or were dangerous
- **`-Os`**, globally or per cold function: slower.
- **Strips instead of triangle lists** for level geometry: no gain. The data is mostly quads, so there were only 4% fewer indices.
- **Lower-resolution rendering:** no gain. Pixels cost nothing (A6).
- **Skeleton or render work on the Media Engine:** **slower overall.** The cache flushes and the shared-bus contention cost more than the offloaded work. Use the ME only for small, independent async jobs such as audio.
- **Dead save/restore removal:** blocked. The game **reads uninitialised stack** (stack contents from an earlier sibling call), so stack contents must stay exact.
  - Possible fix: keep the saves only in the functions whose frames that read can see.
- **Skeleton LOD done inside the recompiled game** (skipping far skeleton updates): broke exactness. The per-frame bone-matrix allocator, attachment queue and counters all change. Reverted. It is only acceptable as a declared gameplay change, with the user's consent.

### A5. GPU/CPU bus contention — the biggest single finding
- When the GE reads vertex data from **main RAM**, it slows the CPU.
  - Game update with the GE idle: **24.0 ms**. With the GE drawing everything: **38.9 ms**.
  - About **15 ms of the CPU's frame were lost** to the GE's memory traffic (level 9.3 ms, characters 4.2 ms).
  - Cycles per miss: about 64 when the GE is idle, 147–160 while it draws.
  - The GE is also slowed (about 11 ms) by the CPU.
- **Fix: put vertex data in VRAM.** The GE reads VRAM over its own bus.

  | Level vertices | GE ms | CPU update ms |
  |---|---|---|
  | float, in RAM | 38.1 | 36.3 |
  | 16-bit, in RAM | 36.4 | 35.3 |
  | **16-bit, in VRAM** | **24.6** | **28.9** |
  | no level at all | 18.5 | 29.0 |

  **Where the data lives matters, not its size.** From VRAM, the level costs the CPU nothing extra.
- The same applied to character vertex buffers:
  - We added a VRAM pool for characters (deduplicated by hash), 576 KB.
  - Characters drawn from RAM instead cost +1.7 ms GE and **+4 ms CPU**.
- **Textures in RAM cost little:** turning off all RAM textures saved only about 0.6 ms GE and 0.2–0.4 ms CPU. With VRAM short, put **vertices** in VRAM before textures.
- **Index buffers in RAM cost about 0.** Keep them in RAM and save the VRAM.

### A6. The GE is not fill-bound in a PS2-style scene
- Same frame with pixels disabled (scissored): level −0.1 ms, characters −0.1 ms. Clipping off: 0.
- Costs are vertices, transform and memory traffic, not pixels.
- So: reduce vertex bytes, move vertex data to VRAM, and cut draw count. Resolution and fill do not matter.
- Measured GE costs before the VRAM work:
  - level ≈ 20 ms; characters ≈ 13 ms; fog pass ≈ 3 ms; props ≈ 1.3 ms; sky < 1 ms; lighting ≈ 0.

### A7. GE data format facts (verified on hardware)
- **16-bit UVs are read UNSIGNED** (value / 32768). 16-bit positions are **signed**.
  - Signed UVs produced a "wrong texture / LOD" look that was easy to misread as a mipmap bug.
  - Fix: per batch, `uo = floor(min u)`, scale by a power of two ≥ the range, store 0..32768.
- **Coplanar duplicate layers need identical arithmetic.**
  - A per-batch position origin (in the model matrix) gave layered surfaces slightly different depths. With a GEQUAL depth test, whole surfaces **blinked between two looks**.
  - Fix: **one global grid and one origin for all batches** (scale = max extent / 65000).
- 16-bit vertex layout used: `u16 u, v; u32 colour; s16 x, y, z, pad` = 16 bytes (float version: 24 bytes).
- Character positions as int16 (bones scaled ×32768): exact enough (99 px of edge noise in a screenshot diff).
- **T8 (256-colour) textures + box-filter mipmaps** for characters (median cut, `GU_TEXTURE_AUTO`, `LINEAR_MIPMAP_NEAREST`):
  - GE 46 → 38 ms, frame 58.6 → 55.6 ms. The user judged it visually fine. Lossy, so ask first.
- **Exact T8:** when a 16-bit (RGB555) texture has ≤ 256 distinct colours (the sky had 55–115), T8 is lossless if the palette entries use the GE's own expansion `x<<3 | x>>2`. The formula `(r*255+15)/31` differs for r = 3, 7, 24, 28.
- Release the RAM copy of anything uploaded to VRAM. That freed 1.2 MB of heap once.
- Build the display list with GU_SEND and send it at the end of the frame **without a sync**. The GE draws while the CPU runs the next frame.

### A8. VRAM budget (2 MB) is the new constraint
- Example layout:
  - framebuffers 816 KB (double-buffered 5650 at a 512 stride, plus 16-bit depth);
  - level 16-bit vertices about 630–670 KB;
  - character vertex pool about 560 KB;
  - whatever is left goes to textures.
- Priority order by measured benefit: **level vertices > character vertices > textures > indices**.
- Larger levels push every texture to RAM. Measured cost of that ≈ 0.6 ms, so it is acceptable.

### A9. FPU: match the PS2's rounding
- **The R5900 rounds toward zero.** It ignores guard, round and sticky bits (EE Core manual; gcc-patches 2013-07 msg00325).
  - It treats exponent-0 operands as zero, flushes denormal results, has no Inf/NaN (overflow → ±Fmax), and gives x/0 = ±Fmax.
- **Allegrex FCR31:** RM = 1 (toward zero) plus FS = 1 → `0x01000001`.
  - Measured with FS = 1: denormal **inputs are used exactly (no DAZ)**, denormal results flush to signed 0, x ± 0 returns x unchanged, inf − inf = 7fc00000.
  - The remaining PS2 differences are those no-DAZ cases only.
- Rounding mode changes the game-state trajectory (different hashes, different HP after 900 frames). Toward-zero is the faithful default. Same speed on hardware; 6060 frames stable.
- Write an **FPU self-test** that runs a table of operations in both modes and compares against expected bits. It found the no-DAZ behaviour.
- x86 reference: set MXCSR FTZ + DAZ + round toward zero (`|= 0x8040 | 0x6000`) to mirror the PSP build. Add `-ffp-contract=off` (no FMA).

### A10. Stability: things that powered the console off
- **Hardware performance counters (block 0xBC400000): reading word +0x50 kills the reading thread** on a PSP-3000. Our build crashed and the PSP switched off.
  - Words 0x00–0x4C are safe. Never reset them, restore PROFEN, keep runs short.
  - **Keep counters off unless the user agrees.**
- **Exit order.** An exception handler left pointing into freed memory, plus automatic exits, caused power-offs. Use this order:
  1. HOME sets a quit flag; the main loop finishes.
  2. `sceGuSync` → `sceGuTerm` → give the exception handler back → `sceKernelExitGame`.
  3. The callback thread falls back to doing it itself after 2 s.
  4. Benches **wait for a button** and never exit automatically.
- **Overclock:** only through the CFW's own clock call.
  - 365 MHz gave −8.6% frame time (external report). Writing the PLL register directly switched a console off.
  - ARK-4 is capped at 333 MHz; ARK-5 offers more. Changing the user's CFW is the user's decision.
- Never write flash0 or NAND, never touch power or clock registers, keep kernel-mode and ME code minimal and pointed only at your own buffers.

### A11. Stubbing the presentation layer: verify, don't trust names
- The headless original game keeps all game logic and turns the presentation layer (render, sound, particles, HUD, FMV, debug UI) into no-op stubs, chosen by a **class-name regex**.
- **A name regex will catch game logic sooner or later.**
  - In a later level the **camera-path spline class** (camera paths for scripted cameras) was matched as presentation. Its stubs returned 0, the camera went to (0,0,0), and **the level stopped drawing** at about frame 160.
  - Things that are game logic even if they sound like presentation: camera, camera paths, frustum and visible-sector lists (they decide which objects are active), fade services that trigger respawns, particle-object allocation (decides whether an object exists).
  - Fade example: corpses are removed by the fade-expiry trigger. With the fade stubbed, corpses stayed forever and spawners never refilled.
- Count every stub hit, and list the **non-presentation** ones in a debug build.
- When a level misbehaves, trace the camera matrix and active-object counts per frame on the PC build first.
- After un-stubbing, check that earlier levels stay **byte-identical** (guest-memory dump comparison).

### A12. Frame-time spikes
- Lazy conversion at a texture's first draw (T8 quantise + mipmaps) took **0.4–1.2 s in total** in the emulator, spread over the frames where new characters appear. That causes visible hitches.
- Convert at level load, or offline.
- Early frames after level start are heavy (spawns, first skeletons): 85–105 ms of game logic in the emulator for the first 30–210 frames. Report them separately from the steady state.

### A13. Gameplay levers (change the game: only with the user's consent)
Implemented as hooks around the original functions, configurable and recorded in the replay header:
- far NPCs (beyond 3 m) update every 2nd frame with the accumulated dt; their world-collision requests at the same rate;
- awake-enemy cap 10 (the spawner's "number alive" answers "full"; the original retries 5 s later);
- chosen allies' AI ("brain") every 3rd frame;
- fewer allied NPCs parked (for example 4 elves instead of 5, 2 followers instead of 3).

These are **per-level by actor name**. Names from one level don't apply to another.

---

## Part B — Evidence and history (version by version)

| Version | Change | Hardware result |
|---|---|---|
| v5→v8 | memory base register, 64→32 narrowing, register cache, layout, VU0 AOT | heavy-scene logic 95.8 → 59.8 ms |
| v9 | more emulator-model gains | no hardware change: **the emulator's cost model was ~2× optimistic** |
| v10 | input record/replay, FPU self-test, boot-step hashes, hardware counters | counters crashed the PSP (word +0x50) |
| v11 | save/restore helpers; bench alternating GE sync per frame | proved bus contention (update 8.47M vs 6.39M cycles) |
| v12 | counters removed | 6120 frames stable |
| v13 | character positions int16; T8 + mips option | GE 46 → 38 ms, frame 58.6 → 55.6 ms |
| v14 | T8 default; sky exact T8 (−640 KB); gebench | GE per part: all 34–40, characters 13–15, level 15–20, fog 3 |
| v15 | release level texture RAM copy (+1.2 MB heap); strips | strips ≈ 0; untextured level ≈ 0 |
| v16 | gebench pixel / clip modes | pixels and clipping cost ≈ 0 |
| v17 | ordered exit, round-toward-zero option, "CPU waits" mode | contention ≈ 15 ms; GE 38.3 / update 38.9 ms |
| v18 | bench-only test: level vertices 16-bit in VRAM | GE 24.6 / update 28.9 (from 38.1 / 36.3) |
| v19 | VRAM level vertices default; UV sign fix | GE 25.0 / update 28.9 ms |
| v20 | indices to RAM, character vertex VRAM pool | GE 23.5 / update 26.1; frame end 40.6 ms |
| v21 | flicker test | blinking surfaces found |
| v22 | one global grid / origin | blinking fixed (user confirmed on hardware) |
| v23 | round toward zero by default | bench average 45.8 ms/frame (v17: 56.0); play 43.6 ms (≈ 23 fps) |
| v24 | level select; camera-path spline un-stubbed | two more levels run in the emulator; earlier level byte-identical |

Steady state (v23, first level): CPU logic ≈ 40 ms, GE ≈ 24 ms in parallel → about 23 fps. **The CPU is the wall; about half of its time is I-cache misses.**

External reports used (findings only, no code). These come from the SandstoneDev PS2RC / universal-ps2-recompiler / psp-developer repos:
- the counter +0x50 crash;
- exit-order power-offs;
- the R5900 rounds toward zero;
- skipping off-screen render passes was their biggest win (check your port: in ours none ran);
- measure GE time from send to finish, not with a busy sampler;
- texture-fetch traffic slows the CPU;
- 365 MHz through the CFW call: −8.6%;
- their recompiled open-world game reached 17–21 fps outdoors and 30 indoors.

---

## Part C — Method that worked

### C1. Deterministic reference
- No input, fixed dt (1/30), N frames. Hash all of guest memory (FNV) at fixed frames, and print one gameplay value (for example the hero's HP).
- Keep three references that must agree:
  - an **x86 build** of the same generated C (with FTZ/DAZ/rounding set as on the PSP);
  - the **emulator**;
  - **hardware**.
- Every code-generation change must keep the hashes **identical**. Any difference means a bug, or a deliberate gameplay change that gets a new reference.
- Input record/replay (8 bytes per frame: pad bytes + dt, with the settings in a header). It reproduces hardware play sessions on x86, and gives low-noise A/B runs.
- A hash per boot step helps find where emulator and hardware diverge (that found the FS/denormal behaviour).

### C2. The emulator (PPSSPP) answers booleans, not costs
- Its GE timing is ≈ 0 and its CPU numbers are about 2–2.5× optimistic.
- Use it for "does it boot / render / crash", screenshots, and boolean checks.
- Patch it to match the hardware FPU (FS without DAZ, x ± 0 passthrough) so its hashes equal hardware.
- A cache-model profiler inside the emulator interpreter matched hardware I-misses within about 5% (82.9k vs 87.3k) after fixing its address range. It is good for layout work, not for absolute ms.
- Headless runs: build with a frame limit so the program does not wait for input at the end.

### C3. "gebench": the instrument for GE questions
- Cycle render modes on consecutive frame pairs: everything; characters from RAM; textures off; no level; no characters; CPU waits for the GE.
- Report the GE time (list sent → finished) and the CPU update time per mode every 300 frames.
- Render toggles must not change the game hash. Check it.
- The "CPU waits" mode separates the uncontended GE time from the uncontended CPU time, so the difference is the contention.

### C4. Discipline
- **One change per measurement.** Take every cost from hardware.
- Log memory headroom (binary-search malloc). We had 5.8–6.7 MB.
- Revert experiments that break exactness or show no gain. Keep copies of the last good generated files before regenerating.
- When adding levels: run the PC build for 1000+ frames with a camera / active-object trace before touching the PSP.

---

## Part D — Open levers (not done yet), with expected size

| Lever | Expected | Type |
|---|---|---|
| Texture conversion at load instead of first draw | removes the hitches | safe |
| Guest arguments / returns in C parameters (calling convention) | −3 to −10 ms (estimate; call protocol ≈ 17% of instructions, ~19 host instructions per guest call) | exact, long |
| Write return values back only where a caller reads them | ≈ −2 ms (external estimate) | exact |
| Cold-path marking + call-chain order | ≈ −1 ms (external estimate) | exact |
| Jump tables as a real switch on the index; indirect calls through a hash table (the inlined dispatch switch was 88 KB) | unmeasured | exact |
| Dead save/restore removal with the sibling-frame exception | unmeasured | exact if done right |
| Compact or VFPU form of the hot skeleton VU0 program (51 KB of generated code) | unmeasured; the VFPU form is not bit-exact | needs a gameplay check |
| Real PSP scratchpad for the guest scratchpad | D-misses are only ≈ 5 ms in total | exact |
| More gameplay LOD / skeleton update every 2nd frame for far figures | −2 to −5 ms in busy scenes (estimate) | consent |
| 365 MHz (ARK-5) | −8.6% | user's CFW decision |
| More textures in VRAM | ≈ −0.6 ms | not worth it |
