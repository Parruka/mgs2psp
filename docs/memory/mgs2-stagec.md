---
name: mgs2-stagec
description: MGS2 Stage C (native drawing from game data): steps 0-3 (texture pages gl13, faces T1/T2/C1 gl14, people C2 gl15 kept off, rooms' meshes trims in gl16), step 4 effects (converters checked, batches next), model jitter, lessons
metadata:
  type: project
  modified: 2026-11-03T10:15:00.000Z
---

- Chosen by Ffm (card 2026-11-01 20:50): mgs2/native_draw_study.md (faces, people, rooms' meshes, effects). Notes: mgs2/stagec_notes.md, step N = sections N.x.
- STEP 0 (texture memo by upload, CLUT loads by identity; g_opt2 1, 2) = gl13 (tree sb9).
- STEP 1 (tree sb11): T1 memo front (OPT2 4), T2 P07 calls held to the transfer's end (8), both exact; C1 the GE skins/lights/maps P07 (16; faces 1-3.5% of pixels, ~99% by 1 level, state never changes). gl14 PSP: T2 -5.4..5.6 ms in the calls, T1 -1.8/-2.0, C1 -2.0/-2.6 in the calls only. C1 ON BY DEFAULT since gl15 (my call 2026-11-02 17:12, Ffm left it to me).
- STEP 2 (C2, the people, tree sb12, [[mgs2-c2-people]]): exact, but no CPU saving on the PSP; FFM KEPT IT OFF (2026-11-02 23:22): OPT2_DEFAULT = ~OPT2_C2; retry after the ME picture work.
- STEP 3 (task #144; tree /home/claude/sb14 = sb12 + tools/dev/sb14/sb14.diff; stagec 3.1-3.11, inserted 2026-11-03 00:14): the GE already transforms df0, so three exact CPU trims, on by default: 3v two passes (OPT2 64), 3z depth range by the box (128), 3w vertices straight into the frame room (256, needs 3v). x86 3 routes: every line = trims off, 0 different. Model (plain builds sb14m, sb14m2), own functions: 3v -0.93 deck / -0.92 inside, 3z -0.23/-0.25 (lost to the rest of the window there), 3w -0.29/-0.51. GL16 = sb14, trims all on vs all off in turns (task #146; drivers tools/dev/sb14/gl16/): DELIVERED 2026-11-03 02:28 (MGS2T_gl16, psp_harness.md "Build gl16"), exact on both emulators and the master (16 master shots = the C2-off master); its own model: all on 143.2 deck / 87.7 inside, trims off 144.7 / 89.8, so the trims save 1.5 / 2.1 (3w 0.6/1.3, 3v+3z 0.9/0.8, 3z alone noise). PSP RUN 1 PASSED (files 2026-11-03 13:34, prof/hw_gl16/run1): exact, 44.9 min, headroom ~388 KB; trims save 1.8 deck / 2.0 inside (model 1.5 / 2.1), all on -0.3/-0.4% vs gl15; they stay on. Not now: kept GE vertices per mesh (26-254 writers, no exact change test), lane bookkeeping (3b).
- STEP 4 (task #152, the effects): [[mgs2-stagec-effects]].
- MODEL WINDOW JITTER [C, .calls files]: the deck window's game code is identical every run, but the picture work inside a window moves up to 0.6% of the translator's calls (gif_qw 686.7k-690.8k) = +-0.1-0.2 ms; inside, game hardware polls vary too (rt_mm 58.2k-58.7k, game code +-0.2 ms) and a window can hold one more game step pass (80 calls for 79). Judge a trim by its own functions, or check call counts first.
- DATA-CACHE PLACEMENT: the model's set-94/95 finding (#138) is unproven on the PSP [I]; tools tools/dev/sb11/ana.
- LESSONS: plain model builds swing +-2 ms by placement; new hot code must be in the PGO training and traces; the PSP gives 0.3-0.8 of the model's all-on change.
- Open trims: face vertex cache (stagec 1.7, ~1.5-2 ms in the calls [I]); gate T2's slot bookkeeping (~0.2 ms inside, stagec 1.6).
