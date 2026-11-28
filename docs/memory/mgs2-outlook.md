---
name: mgs2-outlook
description: What frame rates the MGS2 PSP port can reach (25 fps outlook, the corrected estimates, step budgets, memory budget) and what Ffm was told and when
metadata:
  type: project
  modified: 2026-11-23T15:56:00.000Z
---

- FULL NATIVE? (Ffm 2026-11-19 15:34 "would we gain fps? possible? idc how long"; answered 15:5x, mgs2/native_port_answer.md): yes, but the gain is mostly the native renderer (Phase 2): light deck ~20-25 fps after it, ~25 plausible with native C for the hot ~200 functions; rain 11-20 fps under ANY approach unless lighter content [I]. Full rewrite not recommended even with unlimited time (13,500 functions, no source). Promised: A/B of 5-10 hot functions native vs recompiled, after the renderer.
- CURRENT (2026-11-01; native_draw_study.md §5, mgs2/roadmap.md "The road to 25 fps"): gl12 PSP deck 3.3 fps (153.4 ms a step), inside 5.2, calls 7.5-7.7. End state [I]: Stage C rebuilt renderer + logic work deck 9-14.6, inside 13.7-22.6, calls 24-25; the ME picture idea (not sized) might give deck ~12-18, inside ~18-25. The deck needs lighter content for 25 and for 20 (game code ~28-40 ms a step after logic work; GE ~40 ms a deck frame): Ffm's call later with shots. Ffm's goal (21:47): full game, 25 fps, at least 20, 30 a bonus. The lines below are older history.
- AT ANY COST (Ffm 16:00): game code ~1.5x, ME runs effect actors, no exact maths/address checks, lighter content; half-rate game (one step a picture, timers/animation/physics rescaled, proof gone, months) = the only route to 20 in the rain. mgs2/halfrate_study.md (wait word 0x18db74).
- (Older estimates 2026-10-20/21 superseded.) 25 FPS OUTLOOK (Ffm 2026-10-24 11:44 "impossible? should we stop?"; answered 11:58, mgs2/outlook_25fps.md): frame rate = game speed (no time step). 25 fps everywhere is not realistic [I]; lighter areas likely (crew deck scripted 17.2 ms logic + IOP against ~15 needed); rainy deck free play ~13-18.5 fps after all levers [I] (lean code generator, native effects, native guards + player, sound on the ME), lighter rain 14.5-20 [I]; near 25 also needs the renderer's CPU work halved. Recommended: keep going. Deck free-play logic (model): effects 23.2 of 48 ms, gameplay 22.1 (3 guards 7.2), wait 2.7.
- Budgets: ~12 ms of logic per step at 25 fps (2 steps a frame, ~20 ms per step in all); the GE frame must fit 40 ms with the CPU busy. x86 profile (2026-10-19): ~80% of the time in the PS2 draw pipeline (DG_ActLast, mostly VU0 micro code).
- Memory (2026-10-21): 51 MB user = 32 MB PS2 RAM, ~4.5 MB stage code, main code a few MB, 1 MB gfx buffer, ~2.2 MB stacks; the volatile 4 MB holds the SPU2 RAM (2 MB) and the picture's frame memory (2 MB). The full -O2 code does not fit (audit H1): hot set per stage + an exact interpreter for the rest [[mgs2-audit]].
- Vertices in RAM vs VRAM (Ffm 2026-10-21 00:56): re-measure as a GE replay mode once VRAM gets tight.
