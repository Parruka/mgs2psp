---
name: ff8-p6
description: FF8 play build p6 (READY 2026-11-18 04:50; copied to the PC 13:22 as a SPARE, never run: Ffm went straight to p7; replaces p5 which never went out): p5 + lean drawing-time pixel loop, deferred patch swaps, NCCS short path, new code placement r1; files, checks, model figures
metadata:
  type: project
  modified: 2026-11-18T04:46:48.231Z
---

Follows [[ff8-p5]] (p5 = summons from files + world map w2; built, NEVER delivered). Notes: stage5_notes.md entry "2026-11-18 03:00- world map w3" (md5 935e4e0d after it). Sources /mnt/project-files/ff8/src_p6.

**Files [C]:** container psp/rel_p6 FROZEN. EBOOT 8c91f896 (18,863,260 B, "FF8 play p6", 3 LOAD, g_emuprof 0x0a09f3c0), ff8.cfg = p4's 9c719328, README_p6.txt, qs_6c89b1d9.st (= p4's state 17f9b339) + .ok, ovl/ 35 files (as p5). md5 list rel_p6/md5_delivery.txt (40 lines). Deliver to FF8\Builds\FF8T_p6\FF8T; logs to FF8\LOGS\p6. Tell Ffm in one line that p5 never went out and p6 = p5 + four speed changes.

**Changes on p5 (each exact, each profiled alone):** (1) gpu8/gpu9.c tt_run4/tt_run8: drawing-time pixel loop in PSP assembly, 8 instr a pixel (was 22; gcc hoisted the key math); TT2 check: 36.9 M triangles, 0 differ. (2) gpu9.c lz_walk swaps patches only right before a VRAM write (LZ_SYNC): -42% swaps; VSUM 49/49. (3) gte6.c nccs_fast (NCCS/NCCT, sf=1, |BK|<2^30); GTE2 check 3.5 M commands, 0 differ. Compile GTE with scratchpad p1/lgt.sh (ref/gte.c must stay 96155868). (4) code placement r1 ([[ff8-layout]] method, new tool layout/mkr.py inside the l6q.sh script; traces of p4 session + fx1, lopt): I-misses -30%.

**Model [C model] p4 -> p6:** world map 120.7 / 109.0 -> 80.1 / 73.4 (~75 / ~69 on the PSP [I]); fights 40.3 / 45.1 -> 37.8 / 42.9; rooms 21.6 / 24.3 -> 20.4 / 22.9; title 66.5 -> 63.4; Quezacotl 153/240/120 -> 78/115/81; Shiva 131/132 -> 89/91; Blizzard 95 -> 79.

**Checks on the exact files [C emulator]:** route 80/80, p1 683 + card 0d48a931, p3 53 + card, p4 session 49 + VSUM 49, fx1 132. Heap = p5's (p1 9,216 KB, p4 11,520, fx1 12,032; PSP = emu - 5,120).

**Next (dev):** overlay area placement (world map game code 8.7 ms, fixed point for lopt), CLUT zero scan cache in draw_tri, summon see-through layers, spells family. A new layout is needed whenever runtime functions grow (l6q.sh drops grown ones out of their slots; check its "out of their slots" list).
