---
name: ff8-stage5
description: FF8 speed track after stage 4 (2026-11-11): compact generated code = PSP build s1 (ran: 81.7 ms a frame, quiet field 53.6, exact); steps s5-s10 (owed SPU samples, light scanline events, P in a register) = PSP build s2 (ran 2026-11-12: 76.9 ms a frame, quiet 42.2, exact); s3 = direct translation delivered 02:36, the one build out
metadata:
  type: project
---

Part of [[ff8-port]], after [[ff8-stage4]] / [[ff8-g3]]. Notes: /mnt/project-files/ff8/stage5_notes.md §1 (1.5 code steps, 1.6 build s1, 1.7 event steps). U1 / screen shape / new art: [[ff8-u1]]. Ffm's card 2026-11-11 20:09: "Both, speed build first"; the Media Engine for the exact drawing is a later, separate OK.

- Model ms a frame [I], quiet (frames 36,000-36,500) / heavy (29,000-29,500), all steps EXACT (x86: 80 hashes + end line + per-frame blocks and cycles; PPSSPP 73/73): g3e 65.8 / 84.0; L2b compact call sites (gen.py --compact, `sh recomp/build.sh compact` -> recomp/genc + psxrcc) 54.58 / 71.74; s3 single-compare block end (hw_fast_dl) 51.31 / 68.12; s4 HostHot (hot host variables in 64 bytes before Psx) 51.35 / 68.09; s5 owed SPU samples 46.23 / 62.63; s6b idle skip past owed ticks 45.41 / 61.95; s7 light scanline events 44.67 / 60.93; s8 (no 64-bit division in line_event, cheaper next-event) 43.88 / 60.32; s9 stores test the code-page flag first 44.42 / 61.13 (runtime part fell, total rose: layout noise is ~0.5 ms; kept); s10 P in register s7 on the PSP (-ffixed-s7; callbacks from libc/system must not use P) 42.80 / 59.38. Since s1: -17% quiet, -13% heavy.
- s1 (= s4 state, EBOOT 532eefe7) RAN ON THE PSP, read 2026-11-11 22:44 (FF8\LOGS\s1, container psp/psp_s1, notes §1.9): exact (80/80, out.txt, shots, picture check = g3's); 81.666 ms a frame (g3 99.602, -18%); quiet field 53.6 (g3 70.1; check off 51.8, model 51.35), busy 64-76 (g3 77-98), title 95-103 (g3 121-127), movies 81-97 (-11.7).
- s5 design: while the SPU interrupt is off a sample tick is only counted and spu_flush runs the owed ticks before every SPU access, hash, save, load; block ends have a light path. In the movies the interrupt is on: ticks run per event. Switches --nospudefer, --noidlespu, --nolightlines.
- DROPPED after measuring: finer invalidation of block-start stamps (~0.15 ms).
- LESSONS: -Os on the generated code was slower (hot globals in a thrashing D-cache set); 1,068 full block ends a frame were 737 SPU ticks + 263 scanlines, each ~1,000 instructions of event code that also evicted game code.
- Sources and dev builds: /home/claude/ff8 (backups *.g3 ... *.s9), psp/s5d..s10d, profiles psp/prof_<name>.
- PSP BUILD s2 = s10 state (psp/s10r, EBOOT 147666bb; log's first line says "FF8 dev s10d"): delivered 2026-11-11 22:44, ran (next line).
- s2 RAN ON THE PSP, read 2026-11-12 01:36 (FF8\LOGS\s2, psp/psp_s2, notes §1.13): exact (80/80, out.txt aae53db0, shots and dumps = s1's); 76.893 ms a frame; quiet check off 42.2 (model 42.80), check on 44.0-44.7; busy 49-69, heavy window 61.9 (model 59.4); title 91-97; movies 81-97 (unchanged: SPU interrupt on). Quiet field = 39.5% of PS1 speed.
- PSP BUILD s3 = dev a4 ([[ff8-direct-translation]]; psp/a4r, EBOOT 175ac81f, log's first line "FF8 dev a4"): DELIVERED 2026-11-12 02:36 to FF8\Builds\FF8T_s3\FF8T, logs to FF8\LOGS\s3. THE ONE BUILD OUT. Expected [I] quiet ~38, busy 51-65. Never run make in psp/a4r or psp/s10r.
- s3 RAN ON THE PSP, read 2026-11-13 13:00 (LOGS\s3, psp/psp_s3, notes "s3 RAN ON THE PSP"): exact (80/80, out.txt, shots, dumps = s2's); 75.787 ms a frame; quiet 38.72 (model 38.41), heavy 57.90 (model 55.15, +2.75 as s2); movies -6; BUT title +3..+6.5 and start-up window 0-500 +49 (emulator +24.5): cause UNKNOWN, not in the model's windows; s3 interprets 1.38 M blocks (cold entries; s2 0).
- NEXT: see [[ff8-s7]] (newest build) and [[ff8-s6]] (not-built list); the Media Engine card is withdrawn ([[ff8-stage5]] line in MEMORY.md).
