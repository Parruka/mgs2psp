# gl39 PSP run 1: read-out (2026-11-23, lead)

Run: launched 12:48:22 UTC, ended before 13:36 (Ffm's "psplink up"), fetched to LOGS\gl39 (59 files; log.txt 4425 lines,
md5 f8773bc7ab73bf1f2393c27afdc0bb2c; rd39pc_out.txt md5 a9648902b445f84055cc4e8a92c042f0, read in full from the PC session's
copy). Battery: on the charger (76% -> 96%). The log files themselves still to be staged here when the folder tools return.

## Exactness [C PSP]
- HASHES 62/62 = 66499cdb584a: EQUAL. DIGESTS 266/266: EQUAL. Schedule check: none wrong. Crash: none. Problem counters 0
  (groups left untaken as in the emulator run; 196 ME load entries not posted = the probe's loop behind, harmless).
- Memory at the end: malloc arena 8900 KB, 3 KB free + 2624 KB in one block.

## Rain wait, ms a PSP step (step | logic pipe gsw | ME busy)
| Kind | n | step | logic | pipe | gsw | ME busy |
|---|---|---|---|---|---|---|
| K0 digests off, F1 on (the play form) | 3 | 80.21 (78.16, 78.11, 84.36) | 45.15 | 27.04 | 4.66 | 70.62 |
| K0 digests off, F1 off | 1 | 78.65 | 44.30 | 26.74 | 4.47 | 69.65 |
| K0 digests on, F1 on | 1 | 84.59 | 43.50 | 26.53 | 4.28 | 84.56 |
| K0 digests on, F1 off (gl38's form) | 1 | 84.73 | 44.36 | 26.67 | 4.29 | 85.56 |
| K7 nothing drawn, ME rests | 2 | 67.06 | 34.43 | 25.77 | 4.01 | - |
| K8 nothing drawn, ME turn, no load | 1 | 66.72 | 34.35 | 25.77 | 4.03 | 0.03 |
| K8 + ME arithmetic | 1 | 67.58 | 34.44 | 25.56 | 3.99 | load 50 |
| K8 + ME cached reads | 1 | 70.63 | 36.83 | 26.07 | 4.02 | load 50 |
| K8 + ME uncached writes | 1 | 69.25 | 35.21 | 26.13 | 4.00 | load 50 |

- PLAY FORM: 78.1 in two of three turns, 84.4 in the third (t66 at 30,600: logic +3.7, ME +5.9; the scene, not the
  switch) -> about 6.0-6.4 pictures a second [C]. Estimate was 76 [I]: 2-4 over.
- THE DIGEST (test only): ME +14-15 ms busy, step +4.4 to +6.5. gl38's 84.99 -> play form ~78-80: the gain is the
  digest leaving, as expected.
- F1 [C]: ME -1.0 busy (69.65 off vs 68.6-68.7 on in the neighbouring turns); the step does not follow (main-limited).
- MAIN CPU AGAINST gl38 at the same vblanks [C]: K7 24,900 73.85 vs 73.81; 25,300 34.48 vs 34.52; 25,600 31.00 vs 30.91;
  30,700 70.70 vs 71.05; steady-deck K7 19.2 vs 19.0. No change within +-0.4: the model's +0.4 and the data-placement
  repair both leave no mark on the PSP; M7 retrain = nothing.

## The coupling, settled in part [C]
- The ME busy on its own memory slows the main CPU's logic by: arithmetic +0.0, cached reads +2.4, uncached writes +0.8
  (rain wait); about 0 in the steady deck and the nothing-drawn scene. So ME bus traffic of these kinds is NOT the
  +7..+11 ms seen when drawing.
- Probes: service calls in logic 12 a step = ~0.11 ms; whole-cache write-backs 7 a step = ~0.15 ms; GE polls ~7.
  The "mq_service polling" guess is WRONG: all of it ~0.3 ms a step.
- What drawing costs the main CPU in the rain wait: K0 (digest off) minus K7 = step +13.15, logic +10.72, pipe +1.28,
  gsw +0.65 (first turns +13.9 / +11.7; walk +6.1 / +5.95). The model (gl38 K0 - K7, prof_rain_k0_minus_k7.txt) gives the
  main CPU's own drawing work as ~+2.8 ms (gif_qw +2.57, mq_wait +0.34, mp_mark +0.35, mg_lines +0.30, mq_service +0.27).
  So ~8 ms a step of logic in drawing turns is still UNKNOWN. Candidates: (a) waits inside logic that no timer covers
  (queue full / mq_wait, the record copies), (b) bus traffic of the GE (lists, textures) plus the ME's real pattern
  (reads of records in main RAM, writes of lists), which the synthetic loads did not imitate, (c) cache effects of the
  shared records. Next build must time mq_wait and the record copies inside logic and add a kind with the GE idle.

## Steady deck (codec screen, 26,200-28,700) [C]
- K0 41.52 (12.0 fps), ME-limited (ME busy 41.51); K5 (stage program = Snake's face off) 26.34; K7 main alone 19.21.
  K8 with a 25 ms load: step 34.6-34.9 = the load itself (ME-limited by design); logic unchanged.
- So on the codec screen the main CPU could give 26 fps; all the cost is the ME (the faces).

## Before the deck (main-drawing vs ME-drawing turns before vb 24,000)
- rd39pc.py run on the gl36 and gl38 logs too (prof/hw_gl36, prof/hw_gl38 log.txt), main-drawing turns [C]:
  | window | gl36 | gl38 | gl39 |
  |---|---|---|---|
  | all before 24,000 (n 120) | 55.56 | 56.31 | 56.63 |
  | 20,000-24,000 (n 20) | 84.87 | 86.45 | 86.94 |
  | tanker bow 15,700-16,100 | 49.05 | 50.57 | 50.31 |
  ME-drawing turns before 24,000: 55.52 / 55.64 / 55.82.
- So gl38's guard failure is real on the PSP: main-CPU turns outside the deck +0.75 (all) to +1.6 (20,000-24,000) over
  gl36, and gl39 another +0.3..+0.5 (the model's +0.4). The deck's rain wait did not move (gl36 = gl38 = 84.99 with the
  digest). gl39's pads and hot block did not bring gl36's placement back. Next build: train the order on the whole short
  route again, or take gl36's order for the code gl36 had.

## What it means
- The rain wait in the play form is main-CPU limited at ~78-80 ms a step: the game's own code with nothing drawn is ~67
  (logic 34, draw pipeline 26, GS walk 4), and drawing adds ~13 on the main CPU, of which ~8 is not yet explained.
- Levers on the main CPU that are known: H1 (cut more of the draw pipeline on unshown steps, exact proof unfinished since
  2026-11-08: ~5.5 PSP ms [I, old model]); the unexplained ~8 ms of drawing-time logic (once found); gif_qw (~2.6 model).
- 20 fps (25 ms a step) in the rain stays far: even all of these give ~55-60 [I].
