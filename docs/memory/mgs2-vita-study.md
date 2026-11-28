---
name: mgs2-vita-study
metadata:
  type: reference
  modified: 2026-11-27T18:58:11.881Z
---

- Logic never simplified on the Vita: 60 steps/s kept; no precedent for cheaper AI/physics.
- RAIN on unshown steps: rain.c, line_sph, rain_cm, ripple, charaspread update only on DRAWN steps (no time correction); body_sph returns before its vertex work. Our model: ~-1.8 ms a step in the rain wait [I] if done on shown steps only (dice kept by the seed jump); stacks with gl43's cut B. rain.c's streaks are never drawn in our build: candidate every-step stub (up to 1.2 ms [I]). Not touched on Vita: d_splash_floor/fall/camera, rain_gas_pers, ropes, lamps, the w00a sea.
- Konami's 2003 PC build thinned rain at CREATION (script values 'n' units for rain.c, 'd' drops per unit for line_sph; rain_gas_pers, rain_cm, d_splash_fall empty at level 0) = the canonical place for a data-level cut. [[mgs2-rain-study]]
- DRAW PREP (our indoor 13.3 ms a step on the main CPU, K7 2026-11-27): the Vita game thread only makes joint/world matrices + a snapshot (set header, per-model entries, root, 2 light matrices, packet descriptors; no vertex data); a second thread does bounding test + cull, fog, lights, chain building, shadows on the copies; nothing is written back to game objects. Lead for us: the cull and chain building can leave the main CPU (check with the read hash that no game code reads a visibility result). [[mgs2-native-renderer]]
- Holds w04a/b/c (later): 128 marines updated in jobs of 10; draw commands built once at load, merged by model, no bounding test.
