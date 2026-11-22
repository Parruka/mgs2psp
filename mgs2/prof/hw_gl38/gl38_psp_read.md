# gl38 PSP run 1: what it measured (read 2026-11-22 22:40 UTC)

Run: launched 21:51:34 UTC via RC (plugin off), end screen, PSPLink back ~22:23; LOGS\gl38 = 59 files (log.txt 1,199,221 B).
Reader: rd38pc.py on the PC -> LOGS\gl38\rd38pc_out.txt (45,218 B; full copy still to be staged here with log.txt when the
folder tools return). Clock proof 329 MHz (333 requested), bus 166. All figures ms a PSP step (a vblank) [C PSP] unless marked.

## Exact [C]
62/62 hashes (66499cdb584a) and 916/916 digests by kind (9ae227a44404 / dz 4c447e32a4ac) = emulator = twin. SCHEDULE CHECK none.
Problem counters 0 (nat none, re-use 0, F1 planned runs with a problem 0, rain owners not right 0; over the table 36,498 = first
deck scene, expected). Groups left untaken 524 / 423 / 2037 at 24,000 / 25,000 / 29,000 (= emulator). No crash record.
End: malloc can still give 2,640 KB (emulator 96 KB: the emulator's memory is not the PSP's). Battery 75 -> 89% (charging).

## Rain wait 29,500-30,600 (frame digests on): K0 n 4 = 84.1-85.9, mean 84.99 (5.9 fps, = gl36's 85.1)
- Main own: logic 42.78 + pipe 26.23 + GS walk 4.21 = 73.2; main waits for the ME 9.1. ME busy 84.4: walks 58 (people chain 2
  19.6, ship chain 3 16.9, chain 6 sorted+rain 15.2, chain 10 3.4), lists 22.4, records 3.7.
- NEW: the ME's lists 22.4 = FRAME DIGEST 13.6 + people 5.3 + ship 2.6 + effects 0.55 + rest 0.4. The digest is the test's own
  picture check (29,490-31,400 only): a play build does not run it. Without it the ME needs ~70.8 < main 73.2, so the rain wait
  would be MAIN-limited at ~73-75 ms a step (~6.7 fps) [I]. Measure it: a digest-off turn in the next build.
- Coupling: main logic 42.8 with the ME working vs 35.5 in K7 turns (ME resting): +7.3 [C]; steady deck +4.8, first deck +15.7,
  nothing drawn +7.0. Cause UNKNOWN (bus traffic of the ME? the digest reads every list).

| kind (vs K0 of the scene) | step | main logic | main waits | ME busy | model [C model] |
|---|---|---|---|---|---|
| K1 ship off | -9.2 | -0.6 | -8.4 | -24.2 | -29.4 |
| K2 people off | -11.7 | -1.5 | -10.1 | -32.9 | -39.1 |
| K3 sorted off (rain kept) R1 | -0.8 | -0.5 | -0.3 | -0.6 | -0.9 |
| K4 rain off but the near sheet | -6.4 | +0.1 | -6.7 | -7.5 | -9.4 |
| K5 stage program (chain 10) off | -3.6 | -0.6 | -3.5 | -3.9 | -5.1 |
| K6 ship+people+sorted+rain off | -16.1 | -6.4 | -8.9 | -76.4 | -92.7 |
| K7 nothing drawn (ME rests) | -15.4 | -7.3 | -8.8 | (all) | -102.6 |
| F1 on, re-use 0 | -1.6 | 0.0 | -1.5 | -1.8 | -2.2 |
| F1 on, re-use 1 | +1.5 | -0.1 | +1.5 | +1.1 | +1.7 (noise) |

- Model vs PSP for ME work (after the units fix): PSP = 0.77-0.88 x model (K1 0.82, K2 0.84, K4 0.79, K5 0.77, K6 0.82,
  steady K5 0.88). The model is now usable for ME levers at ~0.8x.
- Every ME cut stops paying once the ME falls under the main CPU's ~73-76: K1 leaves the ME at 60 but the step at 75.8.
- K3 R0 copy at 30,600 +5.0 is the heavier part of the scene (not a cost). The K0 turn-to-turn spread is 1.8, so F1's -1.6
  is indicative only; the register stage fell 8.6 -> 5.9 us a ship group with F1 (re-use 0).

## Rain walk 30,800-31,300 (n 1 each, indicative)
K0 82.47 (main own 75.4, ME 81.7 of which digests 14.2): K2 -10.3 (ME -53.2), K4 -1.0 (ME -8.5), K6 -14.2, K7 -15.8 -> 66.7.

## Steady deck 26,200-28,700 (d01t cutscene, no digests): K0 n 11 = 41.46 (12.1 fps), ME-limited
- Main own 22.1 (logic 11.6, pipe 6.3, GS walk 4.2), main waits 16.9; ME 41.5 (walks 34.6: chain 3 8.8, chain 10 8.1, frame
  ends 9.9, chain 14 3.5; lists 4.3 = people 4.1; records 2.5).
- K5 (chain 10 off) n 7: 26.38 (19.0 fps): step -15.1, ME -15.6. K7 n 7: 18.94 (main alone 26.4 fps).
- WHAT chain 10 draws in this scene is UNKNOWN (chain numbers are places in the frame's list): look at a K5 picture of the
  cutscene on the emulator before any proposal. A cut there is a visible change: pictures to Ffm first.

## First deck scene 24,000-25,000: K0 turns 87-97 apart, kinds indicative only (K3/K4 over the rain table).

## What it changes
- The rain wait's true play speed is main-limited, not ME-limited [I, to measure]: gl39 = main CPU (logic 42.8 / 35.5 alone,
  pipe 26.2 constant in every kind = the game's own drawing preparation, GS walk 4.2) + a digest-off turn + the gl38 main-turn
  data-placement guard failure fixed or gl36's placement kept. Re-check the coupling with digests off.
- K4 rain cut as a default: not now. In a play build (no digest) the ME is no longer the limit in the rain, so it would gain
  ~0 until the main CPU is faster. MGS2_GL_RAIN_CUT stays ready (needs a larger owners' table for the first deck scene).
- Cutscene: chain 10 is the cheapest visible lever measured: 12 -> 19 fps [C]. Identify it, then pictures + card.
