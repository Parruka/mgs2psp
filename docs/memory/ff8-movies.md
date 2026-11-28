---
name: ff8-movies
description: FF8 port, movies as video files (Sony libraries only): intro trial m1+m2 PASSED on the PSP (2026-11-16); card open on extending to the other pure full-screen movies
metadata:
  type: project
---

**Ffm's idea (2026-11-13 18:39):** pure full-screen movies (intro and the like) as video files on the PSP's video
hardware; in-game movies (characters drawn on top) stay on the exact PS1 path.

**DECIDED by Ffm's card 2026-11-13 19:00: "A. Trial: intro as a video file".** Ffm's OK covers: a TRIAL that plays
the INTRO ONLY through the PSP's video hardware = Media Engine THROUGH SONY'S OWN LIBRARIES; sound from the file;
game state after the intro not comparable with the reference while the route is on; in-game movies stay exact.
**How to apply:** no kernel module, no register writes of ours (those need a new OK); the other pure movies need a
new answer from Ffm after the trial (which movies are pure full-screen is UNKNOWN).
Cards 20:53 "A. PS1 first, Remastered next" and 20:59 "B. Whole picture with side bars" (388x272).

**Design** (notes: /mnt/project-files/ff8/stage5_notes.md "INTRO VIDEO TRIAL m1" ... "TRIAL m2 DELIVERED"; sources
ff8/src_trial_m1, src_trial_m2): hook at the field module's MovieOpen (0x801E8104; intro = sector 282,220, 2,897
pictures); player in psp/main_ff8.c (cfg "movie <file>", "mvtail 16"): sceUtilityLoadAvModule(AVCODEC) +
sceJpegDecodeMJpeg, a sceAudio thread as the clock; the game gets only the last 16 pictures and ends the movie
itself; scripted pad read 11,494 frames ahead; any failure = the game plays the movie itself. File format .ffv v2
(JPEG q90 pictures + PS1 sound; trial/tools/mkmovie.py, mkmovie_rm.py). Quick start pad_quick.txt: the intro at PS1
frame 3,309 (leaves the title credits at once, so the credit pictures are not shown: by design).

**m1 RAN ON THE PSP 2026-11-13 (psp/psp_m1):** AVCODEC load 0, 4,348 KB free throughout; 2,881 pictures, 0 late;
decode 4.34 ms, read 9.94; Ffm: sound buzzed, rest synced, hand-over "looked good". Buzz cause [I]: one buffer
refilled right after sceAudioOutputBlocking returned and/or 4 KB stick reads per block; fixed by m2's ring.

**m2 DELIVERED 2026-11-13 21:47** to PC FF8\Builds\FF8T_m2\FF8T (EBOOT c479531b "FF8 trial m2", ff8.cfg, README,
intro_rm.ffv 96 MB = the Remastered's intro, frame = PS1 picture + 28, 388x272 on a 400x272 canvas, PS1 sound),
THE ONE BUILD OUT, logs to FF8\LOGS\m2. New: sound ring of 128 blocks filled by the main thread in 64 KB reads
(never refills a playing block); first picture decoded before the sound starts; wide drawing. Container psp/rel_m2
(never touch), layout folder psp/vj (m1: rel_m1, vh, vi). Exact with the route off (80/80).
**m2 RAN, log read 2026-11-16 19:17 (psp/psp_m2):** Ffm "sound looks fixed and synced"; Sony's decoder takes
400x272: 2,881 shown, 0 late, 0 bad; decode 6.40 ms a picture, read 8.77 (pictures are 21 KB, m1's 16 KB: the read
is ~9 ms whatever the size); sound 0 silent blocks; no memory taken. TRIAL PASSED. No build out now.
**DECIDED by Ffm's card 2026-11-16 19:19: "A. Video files, Remastered pictures"**: ALL pure full-screen movies
play from video files (Remastered pictures with side bars, PS1 sound, Sony's libraries only); movies with characters
on top stay exact; exact route stays as fallback. Size as built 497 KB a movie second (all 3,461 s = ~1.7 GB [I]).
Survey: [[ff8-movie-survey]] (54 pure). m3 delivered: [[ff8-m3]].
TRAPS: the trial's functions are in section .text.mv so main_ff8.o(.text) keeps its layout slot; PspProf needs
PSPPROF_SWITCH=1; device_commit_files takes at most 25 MB a file (parts to _claude_tmp\ff8, joined with cat).

**Title credits (Ffm asked 2026-11-13):** the black and white pictures before the menu are stills, NOT a movie
(0 MDEC commands before the menu). The Remastered has them only at the PS1's size (loop01-14.lzs, 640x400, in
data\lang-en\main.fs); only the name overlays and logos have 3x PNGs: possible later upgrade.

Related: [[ff8-s9]], [[ff8-s8]], [[ff8-decomp]], [[ff8-u1]].
