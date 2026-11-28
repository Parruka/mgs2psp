# gl42in on the PSP: the indoor measurement (2026-11-27)

Build: gl42's EBOOT (bb85fb95) + mgs2.cfg 5b0fddb0 + hashref 5d6b129f (attic/gl42in/notes.md).  Route pad_w01af to vb 37,800; indoor
(stage w01a, the crew deck) timed vb 31,800-37,800.  Figures below are the reader's (rd42in.py 7b990600) as reported from the PC
(LOGS\gl42in\r2\rd42in_out.txt, 69,615 B; the log files themselves are still to be copied here: OWED).  All [C] PSP unless marked.

## r1, launched 16:29:45 UTC: froze, then the PSP off by itself, right after the deck door.  No log (overwritten by a second start).
- Reached >= vb 31,287 (last live shot); Ffm saw the freeze after Snake opened the door, then the PSP went off by itself.
- External power during r1: UNKNOWN (the second start, minutes later, read 71% / 4092 mV).
- NOT deterministic: r2, the same 18 files, launched the same way, passed the door.  So r1 belongs to the no-record "freeze, then off"
  stops (mgs2-switch-offs: gl29 30,760, gl29x 31,040, gl42 r1 29,561, now ~31,5xx); cause still UNKNOWN.

## r2, launched 17:51:42 UTC (pclink, plugin ON, charging 1, battery 81 -> 91 %): RAN TO THE END in 36.7 min, EXACT
- Hashes 124/124 (ea5ddb8a15ee), list digests 161/161 (88e5c3ee0456 / 40cdae675359), content 161/161 (c5a6029df50c) = the emulator's and
  the twin's.  Schedule check none; XP check 1,720 plans, differing 0; problem counters none; textures refused 0.
- Heap: headroom at the start 11,552 KB, heap end 0bd9bfff (= gl42).  Lowest room + top 2,725 KB at vb 23,676.  Outside the heap:
  largest 24 KB, total 44 KB.  (The accidental XMB start had 7,456 KB and heap end 0b99bfff: an XMB launch gives 4 MB less heap than a
  pclink launch [C, one start]; why: UNKNOWN.  It matters for a play build started from the XMB.)
- Stage loads at the door: w00a 4,401,856 B in 777 ms, w01a 3,497,168 B in 623 ms.
- Deck and rain/walk means within 0.3 ms of gl42 r2.

## Indoor means, ms a step (a step = 1/50 s; fps = 500 / ms a step)
Columns: step, main, ME, P | logic, draw pipeline, GE wait | ME walks, ME lists.

| kind (turns)            | step  | main  | ME    | P     | logic | pipe  | GE wait | ME walks | ME lists | fps  |
|-------------------------|-------|-------|-------|-------|-------|-------|---------|----------|----------|------|
| to the door, K-A (4)    | 49.78 | 47.59 | 16.70 | 48.86 | 37.33 | 4.29  | 2.20    | 14.38    | 1.41     | 10.0 |
| K-A, today's path (25)  | 52.35 | 41.66 | 50.04 | 51.67 | 23.71 | 12.98 | 10.69   | 43.69    | 4.01     | 9.6  |
| K-N1, ship native (5)   | 50.01 | 40.93 | 45.61 | 49.27 | 23.36 | 12.83 | 9.08    | 39.89    | 3.57     | 10.0 |
| K-N2, + people (8)      | 47.93 | 41.43 | 45.15 | 46.81 | 23.40 | 13.71 | 6.50    | 39.13    | 3.88     | 10.4 |
| K-N2R, only those (8)   | 45.50 | 41.87 | 40.86 | 44.25 | 22.84 | 14.44 | 3.64    | 35.36    | 3.96     | 11.0 |
| K7, nothing drawn (2)   | 36.55 | 35.58 | -     | 35.58 | 17.39 | 13.31 | 0.97    | -        | -        | 13.7 |
| K-N2, digests on (2)    | 93.64 | 45.72 | 95.32 | 95.32 | 26.63 | 14.66 | 47.92   | 42.52    | 50.41    | (measurement turns) |
| past the cycle, D (10)  | 66.38 | 54.99 | 65.02 | 65.02 | 32.92 | 14.82 | 11.39   | 57.32    | 5.02     | (defaults: old settings) |

Differences: K-N1 - K-A -2.34 (ME -4.42); K-N2 - K-A -4.42 (ME -4.89, GE wait -4.18); K-N2R - K-A -6.85 (ME -9.17);
K-A - K7 +15.80 (logic +6.31: drawing slows the logic through the bus, as on the deck).

## Reading
- Indoors today: 52.4 ms a step = 9.6 fps at 38 % game speed.  gl20 (2026-11-19) had 89.58 (main turns): 5.6 fps.
- Indoors the ME is the limit in K-A (ME 50.0 against main 41.7), so the native renderer pays here: ship + people -4.4 ms (10.4 fps).
  With only the native groups drawn the main CPU becomes the limit (41.9 against ME 40.9).
- Main CPU alone (K7): 36.55 ms a step = 13.7 fps: the indoor ceiling with EVERYTHING else free.  Of it: logic 17.4, the game's draw
  pipeline 13.3 (still run on the main CPU), the rest ~5.9 (IOP + sound and others) [I: split by subtraction].
- 20 fps = 25 ms a step: the main side needs -11.6 from K7's 36.55; the pipeline's 13.3 is the renderer's later steps (3-7: the pipeline
  off the main CPU), logic 17.4 needs the lean-code and logic levers.  25 fps = 20 ms a step: -16.6.
- With drawing, logic costs 23.7 instead of 17.4: +6.3 from sharing the bus with the ME.

## Open / to check when the log files are copied here
- The PC side read "rain: K-N2 +11 ms against K-A (ME +20, pipeline +10)" from rd42in_out.txt; gl42 r2's read gave K-N2 -0.7 in the rain
  wait and the deck/rain means are reported within 0.3 ms of gl42 r2, so this is most likely a digest-on turn read as K-N2 [I].  Check
  against the RAIN table before quoting either figure.
- Ideas noted for the random freeze (not built): a 1-s heartbeat line written straight to the stick (vb, t, main and ME counters, battery,
  temperature); a standalone 40-min CPU + ME stress EBOOT with no game, to split hardware/power from our code.
