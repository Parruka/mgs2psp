---
name: ff8-g3
description: FF8 g3 (2026-11-11): renderer speed build (page tiles, lazy mirror, queue), deterministic cache layout (P_OFF), measuring schedule for its PSP run; ran on the PSP, read 20:08 (99.6 ms a frame, exact, picture = g2's)
metadata:
  type: project
  modified: 2026-11-11T17:58:42.186Z
---

Part of [[ff8-stage4]] / [[ff8-ge-renderer]]. Notes: /mnt/project-files/ff8/stage4_notes.md §7.7. Sources (shared, g2's kept as *_g2*.txt, rt_b3.c.txt): psp/ge_ff8.c 8b591e1f, psp/main_ff8.c f2e9be27, psp/Makefile 7f5da4ed, recomp/rt.c afdfa550.

- STATUS: G3 RAN ON THE PSP, READ 2026-11-11 20:08 (FF8\LOGS\g3; container psp/psp_g3; notes §7.9). Exact (80/80, out.txt = b3). Picture = g2's on the PSP (check 184,009 px over 3,961 pictures, worst 481 = 0.67%; shots and per-primitive files equal g2's). 99.602 ms a frame (g2 103.666, b3 91.063): quiet field 70.1 (g2 72.9, b3 62.1), busy field 84-91 (g2 111-121, b3 74-81), title 120-127 (b3 101-112), movies b3 + 0.9. No PSP build is out.
- MEASURED by the schedule (quiet field, over b3): all on +8.0; check off +6.2 (check = 1.8-2.0 ms); stall every 16th batch: no gain; renderer paused -0.5 (layout as good as b3's); queue off +11.0 (queue saves 4.8); batches not sent +6.0 (the chip's side costs 0.2 ms: the renderer's cost is CPU work in the hooks). Model 65.8 vs PSP 68.2: 2.4 ms outside it.
- LEFT: title/BIOS +14 to +20 = the picture step copying 150-160k px a frame into the mirror for pictures the GE draws nothing in (picture step 10.87 ms per picture, b3 1.66); lever: show such pictures from the exact VRAM, ~-18 ms [I]; comes with stage 5's native-size picture.
- g3 changes: (1) texture pages as 32 KB units (4-bit page or half of an 8-bit page, wide 2-unit page only when a primitive crosses u 128), 4x16 tiles, only needed dirty tiles copied; (2) lazy mirror "gelazy": CPU-written parts marked stale per 16 px of a row, copied only inside a shown picture; a primitive over a stale tile falls back; (3) queue "gequeue": primitives written down and handled together before any other VRAM write (pre hook) / picture; off while the drawing area meets a page or palette in use; pages/palettes unused 30 pictures dropped; (4) GE_NV 8192, last-hit lookups; (5) switches "gestall N", "genull", run-time "ge 0/1", "at F key value".
- LAYOUT FINDING: per-build cache luck was the Psx structure (calloc) and the psxst section moving with code size (idle loop: stack + Psx + SPU voices in one data-cache set = +15k D-misses, +4 ms [I]). Now deterministic: main_ff8.o/ge_ff8.o linked last, -fno-reorder-functions for main, psxst on an 8 KB boundary (anchor in rt.c), Psx at memalign(8192)+P_OFF; P_OFF 4096 best of 0/2048/4096/6144 (Makefile). Open lever: finer P_OFF search; code order for I-misses (138k vs b3 129k a frame).
- MODEL CHECK 2026-11-11 19:00 (psp/prof_g1 vs prof_g2): g1 and g2 run the same instructions; g1 has more misses (quiet 35k D vs 17k). Model g1 - g2 = +7.5 heavy / +6.1 quiet, the PSP measured +6.9 / +5.5: the simulator reproduces placement differences to ~0.6 ms; outside the model only +1-4 ms (g1 +1.2/+3.5, g2 +1.9/+4.1).
- Model after g3 [I] (psp/prof_g3e): quiet 65.8 (renderer +5.2), heavy 84.0 (+8.8). 
- Emulator profile how-to: folders /home/claude/msf/PSP/GAME/FF8Q, FF8Q1-3 (cfg "prof A B"), PSPPROF_FLAG=0x<g_emuprof from psp-nm>, psp/prof_ms.py <elf> <dump> 500; miss leaders: scratchpad ge/topmiss.py. 4 runs in parallel take ~10 min.
- Leads: texture windows the GE path refuses (menu at 28,000: 19.6 a frame, the lazy mirror then drops ~190 rectangles a frame to the exact path) could get their own page; tex_select's 64-bit tile masks (~250 instructions a primitive).
