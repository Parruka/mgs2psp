---
name: ff8-title-speed
description: FF8 title / credit screens' speed (2026-11-17): why they are slow (200 one-line uploads a frame, 22 register accesses each), levers D E F built in the dev build (fade frames -15%, plain -6% in the model), what exact work is left, full speed needs a non-exact library-level upload (card first)
metadata:
  type: project
  modified: 2026-11-17T20:36:26.958Z
---

Notes: ff8/stage5_notes.md "2026-11-17 20:25", "20:35" (its poll-loop reading is WRONG), "20:50" (correction + results). Sources /mnt/project-files/ff8/src_l2 (gpu4.c, ge4.c, swar_test.c). Follows [[ff8-p3]] (my promise to Ffm 19:50: title speed is the next job, in the next build), [[ff8-fight-speed]] (dev method l2.sh).

**What a credit frame is [C, register trace on the PC reference, scratchpad x86t with env IOLOG=frame]:** 640x480 interlaced; the game uploads the shown field line by line = 200 LoadImage calls of 640x1 a frame, then (in fades) one see-through 640x400 rectangle. One upload = 1,199 PS1 cycles and 22 register accesses (GPUSTAT x4, GP1 x2, GP0 x4, DMA MADR/BCR/CHCR, CHCR read, I_MASK r/w x2, timer 1 x4). NO wait loop: our DMA model moves the words at the CHCR write. Cost on our side ~35,000 PSP instructions an upload, a flat list (block ends after each access ~7,500, slow loads ~6,500, pixel path ~7,700, rt_asm_full 3,200, PS1 I-cache model 2,950, game code 3,500).

**Built, dev EBOOT e26d029b (psp/vy, NOT delivered; p3 is out):** D fade rectangle two pixels a word (gpu rect_draw_fast; RECTCHECK 2,540 / 0 differ, exhaustive 803 M pairs / 0 differ); E a row's words by memcpy (XFERCHECK 38.3 M runs / 0 differ; worth ~0.4 ms); F ge mark_dirty1 skipped when the write can meet no page / palette (`dm_bands`). Proof: title run from the p2 quick-start state to 12,000: 20 hashes + cycle counts = the PC reference's.

**Model [C model], ms a frame before the show step (show = 10-14 ms on the PSP [C p2's log]), windows 3,000 / 5,000 / 6,700 / 8,000 / 9,500 / 11,000:** p3 65.21 / 97.05 / 33.03 / 46.25 / 64.21 / 74.07; now 61.45 / 82.10 / 32.96 / 47.66 / 61.32 / 68.26. Profiles psp/run_l2b/tt (p3), tt2 (D), tt3 (E+F); title run = FF8P2 cfg (`quick 0`, `stload qs_2dcf8acd.st`, `padsim padlog_p2.txt`, `quitat 12000`, `hash 500`, six `prof` windows), PSPPROF_FLAG 0x0a196b00.

**Not built (exact), estimates [I]:** G fade owed on the hidden field's rows (-7 ms in fade frames); H accesses that change no event time do not force the next block end to re-read event times (13 of 22 an upload; rt.c; ~-5); C title path in the next generation's profile + placement (~-7). All together ~49 plain / ~63 fade = 22-27% of full speed with the show step. Windows 6,700 / 8,000 have no uploads but a polling loop (exe_80039444, exe_80044360, exe_80046304): not looked at, a wait skip may exist there.

**Full speed is out of reach of exact levers here.** The non-exact way = our own copy in place of the library's LoadImage, charging its measured PS1 time: gives up PS1-exact timing inside the call = a RULE CHANGE, needs Ffm's card with PC-measured numbers first (part of the promised "chip models at the library level" measurement). Not told to Ffm yet as a choice.
