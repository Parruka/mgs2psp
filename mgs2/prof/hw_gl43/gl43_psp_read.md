# gl43 on the PSP: rain option B (2026-11-27)

Build: build_gl43/deliver (EBOOT 318caa49, cfg 533aa271, hashref 58cdec28; harness "Build gl43"; DEVIATIONS G2).  Launched 20:03:46 UTC
by pclink, plugin OFF, the AC adapter in.  Figures are the reader's (rd43pc.py 24875760) as reported from the PC
(LOGS\gl43\rd43_out.txt, 45,969 B; 59 files in LOGS\gl43; the files themselves are still to be copied here: OWED).  All [C] PSP.

## Result: RAN TO THE END ("Press X to exit", 6,157 log lines, about 30 min), EXACT
- Hashes 62/62 (8b9a91cd0765), digests 61/61 (2a30895d498d / dz 1f3203dfe825), content 61/61 (130349776315).  Schedule check none.
- POWER: the first run of this era on the AC adapter: "external power 1" in 362 of 453 battery lines, charging 1 in 254 (then full),
  100 %, 4,278 -> 4,272 mV.  One blip: "power event 000000e4: battery" at t 20 s (vb 89), back to "ac-power" at t 22 s (vb 116).
  Ffm 20:37: the blip was probably the USB cable being unplugged (the run then went on with the adapter alone).
  No stop.  (One run says nothing about the stops; the adapter test needs 5+ runs: freeze_study.md section 7.)

## Option B in the rain wait, ms a step (differences of means; columns step, main, ME, P | logic, pipeline, GE wait | ME walks, lists)
| pair (turns)                         | step  | main  | ME     | logic | pipeline | ME walks |
|--------------------------------------|-------|-------|--------|-------|----------|----------|
| K-B - K-A, digests off (1 - 4)       | -5.99 | -6.16 | -11.80 | -3.75 | -1.96    | -10.38   |
| K-N2B - K-N2, digests off (2 - 3)    | -4.75 | -4.67 | -10.21 | -2.90 | -1.55    | -9.08    |
| K7B - K7 (1 - 2): B's logic alone    | -4.81 | -4.66 | -      | -2.20 | -1.98    | -        |
| K-N2 - K-A, digests off (3 - 4)      | -2.40 | -2.53 | -5.97  | -1.62 | +0.12    | -5.39    |
- K-N2B absolute (2 turns): step 69.70, main 69.32, ME 55.25 | logic 38.37, pipeline 24.70, GE wait 0.38 | ME walks 47.82, lists 4.88.
  = 7.2 fps at 29 % game speed (K-A was 75.7 = 6.6 fps).
- Walk: K-B - K-A -6.52; K-N2B - K-N2 -2.14 (one pair each).
- The "digests none" class (t50-t54, before vb 29,490) reads K-B - K-A +4.66 (ME +15.07): NOT a B figure.  t50 is the turn in which the
  rain only begins, so its K-A mean is too low [I]; the README left t51 - t50 out for that reason.  Per-pair lines (PAIRS section) to be
  read when the file is here.
- Against the estimate (README: main about -5.7, range -2 to -9; ME about -9; K7B - K7 about -4.3): met.

## Reading
- B is worth 4.7 to 6.0 ms a step in the rain wait on the PSP, exact for the game (hashes equal to its own reference; the guards' dice
  are the PS2's by the twin proof).  The rain stays main-CPU-bound: main 69.3 against ME 55.3 in K-N2B, 14 ms of ME room.
- WHERE THE MAIN CPU'S RAIN STEP GOES NOW (K-N2B): logic 38.4 + the game's own draw pipeline 24.7 + about 6 other = 69.3.
  With nothing drawn and B (K7B, about 61.3): logic about 32.2.  So the two main levers left in the rain are the pipeline (24.7: the
  PS2 draw preparation still done by the main CPU on every shown step) and the logic (32-38).
- Consequence for the route: "draw preparation off the main CPU" pays in the rain as well as indoors (indoors pipeline 13.3 of
  K7's 36.55), and the ME has room for part of it in the rain.  Size it next (which functions make the 24.7).
