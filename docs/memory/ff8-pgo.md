---
name: ff8-pgo
description: FF8 profile-guided PSP build (2026-11-12): psp/Makefile PGO=gen / PGO=use with a recording run in PPSSPP (cfg "pgo A B", .gcda in /root/.ppsspp/pgo); dev build pu1 exact, model 31.98 quiet / 40.40 heavy (a8 36.73 / 47.35)
metadata:
  type: project
---

Part of [[ff8-stage5]], after [[ff8-lazy-drawing]]. Notes: /mnt/project-files/ff8/stage5_notes.md §1.15. Recipe taken from the MGS2 work.

- Why: the runtime C code's hot functions (GPU command path, events, GE renderer: ~70 KB) were spread over 4 MB and pushed each other out of the 32 KB I-cache: 26-29 k I-misses a frame quiet (4.5 ms), and +-1 ms of "layout noise" from build to build.
- PGO=gen: rt.o, r_*.o, ge_ff8.o with -fprofile-generate -fprofile-update=single, -lgcov; main_ff8.c -DFF8_PGO, cfg "pgo A B" (counts zeroed at frame A, written at frame B to ms0:/pgo = /root/.ppsspp/pgo in the container: the folder must exist and be EMPTY before a new recording). The generated game code is assembly and takes no part.
- PGO=use: copy the .gcda files into the build folder (plus the g_*.o of a normal build), `make -f ../Makefile G=gena PGO=use`. -freorder-blocks-and-partition does not link (16-bit branches / HI16-LO16 pairs between hot and cold halves > 128 KB apart): PGOX stays empty.
- Recording used: psp/pg2 (= a8's sources), the full 40,000-frame run with "lazy 1", frames 1-39,990; files kept in psp/run_pg2/gcda. A profile matches only the sources it was recorded from: after any change to rt.c, ref/*.c or ge_ff8.c record again (6 minutes) or accept unprofiled changed functions.
- pu1 (psp/pu1, prof_pu1): exact (73/73, 74 whole-VRAM sums and 36,600 per-frame lines = the lazy-off run). Runtime C code quiet: 2,557 k -> 2,393 k instructions, I-misses 28.8 k -> 12.0 k a frame.
- Model so far (quiet / heavy ms a frame [I]): s2 42.80 / 59.38 (PSP 42.2 / 61.9); s3 = a4 38.41 / 55.15; a8 36.73 / 47.35; pu1 31.98 / 40.40.
- pu1's full 40,000-frame emulator run (psp/emu_pu1r, s3's schedule + "lazy 1", 03:28): 80/80, end line, out.txt aae53db0, picture check 3,961 pictures / 138,846 px / worst 336 (unchanged), 0 texture-window refusals of 25,485, shots' exact planes = s2's and s3's, GE planes equal in colour. Ffm told 03:3x. s4 = this state, held until s3's PSP log is read (release folder, title "FF8 speed s4", README, delivery as for s3).
