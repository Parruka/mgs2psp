# MGS2: the plan to 20-25 fps on the Tanker deck (2026-11-18, the lead's plan)

Ffm, 2026-11-18 20:10-20:19: full lead, "just complete the deck runing at 25/20 fps, the more the better, go for smart solutions downscale what is needed for a psp port"; the only rule that cannot be broken is writing the PSP's flash. My own safety lines stay: no flash0/NAND, never above 333 MHz, no power-register writes.
Marks: [C] measured on the PSP or read in code, [C model] the PSP cost model, [I] my estimate. Single estimates in this project have been off by 2x in both directions, so every phase ends with a PSP run that settles it.

## COURSE CHANGE 2026-11-23 15:58 (lead; Ffm 15:57: "you are the boss, i dont take decisions")

- Where we are [C gl39]: rain wait 78 ms a step (6.4 fps), main-CPU limited; the ME is busy ~71 ms on list translation. About half of the main
  CPU's step is imitating the PS2's drawing (prof/hw_gl39/where_78ms_go.md). The exact 1-3 ms levers top out near 7 fps in the rain
  (main/attic/top3/levers_top3.md), because the ME then sets the pace.
- Decision: the renderer stops being batch-identical. Models are cooked once at stage load into GE-ready data and drawn by the GE with its
  own hardware transform, lighting and skinning (as the HD port did with shaders). Game logic stays exact (state hashes equal on x86,
  emulator and PSP; the game reads back only obj->world from the passes, hd_study §7.1, to be verified per step). Pictures are judged by
  side-by-side shots, not digests. The freed ME takes the sound mix (~7.5 ms a step, not in any build yet).
- Then: delete the rain work nobody sees (guards' dice kept by the seed jump, hd_study §1), lighter rain (Ffm's "B"), then game-code speed.
- Expected [I, 2x]: rain deck 12-15 fps, quieter areas near 20; 25 everywhere not expected; 20 in the rain not reachable by any approach.
- CHECKPOINT told to Ffm: if the new renderer's first two steps (ship meshes, people) do not lift the rain wait above ~10 fps (under ~50 ms
  a step) on the PSP, I recommend stopping the project.
- Design: native_ge_renderer_design.md (in progress). gl40 still ships (copy planning, placement). H1 and the gl41 small levers finish as
  ready patches; other small exact levers are parked. The text below is the 2026-11-18 plan (history; its 20 fps estimate is superseded).

## UPDATE 2026-11-23 16:5x (lead): the renderer design's numbers correct the outlook above

- native_ge_renderer_design.md [I, 2x]: rain wait after ship 76-79, ship + people 64-72 (~7.4 fps), all deck kinds 54-63 (~8.6 fps);
  floor if drawing cost the main CPU nothing 52-55: game logic 34.4 [C, K7] + the drawing code whose results the game reads ~14 [C model]
  must keep running. With rain deletes + gl41/gl42 + generator G1/G2: 39-54 (~11 fps). So the "rain 12-15, quieter near 20" told to Ffm
  at 15:5x was too high; quieter areas are UNKNOWN until measured.
- The "~10 fps after ship + people" checkpoint measures a state the plan never stops at: REPLACED by the design's rule: the step-2 PSP
  run has turns K-A / K-N1 / K-N2 / K-N2R (other chains untranslated, upper bound) / K7; from K-N2R's profile project the full renderer's
  main CPU P; stop the renderer track if P >= 62, go on if P minus the separately measured logic levers < 50.
- With exact logic the rain deck tops out near 11 fps [I]. 20 fps in the rain needs the HALF-RATE GAME (one logic step per picture with
  doubled time): per picture ~39-54 ms -> 18.5-25 fps [I]. Promoted from last resort to a study track now: first read how the HD port's
  Vita build switched 30/60 (MGS2_DYNAMIC_FRAMERATE, BP_MGS2_5060) on Ffm's PC (Remote Control, read only), then size it on our binary
  (halfrate_study.md). If half rate cannot work, the rain deck stays near 10-12 fps and I tell Ffm whether to stop.
- The renderer continues either way (needed under both routes; it also frees the ME for the sound mix). Next: step 0 (cooker, container).

## Short answer

