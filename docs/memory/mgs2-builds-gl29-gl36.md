---
name: mgs2-builds-gl29-gl36
description: MGS2 PSP builds gl29-gl36 (2026-11-20..22): what each changed, PSP results [C], model/PSP ratios, lessons; full old index lines in mgs2/memory_status_archive_2026-11-22.md
metadata:
  type: project
  modified: 2026-11-22T12:13:28.759Z
---
All exact on the PSP (62/62 hashes, 916/916 list digests). Entries per build: mgs2/psp_harness.md "Build glNN"; trees /home/claude/tNN, packages build_glNN/deliver, PC Builds\MGS2T_glNN + LOGS\glNN.
- gl29/gl29x (2026-11-20): deck marks = real GE depth test drops the 0x42 shadow decal; X (decal on its floor's own triangles) restores it ([[mgs2-deck-marks]]). Both runs on battery power-stopped near vb 31,000 (battery stops 5 in 14: test on USB power, plugin on).
- gl30: subset pairing (fallbacks 0), X-O 2.29 ms [C model]; PSP exact, no hole patch. Ffm saw a flick (flip wait off in turns) -> FLIPWAIT_FROM=0.
- gl31: native ship step 1 + X whole run (DEVIATIONS B6) + flip wait always: flick gone; native -2.8 ME / -3.5 main a step [C] (model -6.0/-3.9). gl32 (vertex cache) not shipped (~1%).
- gl33: fresh PGO + order: rain ME turns -8.1 ms a step, main -11.1, steady deck 43.4 -> 40.7 [C]. Lesson: PGO goes stale fast; retrain every build that adds code.
- gl34 (measurement): ME timers per chain; native ship ~63-70 us a group, ~2/3 fixed per-group setup, ~30,500 groups (~18 vtx) a rain turn.
- gl35: fast front end + per-group state keeps in turns: keeps only -2.5 of 61 us a group (drawn areas pays, texture keep costs); steady deck ME 40.0 (= 12.5 fps at half speed; I wrote "25 fps" then: wrong, corrected to Ffm 2026-11-22); rain ME 85-88.
- Ship "roll" (Ffm 2026-11-21 23:39) = camera BOB (camerad.c act 0x10b720, two waves on eye.y/target.y from vb 29,067); game logic reads it -> stays exact; D3 (exact camera-tail replay, ~-0.5..-2 ME [I]) deferred (attic/roll/roll_design.md).
- Rain (Ffm asked a cheaper rain + a separate dice): mgs2/rain_options_2026-11-21.md; option A (falling splashes off) NOT invisible (opening spray mist) -> exact EARLY CULL v3 (attic/cull/cull_v3.md: box bound, 14x margin under PSP round-toward-zero, 28M fuzz 0 false). Exact dice form needs a new hashref (my "hashes stay" was wrong; told Ffm 2026-11-22). Visible options (fewer floor splashes/rings ~5-8 ME [C model]): show Ffm shots first (promised).
- gl36 (delivered 2026-11-22 11:15, measurement): cull in turns (t%8<4) + ship run re-use in turns (t%4<2) + keep bit 1 + boot heap fixes A/B + retrain. Needs MGS2_OPT_CYCLE in cfg or cycles are ignored. PSP RUN 1 (read 12:15) [C]: rain wait both on 85.1 ms a step (gl35 88.6); cull -2.7 (model -4.8: 0.55x); re-use alone -1.6 ME (-5.4 us of 58.6 a group; model -3.1: 0.5x) but +0.8..+1.6 in the walk (replays 7-27% there vs 87% in the wait, recording costs ~5-7 us a group); base +0.2 (model +4.0: the ME's own D-cache, data placement does not carry over); d01t main turns cull -6.7; steady deck 40 = gl35. prof/hw_gl36/natsplit_summary.md.
- Where the rain wait stands after gl36 (ME turn, a step): ME walks people 18.9, ship 17.4, chain 6 15.7 (58.6), ME total ~83; main logic 42.7 + waits 9.1. 85 ms a step = 5.9 fps at 24% speed. 20 fps near full speed needs ~25 ms a step on BOTH processors: ME ~-58, main ~-27 (3.4x); I first told Ffm "-35 ms for 20 fps" (wrong units, corrected 2026-11-22).
- Model vs PSP: the ~0.5x was mostly UNITS (model ms a picture vs PSP ms a vblank step); halve main-side model figures too (rain logic 6.9 model = ~3.5 a PSP step).
