---
name: ff8-s9
description: FF8 PSP build s9 (step 14 exact movie decoder speed-up + gestall 16), ran on the PSP 2026-11-13 (49.9 ms a frame, movies 46.1); model and measured figures, files
metadata:
  type: project
  modified: 2026-11-13T19:43:50.699Z
---

**s9 = s8 + step 14 (exact MDEC speed-up) + "gestall 16" default.** DELIVERED 2026-11-13 19:43 to PC
`FF8\Builds\FF8T_s9\FF8T` (EBOOT c3e90da1, 9,188,416 bytes, "FF8 speed s9"; ff8.cfg d28a3941; README 1ddaad2f; pad +
hashref copied on the PC from FF8T_s8; 5 md5s checked; staged copies deleted). Logs come back in `FF8\LOGS\s9`.
THE ONE BUILD OUT: no new PSP build before its log is read. Container: psp/rel_s9 (never touch / never make there),
layout build folders vd, ve (= s9), vf, vg (never `make` there; link only with layout/link.sh). Sources: shared
/mnt/project-files/ff8/src_s9 (ref/mdec.c, ref/hw.c, cfg, README, layout); container backups ref/mdec.c.s8, hw.c.s8.

**Step 14 (all exact; x86 out.txt aae53db0; MDEC_CHECK=1 and MTCHECK=1: 0 differ):** 14a transform by the scale
table's mirror symmetry + colour terms per chroma sample; 14b the timing model's 310.6 M one-word actions done in
27.7 M calls (mt_step_x; mt_step_ref kept as the definition); 14c whole-transfer MDEC DMA paths (hw.c); 14d +128
carried in the chroma terms, direct 24-bit output words; 14e the transform's even half with 6 multiplications.
TRAP: a check that runs the reference beside the new code can hide state the reference sets up (mt_grid): always run
the plain build too and compare out.txt.

**Emulator model (ms a frame; vc = s8 -> ve = s9):** title 8,000: 64.17 -> 66.29; movie 20,000: 73.70 -> 42.38;
busy 29,000: 24.85 -> 24.86; quiet 36,000: 20.59 -> 20.68. Told Ffm [I]: movies ~44 on the PSP (42-48; s8 measured
75-80), fields as s8, whole run ~51 ms a frame / ~34 min. Floor for movies on the exact route ~30 ms [I] (game's own
DecDCTvlc2 3.1 ms, SPU, machine).

**To read in s9's PSP log:** movie windows 15,000-25,500 (s8 75-80); title window 8,000 runs with the picture check
OFF for the first time (s8 79.0 with 15.3 show+check); busy 29,000 check off (s8 26.65); quiet 36,000 (s8 20.20);
schedule has no gequeue/genull/gestall windows any more (12 "at" lines).

**RAN ON THE PSP, read 2026-11-13 21:25 (psp/psp_s9):** exact (80/80, shots = s8's); whole run 49.886 ms a frame,
33.5 min (s8 58.848); movies 15,000-25,000 mean 46.1 (44.5-51.8; s8 77.0; window 20,000: 45.18, model 42.38); title
check on 81.25 (s8 80.23: +1.5 real), window 8,000 check off 68.10 (model 66.29: the check costs ~13.8 ms on the
title); busy 29,000 26.58; quiet 36,000 20.27; "gestall 16" gave no gain to see. No longer the build out (see MEMORY.md for the one out).

**Open:** title +2.1 ms in the model (+7.8 k D-misses a frame; not the .bss shifts, not _end; cause UNKNOWN, how to
find out is in stage5_notes.md). mt_step_x still 3.2 ms (2,217 calls a movie vblank x ~300 instr of 64-bit time
maths). Idea not built: skip the game's wait loop in MovieUpdate (`while (!pictureDone) MovieResumeReading()`,
0x801E8304, 144 k instr a vblank) exactly, like the pad waits.

Related: [[ff8-s8]], [[ff8-movies]] (the intro video trial Ffm chose 2026-11-13 19:00), [[ff8-stage5]].
