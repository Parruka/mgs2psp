---
name: vita-port-study
description: 2026-11-27 study of how the PS Vita MGS2 cut costs (reference Vita code paths + public pages); files, key facts, ideas for the PSP deck
metadata:
  type: project
  modified: 2026-11-27T18:58:10.259Z
---



- Vita did NOT simplify logic: 60 steps/s kept, AI/physics untouched. Savings = threads (game thread: joint matrices + snapshot of draw queues; second thread: cull, lights, chains, shadows on copies, nothing written back; renderer thread; job workers) + a picture every other step + ~225 effects at half rate (13 kept full).
- Deck w00a (always 30 on Vita): rain.c, line_sph, rain_cm, ripple, charaspread update only on drawn steps, no time correction; body_sph skips its vertex work on unshown steps. NOT touched: d_splash_floor, d_splash_fall, d_splash_camera, rain_gas_pers, ropes, lamps, w00a sea. Guards' dice move (accepted by them).
- Holds w04a/b/c: snapshot off; up to 128 marines updated in worker jobs of 10; draw commands built once at load, merged by model, no bounding test.
- Public: armature.com/games/mgshd ("pipelined the renderer and multi-threaded the game"); reviews: MGS2 flips 60/30, below native resolution; no effect reported removed.

Ideas for the PSP deck (sizes [I] from the cost model, not measured): the Vita's rain list on shown steps only, about -1.8 ms a step in the rain wait, stacks with gl43's B; rain.c as an every-step stub (streaks never drawn in our build), up to 1.2; draw prep to the ME by the Vita's snapshot seam, and the cull may leave the main CPU (check by read hash). No precedent for cheaper game logic.


Ffm 2026-11-27 19:00 asked "can we make the game logic cheaper/lighter? or is completely impossible?". Answered 19:0x: three ways (native hot functions ~1.5x [I]; effects thinned ~7-10 ms a rain step [I]; half-rate game, 6-10 weeks deck / 4-9 months game, proof lost). Recommended: finish the effect cuts and test native rewrites on a few functions first; decide on half rate only after those are measured on the PSP. Ffm reacted with a thumbs-up at 19:03 (an acknowledgement, not a card decision).
2026-11-27 20:05: the port lead asked this thread for the gl44 design of the rain list; delivered as exact every-step stubs for line_sph and rain.c: [[mgs2-gl44-rain-stubs]].
