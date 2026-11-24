# gl42 on the PSP, run 1 (2026-11-24 12:18-12:46 UTC): read

Build gl42 (EBOOT bb85fb95, own hashref 0cbcb5b9; harness "Build gl42"). Logs: PC LOGS\gl42 (13 files, md5-equal to the stick).
Reader: rd42pc.py (output kept in the thread's notes; key lines below).

## Stop
- The PSP switched itself off at about t 1,655 s, just after vb 29,561 (start of t55, the first digest-on turn, XP7 check, K-N2).
- No crash record (catcher 3b on; its self-test was skipped: "no answer in 15 s"). Battery thread stopped too. Battery 96%, 4,206 mV;
  charging 1 in 261 battery lines, 0 at the end; external power 0 throughout. Same signature as gl40 r1 / gl29 / gl29x. Cause UNKNOWN.
- Checked and NOT a hang: the ME picture loop kept going to the last line (loop heart +4,181, GE lists +26 in the last 5 s);
  t54's ME columns are empty only because the per-turn ME lines are logged one turn late. The 1,037 ms low-priority wait at
  vb 29,389 is t53 (K-N2R: the ME is faster than the main CPU, so the main thread never waits and never yields).
- System memory outside newlib's heap: 44 KB (largest block 24 KB) at the end; gl41 ended at 47/27 and ran fine; gl40 212/112.
- Exact to the stop: 58/58 hashes equal gl42's hashref; schedule check none; all problem counters 0.

## Deck (n = turns; ms a PSP step: step / main / ME) [C]
| Kind | n | step | main | ME |
|---|---|---|---|---|
| K-A all drawn | 25 | 49.23 | 40.29 | 36.27 |
| K-N1 ship native | 5 | 49.04 | 39.09 | 39.67 |
| K-N2 ship+people native | 10 | 45.32 | 37.58 | 33.78 |
| K-N2R only ship+people drawn | 7 | 39.36 | 39.13 | 18.88 |
| K7 nothing drawn | 3 | 25.65 | 25.47 | - |

Steady deck (vb 26,200-28,700), ME-bound: K-A 40.4-41.3 (main ~24.4, ME ~40.8); K-N1 and K-N2 equal to K-A within 0.5 (ship 0 calls,
people 637 groups there: little to gain); K-N2R 22.0-22.4 (main ~21.9, ME ~17.5); K7 19.42 (main 19.19).
-> On the steady deck the ME's load is the OTHER chains (backdrop, sea, effects). Upper bound for the full renderer there: ~22-28 ms a
   step (main-bound) = 18-22 fps [I; K-N2R leaves those chains out, their native list writing and GE bus share come on top].
Deck first play (vb 25,100-25,900) is main-bound: ~36-38 a step, K7 33.9: the renderer cannot help there (logic + pipe).

## Rain, first turns only (t51-t54; t55-t73 lost) [C, one turn each]
| Turn | Kind | step | main | ME | logic | pipe |
|---|---|---|---|---|---|---|
| t51 | K-N2 | 80.06 | 79.89 | 70.94 | 44.76 | 28.03 |
| t52 | K-A | 79.71 | 79.22 | 76.32 | 44.63 | 26.87 |
| t53 | K-N2R | 74.98 | 74.48 | 43.74 | 40.16 | 27.53 |
| t54 | K-A (digests off) | 76.06 | 75.89 | (late line lost) | 42.94 | 25.75 |
- Native ship+people in the rain: ME -5.4, main CPU about 0 (+0.7 vs t52). The reuse content hash costs 0.94 ms a step there.
- Logic (identical work in every kind) runs 40.2 in K-N2R vs 43-45 in K-A: the GE/ME drawing slows the main CPU through the bus.
- People: 17,607-18,032 groups a turn, no room 0, re-cooks 0. Ship arena peak 751 of 768 KB (near full: watch it).

## Checkpoint (design §4.3) from the partial data [I]
P = K-N2R main - what the rest of the renderer removes there (about 6: the chains' walk and copies, 12d910, 13bf78, the EVM builder,
§4.1 minus the ship and people shares already gone) + its list writing (about 2) = about 70 at the first turns; about 64-67 if the
rain wait proper sits ~4 lower as in gl40/gl41's K7. Both >= 62: by the rule the renderer alone cannot carry the rain.
Not yet [C]: the rain wait proper has K-N2R at t61, t67, t71 and K7 at t63 in the same run. Lead decision 13:0x: RERUN gl42
unchanged (charger), logs to LOGS\gl42\r2; it also tests whether the stop repeats at the same vb.

# Run 2 (2026-11-24 13:23-13:53 UTC, LOGS\gl42\r2): read 14:0x

- RAN TO THE END ("Press X to exit"), on battery alone (90 -> 85%, charging 0 and external power 0 in every line). The same build
  passed run 1's stop point: run 1's stop was not a fault of the code at that spot [I]. System memory outside the heap 44/24 KB at
  the end again (normal for gl42). Battery-alone tally: +1 stop (r1), +1 clean (r2).
- EXACT: hashes 62/62 (70686ac12d16), digests 61/61, content 61/61, schedule none, XP check 720 compared 0 differing, all problem
  counters 0. Native converter checks 0; people not drawn natively 94 of 171,045 groups; evicted 212, no room 0. Heap room 2,636 KB
  at worst. Deck rows equal run 1's within ~0.4.

## Rain wait proper (digests off; ms a PSP step) [C]
| Kind | n | step | main | ME | logic | pipe |
|---|---|---|---|---|---|---|
| K-A all drawn | 7 | 76.21 | 75.85 | 70.48 | 42.70 | 25.83 |
| K-N1 ship native | 1 | 75.30 | 74.77 | 67.76 | 41.80 | 25.80 |
| K-N2 ship+people native | 2 | 75.45 | 75.12 | 64.13 | 42.13 | 26.49 |
| K-N2R only ship+people drawn | 2 | 73.84 | 73.46 | 44.10 | 39.24 | 27.55 |
| K7 nothing drawn | 1 | 66.42 | 66.10 | - | 34.42 | 24.94 |
Pairs against their K-A neighbours: K-N2 main -0.9 / -1.4 (ME -5.4 / -8.3); K-N2R main -4.0 / -2.3; K7 main -9.1.
Walk (one turn each): K-N2 main -3.7 (ME -11.3), K-N2R main -5.7 (ME -26.4).

Findings:
- Native ship+people in the rain: main -0.7..-1.4, ME -6 [C]. The rain stays MAIN-bound.
- Drawing costs the main CPU 9.75 ms a step in the rain (K-A - K7), and 8.3 of it is LOGIC: the same game work runs slower while the
  ME walks and the GE draws (shared memory bus). Pipe is nearly the same with nothing drawn (24.9 vs 25.8): the game's draw-side work
  and our exact walk run every step whatever is drawn.
- K-N2R pipe 27.55 is ABOVE K7's 24.94: the native path's own main-side work (records copied at REC, reuse hash 0.9, list hand-over)
  costs more than the ship/people shares it removed.

## CHECKPOINT (design §4.3), lead decision 2026-11-24 14:0x
P = K-N2R main 73.46 - what the rest of the renderer removes there (remaining walk + copies about 6, builders 12d910/13bf78/EVM
about 2) + their list writing about 2 = about 67 [I on C]; even removing the whole §4.1 list (12) gives 63. P >= 62 -> by the rule
the renderer cannot carry the rain. (c) with the rain logic cut B (~5 [I]) is about 62, not < 50.
DECISION: the renderer is NOT the route through the rain. It stays for the ME-bound scenes (steady deck 40.7 -> about 22-28 upper
bound [I]); the rain is a LOGIC problem (floor with nothing drawn 66.1 = 7.6 fps; logic alone 34.4). Next for the rain: Ffm's cut B
(keep the rain lines nearest the camera; game side), measured in turns. Half-rate game: NO-GO at this P (plan: high end -> the
Vita-style lever instead); its ME side also fails today (K-N2R rain ME 44 a step = 88 a picture for ship+people alone).
