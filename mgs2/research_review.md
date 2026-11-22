# Research re-read for the MGS2 port (2026-10-21)

Ffm asked (2026-10-21 14:33) to re-read everything the investigation has gathered, including the public GitHub
sources, so the MGS2 port does not miss anything. This file is a review, not new research: every item is labelled

- **applied**: already in the port (where),
- **planned**: already in a plan or "next" list (where),
- **missed**: not in our notes, or in them but not acted on or planned (with its source),
- **n/a**: does not apply to MGS2 or is ruled out by Ffm's rules (why).

Nothing in the port was changed for this review. The decision on each missed item belongs to the "MGS2 port" thread.
Items marked *(inferred)* are reasoning, not measurements.

## 0. Short version

- 20 missed items (M1-M20 in section 3; M21 was a check that came out fine). The ones that matter most for the next big step, the GE renderer:
  1. **M1: 2 MB more VRAM may be available on a PSP-3000.** A kernel helper can switch the GE to 4 MB eDRAM
     (`sceGeEdramSetSize`). Our notes say the extra 2 MB is the Media Engine's; the newer source says it is GE eDRAM
     that the system leaves off. It needs one hardware probe. It would remove the VRAM squeeze in renderer_notes §10
     item 6 (frame + depth + 688 KB hot textures + blur copy).
  2. **M2: a bit-exact PSP GE model now exists on PC** (PPSSPP master, PR 22414, merged 2026-10-05). It gives
     device-accurate pixels and depth for checking the renderer without a hardware run per question. Our PPSSPP is
     1.20.4, which is close but not exact.
  3. **M3-M5: GE arithmetic rules** that the renderer plan does not have yet: 16-bit integer depth (the near plane is
     the lever), float24 transform (keep matrices camera-relative), guard band 0..4096 with only a near clip,
     unclamped doubled inverse blend factors (a possible exact form of the PS2's alpha above 0x80, the suspect for
     the replay's "5-15 levels darker" 3D area).
  4. **M6: key texture and mesh caches by content**, not by address (renderer_notes §8 keys pages by page pointer).
- Harness hygiene (M13-M18): log robustness on a crash, power lock while the volatile 4 MB is held, thread stacks
  for future threads, an A/A control in the alternating runs, the 1.4 s step at vb 24667.
- Public sources: PS2RC code we copied is unchanged upstream (all 33 code files and NOTICE.md equal to fac73d4 and to today's main); only its licence moved
  to 1.2, which is more permissive and changes nothing for a personal port. MGS2 decomp symbol files unchanged.
  pspdev gcc 15.2 still has no Allegrex scheduling model. New since our research: the psp-developer hardware notes
  (GE arithmetic, VFPU, memory map), which also confirm three of our own hardware findings (section 2).

## 1. What was re-read and re-checked

Re-read (project files): tt_port/ (ORIENTATION.md, PS2_to_PSP_optimisation_lessons.md, TT_PSP_port_handoff.md,
src/research_summary.md), mgs2/ (feasibility, stage_notes, sound_notes, elf_notes, decomp_names_sles, web_research,
peacewalker_notes, sandstone_review, ark4_options, speed_notes, renderer_notes, psp_harness, recomp/README,
third_party/ps2rc/SOURCE.md, re/*.md), project memory, and the port's current code where a claim needed checking
(psp/main_psp.c, runtime/iop_host.h, recomp/regcache.py, psp/ge_replay/main.c).

Re-checked online (github.com and its API are blocked here; raw.githubusercontent.com works):

| Source | Status on 2026-10-21 |
|---|---|
| SandstoneDev/PS2RC | The 33 code files and NOTICE.md in third_party/ps2rc are byte-equal to fac73d4 and to today's main. LICENSE changed: 1.1 -> 1.2 (drops the "Listed Disc" distribution clause, allows removing the splash screen if the README keeps the credit). Nothing to update. |
| SandstoneDev/universal-ps2-recompiler (knowledge base) | 39 notes in the index; the ones not cited in sandstone_review.md were read now. New items are in section 3. Kit licence is also 1.2. |
| MGS2 decomp (mgs2old) | README, sym.txt (245 names), libsym.txt (677) unchanged since decomp_names_sles.md. |
| psp-developer (PSP homebrew notes, PPSSPP team GE/VFPU studies) | Read: ge-arithmetic, rendering, textures-uv, real-hw-gotchas, optimization-profiling, vfpu-math, memory-map. Source of most new items. |
| PPSSPP | Per psp-developer (status 2026-10-15): bit-exact SoftGPU PR 22414 merged into master 2026-10-05, milestone v1.21; latest release still 1.20.4. The PR page itself could not be fetched from here. |
| pspdev gcc (allegrex-v15.2.0) | Still no `define_insn_reservation` for Allegrex: no pipeline model, as web_research.md said. |
| ARK-4 | Re-checked today in ark4_options.md; nothing newer. |

## 2. Public sources now confirm three of our hardware findings

- **VFPU rounding**: round-to-nearest is hardwired, denormal inputs count as zero, denormal results flush to zero
  (vfpu-docs via psp-developer vfpu-math §1). Matches hwprobe (v12 probes). **applied** (checked VFPU groups, fallbacks
  round like the VFPU).
- **The v12/v13 VFPU check bug**: "`mfvc` right after `vcmp` reads the old condition code; put one VFPU instruction
  between them" (vfpu-docs, "Register hazards"). That is the documented cause of the stale CC read we inferred from
  the missed-check counts. **applied** (v14 bvtl group check reads CC right 4096/4096).
- **FPU compare on Inf/NaN powered a PSP off** in another port (exception enabled). Our FCR31 0x01000001 has no
  enable bits set, set before any guest code (main_psp.c:358). **applied**.
- Rule for new VFPU forms (vfpu-docs errata, not seen in our forms because all hashes match on hardware):
  `vmmul`, `vtfm*`, `vhtfm*`, `vqmul`, `vcrsp` must not overlap destination and sources; `vhtfm2.p`/`vhtfm3.t` write
  wrong elements for destination index 64+; PPSSPP models none of these, so a new form must be proven on hardware
  (as the method already requires).

## 3. Missed items

### Renderer (GE) items

**M1. Upper 2 MB of VRAM on the PSP-3000.** *missed*
- Source: psp-developer real-hw-gotchas §6 [measured: PSP-3000, streaming map engine]; memory-map §2 [public: uofw
  ge.c, exports.exp; pspsdk pspge.h].
- Facts: PSP-2000/3000 have 4 MiB of GE eDRAM; the system enables 2 MiB. `sceGeEdramSetSize` (2 or 4 MB) is a
  kernel-only export, so a small kernel helper must call it, once at boot before GE init, never on a live scene.
  Trust only `sceGeEdramGetSize()` == 4 MiB afterwards and gate every upper-half use on it. Keep frame buffers in the
  lower 2 MiB. PPSSPP returns success but mirrors the lower half, so the emulator cannot validate it. Without the
  switch, 0x04200000 is a mirror (one port saw stripes there on a PSP-3000). Calling an unresolved import (helper
  absent) crashes.
- Our notes: ark4_options.md line 62 reads the 4 MiB as "2 MiB for the GPU, 2 MB for the ME". The newer source says
  the ME's 2 MB is separate local memory and the extra 2 MiB is GE eDRAM left disabled.
- Why it matters: renderer_notes §10 item 6 says the 2 MB VRAM may not hold frame + depth buffers, the 688 KB of hot
  textures and the blur copy; vertices in VRAM (TT's ~15 ms lesson) compete for the same space.
- Recommendation: one hardware probe (hwprobe item): load a kernel helper at boot, call SetSize(4 MB), log
  GetSize, write and read back a pattern in the upper half through the uncached view, draw one texture from it on the
  GE. The helper must export `module_stop` and undo every registration (kb: kernel-module-export-module-stop,
  power-off-around-exit), and be loaded before the GE starts. It touches a GE setting, not flash, NAND or power/clock
  registers. A kernel module is new for this port, so it is worth telling Ffm before the probe ships.

**M2. Bit-exact GE reference on PC.** *missed (known since sandstone_review.md §2.5 on 2026-10-17, not set up)*
- Source: psp-developer ge-arithmetic "Which PPSSPP to trust" [public: PPSSPP PR 22414 + releases]; pspautotests
  `utils/ppdmp-playback`.
- Facts: PPSSPP master's software renderer is "mostly bit exact" against a PSP (pixels and depth); 1.20.x is close but
  not exact; hardware backends are never a pixel reference. Not modelled: swizzled depth mirrors, lazy 8-row
  self-texture loads, REGION1, any timing. A `.ppdmp` frame dump can also be replayed on the PSP over PSPLink and
  compared by MSE.
- Why it matters: GE replay v1-v3 checked pictures in PPSSPP 1.20.4 (blur cut "205-251 pixels differ", the "5-15
  levels darker" 3D area). A bit-exact model answers such questions on PC.
- Recommendation: build PPSSPP master headless with `--graphics=software` (needs a source archive from Ffm's PC,
  since github.com is blocked here; the container patch for PPSSPP_VFPU_RN would need porting). Use it for pictures
  only, never for time.

**M3. 16-bit integer depth against the PS2's Z24.** *missed (rules)*; 16-bit Z itself is *planned* (feasibility §8)
- Source: psp-developer ge-arithmetic (depth), rendering (depth precision); kb ge-3d-backend-costs.
- Rules: the near plane is the main lever (push it out as far as the scene allows); `sceGuDepthOffset` or a small
  bias for decals instead of equal depths; keep `GU_CLIP_PLANES` on (with `sceGuDepthRange(65535, 0)` the far plane
  maps to -1 and is clamped only with clipping on); nothing is clipped at the far plane.
- MGS2 detail: the PS2 draws with GEQUAL (larger is nearer) on Z24. The GE mapping must keep that order with 16 bits;
  z-fighting where Z24 had room is the risk *(inferred)*. Check with M2.

**M4. Float24 transform and the guard band.** *missed (rules)*
- Source: psp-developer rendering "Big worlds", ge-arithmetic (clipping) [measured by the PPSSPP team on a PSP];
  kb ge-3d-backend-costs.
- Facts: matrices reach the GE as float24 (15 fraction bits); at |T| ~ 3000 one ulp is 0.0625 units, so world
  coordinates make vertices swim while the camera moves. Any vertex outside screen 0..4096 discards the whole
  primitive; the only clip plane is near; sprites are never clipped (W < 0 discards them); triangles spanning more
  than ~2730 px in Y draw extra pixels.
- MGS2 *(inferred)*: the pre-lit meshes use the per-mesh local-to-clip matrix (mesh+0x40), whose translation is
  already relative to the camera, so they are safe. Paths that hand the GE world-space data (sort-pass float4
  streams, the sea, particles, effect fans) must subtract the camera on the CPU in float32, and large ground or sea
  triangles need splitting or a CPU clip at the guard band.

**M5. Blend and texture arithmetic of the GE.** *missed (facts)*; alpha doubling is *planned* (renderer_notes §9, blend)
- Source: psp-developer ge-arithmetic "Pixel operations" [measured by the PPSSPP team on a PSP].
- Facts: each blend term is ((2c+1)(2f+1))>>10 with one clamp at the end; a fixed factor of 255 is an exact x1;
  **the doubled inverse factors are not clamped, so with alpha >= 128 their term subtracts**; MODULATE is
  (t*(c+1))>>8, fragment alpha ((a+1)*a_tex)>>8; colour doubling also doubles the specular colour.
- MGS2 *(inferred)*: on the PS2, (Cs-Cd)*As+Cd with As > 0x80 also makes the Cd term negative. Raw alpha plus the
  doubled factors may reproduce it exactly, where `min(255, 2a)` in the converter clamps at 1.0. renderer_notes §9b
  already names alpha above 0x80 as the suspect for the 3D area being 5-15 levels darker. Test: count draws with
  alpha above 0x80 in gsref, then compare both forms in M2.

**M6. Key texture and mesh caches by content.** *missed*
- Source: kb ge-3d-backend-costs, ge-display-list-safety (PS2RC: "a cache keyed by address returns" the wrong mesh
  after streaming); psp-developer textures-uv ("key by a globally unique identity or content hash").
- Our plan: renderer_notes §8 (textures) converts each of the 32 pages at 0x1e6820 once, "cached by page pointer", dropped when
  the stage loader rewrites the page.
- Recommendation: key by a cheap content hash (or size + a few sampled words + a full hash in a debug build) so a
  page rewritten by anything other than the stage loader (animated textures, codec, radar, cutscenes) cannot show
  stale texels. Same for mesh vertex caches.

**M7. Exact sampling rules for sprites and copies.** *missed (facts)*
- Source: psp-developer ge-arithmetic "Rasterisation" [public: PPSSPP GE docs; measured on a PSP].
- Facts: sprite UVs come from planes through three corners; a sprite drawn bottom-left -> top-right has its UVs
  rotated; colour and depth come from the second vertex; two triangles and one sprite over the same rectangle sample
  different texel rows; a 1:1 bilinear sprite hits texel centres only when its area is a power of two; texel
  positions are in 1/16 texel truncated toward zero; bilinear weights are 4 bits.
- MGS2: renderer_notes §9c found every blur cut changes 205-251 pixels ("the GE places texels a texel apart in
  places for a reversed or cut sprite"). These rules explain it and tell the renderer how to cut the blur (always
  top-left -> bottom-right, power-of-two-area tiles or NEAREST for 1:1 copies) so the fast form can be pixel-exact
  with the slow form.

**M8. Texture cache and limits.** *missed (facts)*
- Source: psp-developer textures-uv, ge-arithmetic; kb ge-display-list-safety.
- Facts: 8 KB texture cache (64 lines, 16 sets x 4 ways); `TEXFLUSH` after changing texture registers inline,
  `TEXSYNC` after a block transfer into a texture; 512 x 512 maximum; 16-byte alignment.
- MGS2: the GS uses 1024-wide areas (the frame buffer read as CT32 for post); any texture wider than 512 must be
  split *(inferred; check region sizes in the gsref census)*.

**M9. GE list safety and kick batching.** *missed (as a checklist)*; parts *applied* in the replay
- Source: kb ge-display-list-safety, ge-3d-backend-costs [measured: PSP-3000].
- Rules: guards on primitive counts, CALL depth <= 2, range-checked addresses; a start-up self-test of the
  hand-written command encoder against PSPSDK's own words, feature off on mismatch; a run-time kill switch; full
  state reset at each list start; lists written through the uncached alias; kick every ~16 meshes (each kick is a
  system call; 151 k -> 27 k kicks); per-mesh CPU cost on the PSP is 4x PPSSPP's.

**M10. 2D and post passes.** *missed (facts)*
- Source: psp-developer rendering (post-processing), real-hw-gotchas.
- Facts: through-mode draws inherit the cull state (disable culling for every 2D pass); a 5551 copy carries garbage
  alpha (use `GU_TCC_RGB`); no dithering on additive glows; set the scissor for every render target (stride wrap).
  Already in our notes: never sample the target being drawn.

**M11. Generated mipmaps as a GE-time option.** *missed (option; visual change, needs Ffm's consent)*
- Source: kb texture-fetch-traffic [measured: PSP-3000]; psp-developer optimization-profiling §8.
- Facts: on the PSP the texel fetch traffic is the GE cost; full-resolution textures plus generated mipmaps (2:1
  point decimation keeping the palette indices, levels down to the swizzle floor T4 32 px / T8 16 px, +31% bytes)
  ran as fast as half-resolution textures and looked sharper; more than one level splits the 8 KB texture cache.
- MGS2: textures were 48 of the 75 ms in GE replay v2b; scene meshes are minified (T4 3.3, T8 8.1 texels per pixel).
  The deck frame already fits (38.3 of 40 ms) without it. Offer it as the best option only where a scene's GE time
  goes over budget; the PS2 uses no mipmaps on these meshes (only the sea sets mip registers), so it changes the look (less shimmer at distance).

**M12. 3D lists built on the Media Engine.** *missed (option)*
- Source: kb ge-3d-backend-costs [measured: PSP-3000]: building the 3D lists on the ME saved 3.2 ms per frame.
- MGS2: our "ME offload: don't pursue" note covers game logic. For the renderer the ME could build lists from
  converted data *(inferred; needs the D-cache writeback rules in memory-map §7)*. Consider after the CPU path works.

### Harness and hygiene items

**M13. Log file survives a crash.** *missed*
- Source: kb fat-size-recorded-on-close [measured: PSP-3000].
- Fact: FAT records the file size on close; a crash or power-off loses everything after the last close, even after
  `fflush`. main_psp.c writes with `fputs` + `fflush`.
- Recommendation: commit the log as open-append-close per line group (or every N lines), at least during the boot
  and the first steps, where a crash is most likely.

**M14. Power lock while the volatile 4 MB is held.** *missed*
- Source: psp-developer memory-map §6 [measured: PSP-3000]; kb power-off-around-exit.
- Fact: the volatile partition belongs to the power/suspend service. Holding `scePowerLock(0)` while it is locked
  makes the power switch and sleep wait until unlock. Whether the content survives a suspend is untested. The SPU2
  RAM lives there (main_psp.c:241).
- Recommendation: take the power lock with the volatile block, release it in the exit order. It is a system call,
  not a power register write.

**M15. Threads created later need partition memory, not heap.** *missed*
- Source: psp-developer memory-map §7 [measured: PSP-3000]; ark4_options.md (v19 log: heap headroom 4096 KB,
  outside the heap 764 KB, largest block 512 KB).
- Fact: stacks come from the partition; after `PSP_HEAP_SIZE_KB(-1024)` takes its block, a thread created later
  fails with 0x80020190 while the heap has megabytes free.
- MGS2: the planned stream thread, ME audio worker and any GE-callback or presenter thread must be created at boot,
  or get partition blocks reserved early.

**M16. A/A control and intervals in alternating runs.** *missed*
- Source: kb one-run-interleaved-ab, ab-sets-control-again [measured: PSP-3000].
- Fact: an in-run A/B needs a control-again arm (same config twice) and a paired median with a 95 % sign-test
  interval; one A/A check came out "+1.08 ms slower", so a lone ~1 ms difference is noise.
- MGS2: the alternating windows (every 100 vblanks, or every vblank in v20) compare means. Adding an A/A arm and an
  interval to steps_percall.py costs nothing on the PSP and tells us which sub-1 ms wins are real.

**M17. The 1.4 s step at vb 24667.** *missed (known, cause not examined)*
- Source: psp_harness.md (v19 842 + 589 ms, v20 one 1.4 s step; the emulator run has it too); kb
  hitches-log-and-disc, memory-stick-open-file-limit (`sceIoOpen` 17-34 ms, sequential reads ~15.5 MB/s).
- MGS2: the PSP reads the disc pak synchronously in the game thread (runtime/iop_host.h `disc_pak_read`). The game is
  a load at that moment.
- Recommendation: log bytes and time per disc read on the PSP for that window. If reads dominate, PS2RC's rule
  applies: a reader thread above the game's priority (and the PS2 itself streamed during the load). If guest work
  dominates, it is the game's own load time.

**M18. Smaller harness items.** *missed*
- `scePowerTick` once per frame, so a long run cannot hit the XMB auto-sleep (psp-developer real-hw-gotchas).
- If an exception catcher is ever added: chain the firmware's own `break` (CE=1/2) instead of reporting it
  (real-hw-gotchas §7, measured: a catcher that passed it down killed a console at boot).
- `MEMSIZE` is a request: a damaged memory-stick FAT made ARK read its flash settings and give 24 MB with no error
  (memory-map §4). Our 51.1 MB image would then not start at all; that is the symptom to recognise.
- Keep at most ~8 files open (memory-stick-open-file-limit). Today the PSP uses one pak file plus the log, so fine;
  relevant for the stream thread.

### CPU items

**M19. Base-register masking with guard areas.** *missed (known since sandstone_review.md §2.4, not planned)*; a weaker form is *planned*
- Source: sandstone_review.md §2.4 (PS2RC: mask only the base register, 64 KB guard areas: -6 % frame time).
- MGS2: speed_notes §18 counts 135 k checked accesses per step (`srl 28` + branch + `ext` + base add, about 4
  instructions each, 8.6 % of instructions). The plan is "one range check per base register and block" (0.3-0.4 ms).
  The guard-area form removes the per-access check for every access with a 16-bit offset *(inferred: about 2
  instructions saved per access, roughly 1 ms per step by the cost model)*. It must still route I/O and scratchpad
  addresses, and keep hashes equal.

**M20. Return values written only where read.** *partly applied*
- Source: sandstone_review.md §2.4 (PS2RC: -8.25 % code, -2 ms).
- MGS2: recomp/regcache.py has interprocedural IN/OUT sets (stores only callee-IN dirty registers, reloads only
  OUT-live ones). A per-call-site "return value never read" pass is not in the notes *(check whether IPO already
  covers v0/v1)*.

**M21. Double-precision calls.** *checked: fine*
- Source: psp-developer vfpu-math §3 (one PC port had 5,633 soft-float double call sites).
- MGS2: `double` appears only in log and statistics code (boot.c, main_psp.c, hwprobe.c, ge_replay). A one-line check
  of each new build: `psp-objdump -d EBOOT.elf | grep -c 'jal.*<__\(add\|sub\|mul\|div\)df3>'`.

## 4. Already applied (from the sources)

FCR31 0x01000001 before guest code; no hardware counters; ordered exit; never auto-exit; stock 333/166 MHz via
`scePowerSetClockFrequency`; checked VFPU groups with exact fallbacks (Ffm's consent); v14 CC read fix; machine state
in the scratchpad; claimed-line copies (cache 0x18, self-tested); PGO + I-cache code order from both routes;
ldu fusion, jump tables, stack fast path, poll countdown, native qcopy, native line_sph; zero-time pipeline clock;
reduced renderer on undisplayed deck steps; per-stage overlays; host stack pool inside the main thread's stack
(PS2RC: coroutine stacks must live in the thread's stack); SPU2 RAM in the volatile 4 MB; free-memory logging at
start and end; malloc probe written with volatile (GCC deletes plain probes); md5 deploy check; alternating configs
in one run; GE replay: serial timing with the CPU idle (kb ge-busy-sampler-pitfall), top-left/16-row blur, trail in
16-px columns, VRAM textures, merged draws, colour doubling for MODULATE (PS2 vertex colour 0x80 = 1.0), alpha test
only where Z is written.

## 5. Already planned

GE renderer replacing DG_ActLast and the GS walk (pre-lit meshes first, Bézier sea, two-pass alpha test, GE fog,
GE skinning, T4/T8 + 8888 CLUT with alpha doubling); fresh PGO + order for v21; reduced renderer from boot (after the
persistent-buffer fix) and the whole-game reduced pipeline; ME audio split (PS2RC `spu2_split`, shadow mode);
WSOLA time-stretch; sceSas comparison; PSMF movies; stream thread; stage code dedup; vertices RAM vs VRAM re-measure;
ARK partitions 8/11 via kernel mode later; `-frounding-math` check; renderer-free game hash; D-cache data
placement; one range check per base register (see M19).

Known and still open (not scheduled):
- **PCSX2 over PINE as an oracle** (sandstone_review.md §2.5; recomp/README.md "next"). Our hashes prove x86 = PPSSPP =
  PSP; nothing yet checks x86 against the original game's own execution. It is the only check of that kind and
  needs PCSX2 and Ffm's own BIOS on the PC.
- **Display pacing at 25 fps** (sandstone_review.md §2.3, web_research.md "LCD refresh"): the LCD runs at 59.94 Hz
  only, so 25 fps shows frames for 2-3-2-3 refreshes and judders; the choice (25 shown, 30 shown with 50 Hz logic,
  or an interpolated 60 Hz display, which is a visual change) is Ffm's when the renderer runs.

## 6. Not applicable

- Overclock above 333 MHz (Ffm's rule; also, the GE clock does not move with it).
- TT-only formats and Lua; ATRAC3plus music (MGS2 sound is SPU2 ADPCM through the IOP driver).
- Peace Walker's overrun timing and GTA's variable timestep: MGS2 keeps 50 logic steps/s for exactness.
- Dynamic resolution, half-resolution textures, 16-bit buffers: visual changes, measured only unless Ffm consents.
- PS2RC's "never use the VFPU for game results": superseded by Ffm's VFPU consent with exact fallbacks.
- PSP-1000 VFPU errata (`lvl.q`/`lvr.q` corrupting an FPU register): Ffm's console is a PSP-3000.
- Streaming budget thrash (kb streaming-budget-thrash): MGS2 loads each stage whole; revisit only if the stream
  thread gets a budget.
