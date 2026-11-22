# gl36 vs gl35 on the PSP: native ship path, per-group stage timers (rain, ME turns) [C]
Source: LOGS\gl36\log.txt and LOGS\gl35\log.txt "nat split" lines (PC _claude_tmp\natsplit_gl36_gl35.txt, 26 lines). Arms: t=(vb-24001)//100, reuse t%4<2, cull t%8<4; gl35 keeps 15 when t%4<2.
us a group = the turn's native time / groups (timed-stage sums are higher: each holds a clock read).

| turn | arms | groups | gl36 us/grp | gl35 us/grp | d | native ms/step d |
|---|---|---|---|---|---|---|
| 29000 | R0C1 | 8869 | 69.97 | 71.39 | -1.42 | -0.13 |
| 29200 | R1C0 | 31782 | 53.47 | 57.90 | -4.43 | -1.41 |
| 29400 | R0C0 | 30025 | 58.14 | 60.31 | -2.17 | -0.65 |
| 29600 | R1C1 | 30773 | 53.97 | 59.26 | -5.29 | -1.63 |
| 29800 | R0C1 | 30447 | 58.72 | 60.88 | -2.16 | -0.66 |
| 30000 | R1C0 | 30100 | 52.70 | 58.43 | -5.73 | -1.72 |
| 30200 | R0C0 | 30528 | 58.98 | 61.26 | -2.28 | -0.70 |
| 30400 | R1C1 | 29650 | 52.78 | 57.79 | -5.01 | -1.49 |
| 30600 | R0C1 | 31850 | 57.07 | 58.38 | -1.31 | -0.41 |
| 30800 | R1C0 (walk) | 22882 | 63.59 | 56.78 | +6.81 | +1.56 |
| 31000 | R0C0 (walk) | 14796 | 51.21 | 51.59 | -0.38 | -0.06 |
| 31200 | R1C1 (walk) | 15606 | 55.00 | 49.94 | +5.06 | +0.79 |

Rain wait (29200-30400): gl36 reuse turns 53.2 us a group vs gl36 plain turns 58.6 -> REUSE ALONE -5.4 us (-9%), about -1.6 ms a step of ME [C]
(model -3.13: PSP/model 0.5). gl35 keep15 vs keep0 -2.5 us. gl36 plain vs gl35 plain (keep 1 + retrain) -2.2 us (-3.6%). Cull does not touch this path (C1 = C0).
Stages, reuse turns vs plain turns (gl36): positions 1.1 vs 4.0, register 5.7 vs 9.2, attributes 3.3 vs 1.8 (+1.5), transform 4.0 vs 4.2.
All gl36 turns vs gl35: drawn areas 1.85 everywhere (keep bit 1; gl35 3.4 in keep-0 turns), depth range 1.5 vs 2.2, into the frame 0.87 vs 1.4, host call -0.4, call's rest +0.8 (1.9 vs 1.1).
Walk: reuse finds little to replay (emulator, exact so same on the PSP: 30800 replays 1,590 of ~22,900 groups while recording 20,587; 31200 4,194 of 15,600;
rain wait reuse turns replay ~26,000 of ~30,000 = 87%). Positions are not cut there (4.3 / 3.4) and register +2.0/+1.1, A+D +1.4/+1.2, call's rest +1.0: the
recording and lookups cost ~5-7 us a group -> +0.8..+1.6 ms a step. => gl37: reuse only when the last picture replayed well.
Fixed work a group (plain rain turn): walk+pointers 5.6, register 9.2, A+D 6.4, host call+GS state 5.5, texture 8.8, way back 2.5, drawn areas 1.9,
depth range 1.5, into the frame 0.9, walk's end 1.1 = ~43 of ~66 timed us; per-vertex work (transform, positions, attributes, strips, GE batch) ~16.