- **The rainy deck today: 148 ms a game step, 3.4 fps, the game at 14% speed** [C gl17]. It must be about 20 ms a step for 25 fps at full speed: 7x.
- **Two sides must each shrink about 3x** once the Media Engine draws in parallel (gl18me, now in your folder): the main CPU (the game's code and its PS2 draw pipeline, 70 ms a step) and the picture (our GS walk and GE translation, 75 ms a step).
- **The smart move is to stop emulating the PS2's drawing on the deck.** A new renderer reads the game's own objects (models, matrices, texture pages, effects list) and writes GE lists directly, on the Media Engine. That removes the PS2 draw pipeline from the main CPU and the GS walk and translation from the picture side. It is the one lever large enough for both sides [I: native_draw_study.md §4].
- **The game's logic then has to come down from about 35 to under 20 ms a step**: cosmetic actors every other step, lighter rain logic, a fresh profile-guided build and code placement, and native code for the 30 functions that carry half the game code's time.
- **Expected end result [I]:** 20 fps with the game at or near full speed on the rainy deck; 25 fps there only with lighter rain. Inside the ship 25 fps is likelier. Each phase's PSP run will say how close we are.

## 1. The numbers that set the target

- The game has no time step [C, logic_timing.md]: everything moves once per 50 Hz step, and the virtual vblank count must stay 1 per step (otherwise the random sequence changes). A slow PSP gives slow motion, not dropped frames.
- So **full speed = 50 steps a second = 20 ms a step on the main CPU, whatever the fps**. The fps is set by the picture side: with 2 steps per shown frame, 25 fps needs the picture in 40 ms, 20 fps in 50 ms.
- The GE needs about 39 ms for a deck frame on its own, 42-45 with the CPU busy [C, before the rain was halved]. It is close to the 25 fps limit.

| Rainy deck, ms a step [C gl17, effects off] | Today | Side after the ME split | Target |
|---|---|---|---|
| Game logic | 41.7 (about 35 in gl18 with the rain's skips [I]) | main | |
| PS2 draw pipeline (game code) | 28.5 | main | |
| IOP + sound | 2.4 | main | |
| **Main CPU** | **72.6** | | **20** |
| GS walk + translator | 32.2 | ME | |
| Picture (GE lists, textures) | 43.3 | ME | |
| **Picture side, a shown frame (2 steps)** | **151** | | **40 (25 fps) / 50 (20 fps)** |

## 2. Decisions I take now (each goes in DEVIATIONS.md with its impact)

1. **VU0 math stays on the VFPU (deviation E1 accepted).** "Everything exact" would cost about 9 ms a deck step [I], half the whole budget. The difference is the last bit of vector math; nobody can see it in play. gl18e1 is not shipped. The logic thread's PCSX2 work (#177, #183) leaves the critical path.
2. **Game-side cuts are allowed.** Cosmetic actors may update every other step and the rain's logic may be thinned. They share the stage's random seed with the guards, so the guards' later random choices change: a gameplay deviation, logged. The x86 twin with the same cuts stays our bug detector (equal hashes on the PC, the emulator and the PSP).
3. **The native renderer may differ from the PS2 picture in detail** (GE lighting and skinning, rounding, simpler effects). The rule: no missing or misplaced objects, the same look at a glance, shots in your SHOTS folder for every new kind.
4. **The picture goes to the Media Engine by default** once gl18me's run shows it works on your PSP.
5. **16-bit colour buffers (5650) are tried** to free 544 KB of VRAM for textures and cut GE traffic. FF8 showed 16-bit screens cost nothing and you couldn't tell the difference [C]. MGS2's blur samples the frame, so it's measured and shot first.
6. **Builds may time several changes at once in turns** (switches swapped every 100 vblanks in one run), as before; one switch per turn pair.

## 3. The phases

### Phase 0, now (no PSP build)
- Rebuild the whole build tree as one folder in the project (`mgs2/main/`) with one build script, so a container reset never costs a day again. In progress.
- Add the clock proof line to the harness (FF8's timed loop; it showed 329-331 MHz on your PSP). MGS2 only logs what the firmware reports today.
- Read gl18me's run when it lands in LOGS\gl18me. It tells whether the ME draws as fast as the main CPU and what the bus costs. Everything below is sized on that.

### Phase 1: gl19, the cheap main-CPU cuts (after gl18me's log is read)
In turns, each measured on your PSP [I, deck_costs.md §2.1]:
- ropes every other step (about -2.2 ms a step), lamps every other step (-1.5), the sea every other step (-0.6);
- the rain's logic thinned to half (-1.5 to -2);
- the trail off (-0.5 logic, -1.2 picture; you OKed removing it in October);
- a fresh PGO profile with a check that no function lost its profile (w00a has 287 that did), and FF8's placement method tried against ours (FF8: I-misses 42 k to 23 k a frame, -3.5 to -4.7 ms [C model, FF8]);
- the glows and smoke effects on (your card B4), the ME picture on.
Expected [I]: main CPU about 60 ms a step. The fps barely moves yet, because the picture side is the limit until Phase 2.

### Phase 2: the native deck renderer on the Media Engine (the big one)
- **Design done 2026-11-20 (step 2.0): `native_renderer_design.md`.** Seam at the leaf builders (stubs write records; the ME converts them with today's converters); first PSP step = native ship meshes; rain stays main-CPU-limited, so Phase 3 decides the rain's speed.
- Step 2.0, design: what the game reads back from its draw pipeline (1,716 words on our routes [C, speed_notes §31]: the view data, the packet pointers, some object and bone matrices) and the 24,473 words a pipeline call reads from the call before (mostly the per-object transforms and bone matrices). So the main CPU keeps the pipeline's transforms (the game reads them, and the new renderer reads them too) and drops every packet builder, as the reduced pipeline already does on unshown steps (about -12.8 model ms a reduced step [C model, §33]). The box cull's micro program moves to the VFPU, now allowed with E1 accepted (about 4-5 ms a step [I]). Then what a snapshot of each frame's draw objects holds, per kind.
- Then one kind at a time, the most expensive first, today's path kept as the fallback for kinds not done yet: the people (Snake and guards, about 17 ms a step today [I]) with GE skinning and lighting; the ship's meshes (8-12); the effects in the game's sort order (splashes, glows, smoke); the HUD, radar, sea and the rest.
- When every deck kind is native: the PS2 pipeline, its packet builders and the GS walk stop on the deck.
Expected [I]: the picture side per shown frame from about 150 ms to 25-35; the main CPU from about 60 to 35-40 ms a step: **about 12-14 fps at about half speed**.

### Phase 3: the game's logic under 20 ms a step
- The code generator's remaining steps (values in registers, fewer write-backs), the machine pointer in a fixed register (FF8: -1.6 ms [C model, FF8]), data-cache placement (FF8 found up to 3-4 ms there).
- Render-only actors skip the unshown step (about -4 [I]); work owed and done only when read (H1, about -5 [I]).
- Native C for the hottest game functions: 30 functions carry half of the deck's game code time [C model]: the guards' brain (watcher), Snake (raiden), the ropes, putmodel and the like.
- The sound mix on the ME (-2.4).
Expected [I]: main CPU 18-25 ms a step: **20 fps at 80-100% speed; 25 fps where the step is under 20 ms**.

### Phase 4: a playable deck
- Your pad instead of the route file; a frame skip that never slows the game (show 1 of 3 steps when the picture is late); the sound on the ME; saves.
- If the rainy deck still misses 20 ms a step: lighter rain (fewer drops in the game), a calmer sea, a shorter draw distance. Each with shots first, chosen by me under the lead, told to you.

## 4. Risks and what would change the plan

- **gl18me's run** may show the ME slower than the main CPU, or the bus costing more than the probes said. Then the native renderer splits its work between the two CPUs.
- **The GE limit:** about 40 ms a deck frame. If the native renderer makes the CPU side fast, the GE becomes the wall at 25 fps. Fixes: 16-bit buffers, fewer overdraw layers, effects at lower resolution.
- **Main-CPU estimates** for Phase 3 have the widest range. The 30-function native work is the largest job after the renderer.
- **Coverage:** the rest of the game has never run on the PSP. The renderer is built for the deck's kinds first; other areas fall back to today's path until their kinds are added.

## 5. What I need from you

- Run gl18me (Builds\MGS2T_gl18me, on the charger) and put the files in LOGS\gl18me. That's all for now.
- Sources: lessons from FF8 and the project since 8 Nov (`scratchpad lessons_2026-11-18.md`, summarised into this plan), `deck_costs.md`, `halfrate_study.md`, `native_draw_study.md`, `roadmap.md`.

## DECISION 2026-11-23 20:4x UTC (lead): half-rate game sized, kept as the second track
- Sizing: halfrate_sizing_2026-11-23.md. Our PAL disc already carries Konami's 50 Hz conversion (floats x6/5 too) [C], so a 25 Hz
  game is a second conversion of the same sites at ratio 2. Deck ~900-1,200 patches, 5-9 weeks; whole game 4-9 months [I].
  First twin test (wait word 2 + 16 patches): Snake and camera follow the full-rate path closely; guards not yet [C twin].
- Gain [I, 2x]: after the renderer, rain 46-67 ms a picture (15-22 fps) vs 78-108 at full rate (9.3-12.8). Before the renderer it
  buys nothing (ME-bound ~7 fps).
- Order: renderer first (precondition; gl41 = ship stub + P1/P3 turns on gl40's tree after gl40's log). Twin step M2 in parallel
  (guards' timers, blends, camera, script waits, parity gates, collision). Go/no-go at the renderer checkpoint: near 46 ms -> half
  rate; at the high end -> the Vita-style lever (~5-6 ms a step) instead.
- Exactness: the half-rate game gets its own exact hash reference (patched image built once for twin, emulator and PSP); exactness
  against the original game is given up only if we go. Logged in DEVIATIONS.md before any build carries it.
