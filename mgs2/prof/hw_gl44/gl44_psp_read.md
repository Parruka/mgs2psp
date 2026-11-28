# gl44 on the PSP: V44 (rain stubs) + the black box (2026-11-28)

Build: build_gl44/deliver (EBOOT 4b796e12, cfg 1cf3d930, hashref b6a845bf; harness "Build gl44"; DEVIATIONS E5 "V44"). Figures are the
reader's (rd44pc.py 3a879875, rdbbox.py 7dcebb9b) as reported from the PC (LOGS\gl44, LOGS\gl44\r2). All [C] PSP unless marked.
Files here: gl44_r1_psp_read.md (the stop, raw black-box records), gl44_r2_turns.txt (r2's TURNS table).

## r1 (09:40 UTC): STOPPED at vb 17,843 (the random freeze, on the AC adapter). See gl44_r1_psp_read.md.

## r2 (10:06:54 UTC, same files, plugin OFF, AC adapter from t 38 s): RAN TO THE END, EXACT
- 'Press X to exit.', 6,486 lines. Hashes 62/62 (4dea1824afdf), digests 61/61 (6bf2bb9bfd5c / dz 5bb8f80b7648), content 61/61
  (6b657c5fbaf2). Schedule check none. Problem counters none / 0; XP check 720 plans compared, 0 differing.
- H1 proven on the PSP: log_prev1.txt, battery_prev1.txt, bbox_prev1.txt, crash_prev1.txt byte-equal to r1's copies in LOGS\gl44.
- Black box: 2,343 records over 2,364 s, "EXITED IN ORDER"; write mean 18.64 ms, max 154; wake lateness mean 0.01, max 5 ms.
- Heap (pclink start): texture table at vb 536 with 2,631 + 34 = 2,666 KB of newlib room; lowest room + top 2,654 KB at vb 23,676;
  textures refused 0. Outside the heap at the end: largest block 24 KB, total 40 KB.
- V44's counters = the emulator's: stubbed 65,600 + 16,400 = 82,000; by G0 76,506 + 24,600; rain.c skipped 12,000 + 3,000; in place
  and run 13,995 + 4,500. Option B's: 0 polls due, 0 guards.

## V44, ms a PSP step (columns step, main, ME | logic, pipeline)
| pair | step | main | ME | logic | pipeline |
|---|---|---|---|---|---|
| K-N2BV - K-N2B, rain wait, means (3 - 4 turns) | -1.29 | -1.26 | +0.02 | -1.52 | +0.20 |
| t58 vs t57 t59 | -1.22 | -1.15 | +0.19 | -1.63 | +0.30 |
| t60 vs t59 t61 | -1.28 | -1.26 | -0.08 | -1.34 | +0.04 |
| t62 vs t61 t63 | -1.15 | -1.21 | +0.12 | -1.55 | +0.33 |
| K7BV - K7B (t65 - t64, t66 - t67) | -3.71, -1.02 | -3.65, -1.02 | - | -2.89, -0.53 | -0.77, -0.41 |
| K-BV - K-B (t52 - t51, t53 - t54) | -2.18, +0.33 | -2.03, +0.16 | | | no reading, as the README said |
| walk: t69 vs t68 t70, t71 vs t70 t72 | -4.86, -2.81 | -4.76, -2.80 | -8.21, -1.94 | -2.83, -2.29 | -1.53, -0.39 |
- Against the estimate (README: K-N2BV - K-N2B about -1.1 on the PSP [I], model -1.32): MET, -1.2 to -1.3. The gain is all logic
  (line_sph's and rain.c's hidden work); the pipeline column moves +0.2 (noise or placement). K7 pairs and the walk are noisy (the
  scene drifts turn to turn there).
- Absolute, rain wait: K-N2BV 69.32 (main 69.09, ME 53.80; logic 37.41, pipeline 25.32) = 7.2 fps. K-N2B 70.61 (main 70.35; logic
  38.93, pipeline 25.13). K-N2 75.67 (one turn). K7B 65.93, K7BV 63.56.
- Option B again: K-N2B - K-N2 -5.06 (gl43: -4.75).

## The black box's cost [I]
gl44 minus gl43 on the same kinds, rain wait: K-N2B 70.61 against 69.70 = +0.91; K-N2 75.67 against about 74.45 = +1.2. The model gave
+0.09 for the code alone, so about +0.8 to +1.1 ms a rain step is the black box: 18.6 ms of writing each second at about 14 steps a
second is 1.3 ms a step if it were all CPU. For a play build: one record every few seconds, or the file kept open, or off.

## The deck without rain, r2 (gl44_r2_turns.txt)
- Steady deck: K-A 40.3 (ME-bound: ME 40.4, main 24.5), K-N2R 22.2-22.5 = 22 fps (main 22.2, ME 17.3), K7 20.5.
- Deck, first play: K-N2R 37.0 = 13.5 fps (main-bound: logic 19.2, pipeline 12.3), K-A 36.3-41.8, K7 35.7.
- First deck scene (cutscene): K-N2R 72-77, K-A 85-105.
- Before vb 24,000 (alternating 100-vb turns): main 57.98, ME 55.31 mean of all windows; 20,000-24,000: 87.4 / 86.4 (logic 45.0 / 35.9,
  pipeline 34.2 / 26.3).

## Reading
- V44 is worth what the model said. The rain is still main-bound: logic 37.4 + pipeline 25.3 + about 6. gl45's cuts aim at the
  pipeline (about -6.3 [I]); the logic stays, so the half-rate game remains the rain's route to 20 fps.
- The stops: 1 of 2 gl44 runs; AC adapter tally: gl43 passed, gl44 r1 stopped, gl44 r2 passed.
