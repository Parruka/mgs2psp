---
name: ff8-m8
description: FF8 movie viewer m8 (standalone PSP program FF8MV that plays any converted .ffv movie file): design, emulator checks, delivery 2026-11-17, what is asked of Ffm, test switches
metadata:
  type: project
---

Notes: ff8/stage5_notes.md "2026-11-17 13:30: MOVIE VIEWER m8". Follows [[ff8-m7]] and [[ff8-movie-lineup]] (the 54 files).

**Why:** only two movies are on the scripted route; the other 52 files had never been SEEN on the PSP. I announced the viewer to Ffm on 2026-11-17 with the phase 1 report, as the next step (no objection).

**What:** a program of its own, no game code: source /home/claude/ff8/psp/view_ff8.c (md5 3ee36671; copy + Makefile in /mnt/project-files/ff8/src_view/, build folder psp/view, release psp/rel_m8). EBOOT f6ce2957 (158,700 bytes), module "FF8MV", PSP folder PSP/GAME/FF8MV, movies in its folder `mvd` (Ffm copies the PC's FF8\Movies\mvd, 1.07 GB; any subset works).
- NOT inside the harness: its layout has no room ([[ff8-harness-layout]]: main .text 652 bytes free, .text.mv full, any new .text.* lands before the anchor).
- Player = m6 / m7's, copied: Sony's JPEG library, sound ring + thread, pictures timed by the sound, two real 8888 screens at VRAM 0 / 0x88000 swapped with NEXTFRAME. Only Sony's libraries. New firmware calls: scePowerTick(0) every 16 pictures (idle timer; told in the README), sceCtrlPeekBufferPositive, SetSamplingCycle / Mode. Default link script (ABIFLAGS + 2 LOAD = b1's shape, which ran).
- Keys: up / down, left / right = a page of 27, X play, START play all from here, O stop, HOME exit; never exits by itself. List sorted by file name (disc, then position): 1 = d1_154480 (m7's second movie), 14 = the intro, 53 = credits, 54 = the longest (472.7 s).
- log.txt (rewritten at each start): a block a movie: pictures shown / late / bad, decode / read / GE ms, samples sent, silent blocks.
- Test switches in view.cfg (never delivered): `auto N` (first N pictures of every file, 0 = whole; ends with "free memory at the end" so emu_run.sh stops), `scr K` (screen dump at picture K), `dir <folder>`, `keys DDO30XRS` (scripted pad; O<n> = O at picture n).
- Emulator folder /home/claude/msf/PSP/GAME/FF8MV, its mvd = HARD LINKS to trial/mvbatch, mvd, mvd_intro (no extra disk); run with `G=<that folder> sh emu_run.sh <abs out> 3000`; all 54 whole take ~100 s.

**Checked in the emulator [C]:** all 54 files whole (2,204.4 s): every picture shown, 0 late, 0 bad, every sample sent, 0 silent blocks; screens vs the file's own JPEG right (54 at picture 30, 49 at 100; bars black); scripted pad: cursor, pages, X, O, START all right. Speed and tearing: only the PSP can tell.

**Delivered 13:30** to FF8\Builds\FF8MV_m8\FF8MV (EBOOT.PBP, README_m8.txt, md5.txt; md5 OK on the PC); logs to FF8\LOGS\m8. (Was the one build out until Ffm closed it.)

**CLOSED by Ffm 2026-11-17 13:46** ("why do i ned to run m8?" -> told it is optional -> "skip, i will watch later i say something if anything is wrong, lts continue"): no run, no log expected; NO BUILD OUT; the viewer stays on the PC for Ffm to use when they like. Lesson [mine]: do not ask Ffm for a run that only re-checks an already approved player; say up front when a build is optional.

**Was asked of Ffm:** anything wrong in a movie, by list number; log.txt. Open one-frame doubts by list number: 35 (d3_232829, first 13 s), 48 (d3_281559, 14-20 s), 49 (d3_287809, first 7.5 s, last 3 s).

**How to apply:** when Ffm reports, read LOGS\m8\log.txt (late / bad / silent blocks, decode and read times per movie against m7's 6.4 / 8.8 ms), record, then next: sound end-fade, play-time clock, phase 2 (mixed movies).
