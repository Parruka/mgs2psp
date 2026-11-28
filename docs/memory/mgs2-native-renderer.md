---
name: mgs2-native-renderer
description: MGS2 Phase 2 native deck renderer on the ME: design result (2026-11-20, Fable 5.2), seam, build order, expected fps, step 0 and step 1 (gl30n) status
metadata:
  type: project
  modified: 2026-11-21T17:06:00.000Z
---

- Design: mgs2/native_renderer_design.md (Fable 5.2 agent at high, 2026-11-20).
- Seam [C x86 twin]: stub the LEAF builders, keep the game's pass code as the traversal. 0x1214a0 (ship meshes) and 0x12e8b8 (people) are clean stubs (V1/V3a: same reads on the deck). 0x12d910 (effects) and 0x13bf78 (sea) need their real return values (the sea's plain return flips flag 0x1b4818 bit 22). Whole-pass cuts (H1 set) are NOT clean.
- Records = the five pointers + header words lane 1 parses -> the ME feeds the SAME converters (xconv.c) -> batch-identical to today; gelist.c unchanged; HUD/radar/post stay on the walk; new queue entry XE_SNAP, ME reads game RAM only before the hook that ends the step (hold).
- Build order: step 0 container (shadow compare, order key, read-hash) -> PSP 1 ship meshes -> 2 people (GE skinning/lighting, C2) -> 3 effects -> 4 vertex cache -> 5 picture every step -> 6 faces/sea -> 7 copy plans off -> 8 GE levers. Decisions D1-D6 in design §6.
- Expected [I, 2x error]: rain after Phase 2 main 62-70 ms a step, ME 25-45 a picture, GE 28-31 a frame -> 13-15 fps at ~30% game speed (main CPU logic 49 ms/step = the wall -> Phase 3); quiet deck (d01t) ~23-25 fps near full speed. Today rain 5.7 fps [C gl28].
- Corrections: ME eDRAM is 4 MB; the "steady deck" 26,300-28,700 is the d01t demo stage (faces only).
- Decal rule from gl29 (section 9): decal drawn with its base's triangles; bias only for layers with own geometry.
- STEP 0 DONE 2026-11-20 17:50 [C x86 twin]: native ship-mesh batches = lane 1's in all 17,664 pictures (0 of 1,094,606 df0 batches differ); order provable (key = SPR 0xc70 + 16*SPR 0xc64, 0 inversions); on mode (builder stubbed) needs a LANE STOP in lanes 3/4 at the next key (also in xlme.c); records 32 B a call + 32 B a group (rain ~72 KB a picture, loads up to 303 KB -> per-call fallback); stub 0x12d910 returns 0; HUD depth noise in 92 rain pictures only. Report: mgs2/native_step0_report.md; patch mgs2/main/attic/native0/.
- STEP 1 BUILT 2026-11-20 (gl30n; report mgs2/native_step1_report.md): ship meshes from 48-B group records (2,304 a packet buffer); MGS2_NAT_CYCLE arms 0 builder / 3 native / 4 stubbed+not drawn (arm 4 hides the ship: tell Ffm first). OPEN: w01a stub turns change w-lane reads (classify before a build runs w01a with the stub on).
- STEP 1 ON THE PSP (gl31, read 2026-11-20 23:45) [C]: exact; rain wait ME turns -2.8 ms a step, main -3.5 (model -6.0/-3.9). ME there: walks 73-76 (native part 23-24), list build 22.5-23.3, idle 1.1 a step: ME-limited. gl32 (vertex cache) not shipped: converter only 4.7 of the ship's 74.6 ms ME work a picture (copy 14.4, front end 13.6, call+tracking 15.7, per group 11.3).
- gl33 ON THE PSP (read 2026-11-21 11:20) [C]: fresh PGO + order = rain ME turns -8.1 ms a step (101 -> 92.8), main -11.1, steady deck ME 43.4 -> 40.7; the in-place static-stream rule only ~-1.5 a step [I] vs model -5.8 (the model overrates ME savings on copies/D-misses). Rain ME now: walks ~65.9 (native ~21.9), lists ~22.3, main waits for fills ~13.5 a step.
- gl34 ON THE PSP (read 2026-11-21 17:05) [C timers]: native ship ~63-70 us a GROUP, ~2/3 fixed per-group setup (register 9, texture 8, walk 8, A+D 6, GS state 6, drawn areas 5 us), groups ~18 vertices, ~30,500 a rain turn; fast front end only -9%. Rain ME walk by chain: people ~19.5, ship ~22, sorted prims+meshes ~18.7 ms a step. Next: cut per-group fixed work (keep state across groups, merge groups), then people.
- gl35/gl36 ON THE PSP (2026-11-21/22) and where the rain stands now: [[mgs2-builds-gl29-gl36]]. Next native lever: per-group fixed work (~43 of ~58 us a ship group), then people (chain 2).
