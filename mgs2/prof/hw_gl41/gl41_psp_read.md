# gl41 on the PSP: the read (2026-11-24, lead)

Build: gl41 (package /home/claude/main/build_gl41/deliver, EBOOT 44272485, cfg bf35e0c6, hashref c9ddb593; report
main/attic/gl41/report.txt). Launched 11:09:28 UTC on Ffm's "run it", ended "Press X to exit" ~11:39 (log 5,810 lines). PC: LOGS\gl41,
59 files md5-equal to the stick (log.txt 157d54ed, steps.csv 8bdf3477, battery.txt 63aa1302). Reader rd41pc.py (609 lines; full
output kept on the PC only). ms a PSP step (a vblank); fps = 500 / ms a step. [C] = PSP log.

## 1. Exactness [C]
Hashes 62/62 EQUAL (66499cdb584a); digests 116/116 EQUAL; CONTENT 116/116 EQUAL (native ship = today's); schedule check none;
XP check 1,320 plans 0 differing; F1 problems 0; native step 1: declined 0, stub-time WORLD differs 0 (the loop-apart fix holds on
the real ME), no room 0, evicted 37, cooked 272, reused 37,476; probe c (P3) 690 forced inputs, 0 different; ME loop dropped 0.
Power: on charge (80 % -> 97 %, "charging 1" at the end).

## 2. Memory [C]
The PSP's newlib heap is far larger than the emulator's: room 11,591 KB at the start; the route's worst moment (texture table, vb
536) 2,679 KB above the break; ME heap events' lowest 2,703 KB; 2,688 KB free at the end; textures refused 0. The emulator gate
(170 KB) is a conservative floor: gl42's +32 KB of code is no risk on the PSP. Main stack 2,948 of 129,472 B.

## 3. Rain wait (digests off) [C]
| kind | n | step | logic | pipe | gsw | ME busy | plan |
|---|---|---|---|---|---|---|---|
| K-A | 3 | 74.79 | 41.99 | 25.29 | 4.41 | 68.96 | 4.02 |
| K-N1 (ship native) | 2 | 75.61 | 42.53 | 25.64 | 4.39 | 66.29 | 4.03 |
| P1 off | 1 | 76.39 | 42.88 | 25.99 | 4.42 | 69.29 | 4.05 |
| P3 off | 2 | 75.59 | 42.01 | 25.64 | 4.42 | 69.06 | 4.00 |
| K7 | 1 | 65.90 | 34.28 | 24.73 | 3.99 | - | 0 |
- K-A 74.79 = 6.69 fps (gl40 XP3 same stretch 76.44, gl39 78.1): gl41's new order -1.65 (logic -0.75, pipe -0.97); model -0.84..-1.11.
- Native ship step 1: ME busy -2.67 (pair t59 -2.54) [C], step +0.3..+0.8 (main-bound: the ME's gain cannot show). Heavier stretch
  (t67) and the walk (t68) show no ME gain / scene confound (walk content grows turn to turn).
- P1: +1.46..+1.60 when off = ~1.5 ms a step gain [C] (model 0.9-1.4). Keep on.
- P3: +0.36..+0.94 when off, mostly GE wait; logic +0.0..+0.3: a small gain at most. Exact; keep on.
- Drawing's cost on the main CPU (K-A - K7): +8.89 (logic +7.71); gl40 XP0 +13.55.
- Digest turns cost +24 a step now (ME busy 99 vs 69, lists 36 vs 5): gl41 adds the content digest on the ME. Measurement only.

## 4. Other scenes [C]
- Steady deck (ME-bound): K-A 40.90 (n 13), ME 40.80, gew 16.85; K-N1 -0.13 (no ship mesh drawn there: control), P1/P3 off +0.33 /
  +0.15. K7 19.18.
- Deck first play: K-N1 -1.83 (ME -3.77). First deck scene: pairs swing +-15 (scene changes inside turns): not read.
- Main-drawing windows before vb 24,000 (main): 54.96 (gl40 55.51, gl36 55.56: the best so far); 20,000-24,000 83.53; tanker bow 49.00.
- Probe a: a missed 64-byte line 172-175 ns; with a 'cache 0x1e' fill 1-2 lines ahead 189-190 ns: the fill does not help. Dropped.
- Probe c: box cull per call VFPU 7,348 ns, P3 7,235, C form 9,595.

## 5. What it means
- The rain is main-CPU-bound (main ~74.4 vs ME 69.0). ME savings (native ship -2.7, native people next) only show once the main
  side falls under the ME. gl42's K-N2R turns measure the main side with the native renderer doing the ME's work.
- Next: deliver gl42 unchanged (C2 on, P1/P3 on, XP3); read K-N2 / K-N2R against K-A for the renderer checkpoint.
