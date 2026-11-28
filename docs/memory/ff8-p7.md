---
name: ff8-p7
description: FF8 play build p7 (delivered 2026-11-18 13:25, Ffm skipped p6) RAN on the PSP, log READ 14:31-14:57: exact, world map 56.6 ms, fights 31-35 / 44-46 / 51-52 (4 enemies); files, checks, figures
metadata:
  type: project
  modified: 2026-11-18T10:32:44.457Z
---

Follows [[ff8-p6]] (p6 READY, not yet delivered) and the dev work in [[ff8-w5]] and [[ff8-w12]]. Notes: stage5_notes.md entry "2026-11-18 09:30- layout r5; p7 candidate" (md5 af672961 after it). Sources /mnt/project-files/ff8/src_p7 (read back OK).

**Files [C]:** container psp/rel_p7 FROZEN. EBOOT b799455b (18,863,260 B, "FF8 play p7", build id 4b7331fb, 3 LOAD, g_emuprof 0x0a0a1b80), ff8.cfg = p4's 9c719328, qs_bb1554dc.st = p6's state 17f9b339 (hash cc3262c38aff97f0) + .ok, ovl/ 35 files (p6's names), FF8T.elf, FF8T.map, units.txt, ff8_r5.x, link_cmd.log. Deliver to FF8\Builds\FF8T_p7\FF8T; logs to FF8\LOGS\p7.

**DELIVERED 2026-11-18 13:25** to FF8\Builds\FF8T_p7\FF8T, all 40 files md5-checked on the PC (= rel_p7/md5_delivery.txt). Ffm 13:19 "can we go strait to p7? can run p6 if realy needed": p6 was copied to Builds\FF8T_p6 (40 files checked) as a spare, never run. README_p7 b894ce09: PSP guess from p4's run [I] (world map ~56 / ~50 ms, fights ~31 / ~35, title ~76-81); go-back = p4's files + ovl. Logs to FF8\LOGS\p7. Staged copies deleted. NEXT BUILD (p8 = r6 + w20 + later levers) only after p7's log is read.

**Checks on the exact files [C emulator]:** route 80/80, p1 683 + card 0d48a931 (written 2x), p3 53 + card, fx1 132, p4 session lazy + eager = p4's out, [vsum] 49 = p6's eager; interp counts = p6's; GE check lines = p4's (worst 2+ steps 0.66%). Heap largest new block: p1 9,216 KB (= p6), p4 11,264 (p6 11,520), fx1 11,776 (p6 12,032); PSP = emu - 5,120.

**Model [C model] p6 -> p7:** world map 80.1 / 73.4 -> 60.4 / 53.7; fights 37.8 / 42.9 -> 32.1 / 36.3; rooms and title equal; fx1 fight 48.3 -> 41.0, Quezacotl 78/115/81 -> 74/108/76, Shiva 89/91 -> 85/88, Blizzard 79 -> 73.

**Dev left:** r5 world map +0.9 over r4: rt_asm_full I-miss conflict, mark_dirty1 loop over 128 slots (compact array idea), overlay game code I-misses; rect_draw_slow 1.8 ms in fights; px8 span in assembly; summons' stacked see-through layers.

**p7 RAN, log READ 2026-11-18 14:31-14:57 (PC FF8\LOGS\p7; container psp/psp_p7; notes entry "14:31-14:57") [C PSP]:** 331 MHz; quick start to frame 98,687 (title, new game, opening movie from file 0 late, rooms, save at 69,248, world map, 3 fights). EXACT: emulator replay FF8R7 (padsim the PSP's padlog, card 0d48a931 at start) gives the same prints, card 4691e69b, 72,406 interpreted blocks (fight stage code 0x8013E000). Heap end 4,096 KB (emu 9,216). Worst 2+ step picture 1.64% (fight). ms a frame PSP / model: title 77.6 / 63.3 (blank waits 7.0), rooms 25-31 typical, WORLD MAP 56.6 / 57.3 (p4 102-124), fight 1 31-35 / 34.9, fight 2 (in a room) 44-45 / 46.5, fight 3 (4 enemies) 51-52 / 54.0. Fight 3: lz_run 10.5 + rect_draw_fast 4.9 ms = software drawing of video memory read back as textures (lever: GE). Then [[ff8-p8]].
