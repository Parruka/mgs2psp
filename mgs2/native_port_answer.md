# Would a fully native PSP version of MGS2 be faster? Is it possible? (2026-11-19)

Ffm asked on 2026-11-19 at 15:34: "if we would create the full game natively to psp, would we gain fps? would it be possible? idc about how long it would take".
The same question was answered on 2026-11-01 in native_draw_study.md §9. This note redoes the answer with the gl23 PSP figures.
Marks: [C] measured, [C model] from the cost model, [I] inferred, [S] what a source says.

## Short answer
- **Yes, a native version would gain fps.** Most of that gain comes from making the renderer native, which is already Phase 2 of plan_deck_25fps.md. Very little of it needs the whole game rewritten.
- **A full rewrite is possible in principle but not realistic, even with unlimited time:**
  - about 13,500 game functions, with no source;
  - without the recompiled game as a reference, we could not prove the game still behaves the same;
  - its gain over the hybrid path below is small (about 10-25% [I]).
- **Recommended: the hybrid path.** Keep the recompiled game, which the hash test proves exact. Replace the expensive parts with native code one at a time, checking each against the hashes.

## Where the time goes now (gl23 PSP, deck, per vblank step; 25 fps needs 20 ms or less) [C]

| Part, ms a step | Light deck | Rain stretch |
|---|---|---|
| Whole step | 49.0 | 117.2 |
| Game logic | 11.9 | 52.7 (41.7 when the main CPU runs alone) |
| PS2 draw pipeline | 6.9 | 29.9 |
| Copy plans for the ME | 4.0 | 5.4 |
| IOP and sound | 2.4 | 2.6 |
| Main CPU waiting for the ME's walk | 23.7 | 26.4 |
| The ME's walk itself (PS2 graphics emulation) | 33.4 | 59-65 |
| GE drawing per frame (overlapped; not the wall) | 22-27 | 25 |

Source: /home/claude/runs/gl23/log.txt windows; psp_harness.md, entries gl21-gl23.

## Gain by layer [I unless marked]

**Native renderer.** The GE draws straight from the game's objects, using the GE's own skinning and lighting. That replaces walking the PS2's VU1 and GS packets.
- It removes the walk, the copy plans and the packet builders.
- Light deck: the main CPU's own work goes from about 26 to about 17-21 ms a step, which is about 20-25 fps.
- Rain: about 50-70 ms a step.
- gl24's overlap removes the same wait, so its gain must not be added on top of this one.

**Native C for the hot game code.** Faithful native C would remove these overheads [C model, speed_notes §48-50]:
- emulated 64- and 128-bit registers;
- 63-instruction guest calls;
- the address check;
- code that is 6.5-7 times larger than the PS2's.

Native C might run in 0.5-0.7 of today's logic time. That gives the light deck about 6-9 ms of logic and the rain stretch about 21-37.

The hot code is concentrated. On the deck, 200 of about 930 running functions carry 90% of the game code's time, and 30 carry 50% [C model, native_draw_study §5].

**Lighter content.** Less rain, a simpler sea, effects every other step: about -5 to -15 ms a step on the rain stretch.

**End state, all layers:**
- light deck: about 13-17 ms a step, so 25 fps is plausible;
- rain: about 25-45 ms a step, which is 11-20 fps.

25 fps in the rain needs lighter content under any approach, a full rewrite included.

## Why not rewrite the whole game

- **Size [C]:** about 13,500 distinct game functions, about 5.9 MB of PS2 code over 123 stages, plus the main program's 2,303 functions.
- **No usable source:**
  - The public decompile, mgs2old, is an early one of the US Substance build.
- **Hardware-bound code to redo:**
  - VU0 code inside the game logic (863 COP2 macro sites, 45 micro programs whose results reach the game state);
  - VU1 programs;
  - the IOP sound and streaming driver;
  - the IPU movies;
  - the GS-only effects.
- **Exactness:** a rewrite drifts in float rounding and in the order of random draws. The guards and the effects share one random seed. Bugs would show hours into play, with no exact build to compare against.
- **Nothing playable** until most of the game is done.
- **PSP limits that stay whatever the approach:**
  - 20 ms of main CPU a step;
  - the GE shares RAM with the CPU (the CPU kept 75-83% of its speed during a 39 ms GE frame [C]);
  - 2 MB of VRAM (688 KB left for textures with 32-bit screens);
  - the 16 KB instruction cache.
- **Precedent [S]:**
  - No PS2-to-PSP 3D port, official or fan-made, kept the original content at full speed.
  - Peace Walker reaches 30 fps because its engine was built for the PSP.

## Path (each step is checked by the hash test on x86, the emulator and the PSP)

1. gl24 (now): the walk overlaps the next step's logic, about -20% on the light deck [I].
2. Native renderer, one draw kind at a time. Today's renderer stays as the fallback and the picture reference.
3. Native C for the hottest functions, starting with the 30 that carry half the deck's game time.
4. Lighter rain and effects, each logged in DEVIATIONS.md.

## How to settle the numbers

- **The native-C factor:** decompile 5-10 hot deck functions (rain, line_sph, the box culls). Time them against the recompiled ones in turns in one PSP run, with equal hashes. This also gives the code-size ratio.
- **Rain logic at 52.7 ms with the ME on, against 41.7 with the main CPU alone:** time the same windows with the ME walking, idle and halted.
- **The native renderer's real cost:** build one draw kind (opaque meshes or characters) and read the ME-side timers.
