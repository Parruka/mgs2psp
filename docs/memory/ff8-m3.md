---
name: ff8-m3
description: FF8 movie trial m3 (two movies from video files, one file per movie in folder mv), delivered 2026-11-16 19:46, the one build out; faults fixed, build recipe, what is next
metadata:
  type: project
---

Follows [[ff8-movies]] (m1, m2, Ffm's card "A. Video files, Remastered pictures") and [[ff8-movie-survey]].
Notes: /mnt/project-files/ff8/stage5_notes.md "TRIAL m3 DELIVERED"; sources ff8/src_trial_m3.

**m3 DELIVERED 2026-11-16 19:46** to PC FF8\Builds\FF8T_m3\FF8T: EBOOT 387a014e ("FF8 trial m3"), ff8.cfg (frames
16553, "mvdir mv", "movie intro_rm.ffv"), README, mv\d1_154480.ffv (6 MB). THE ONE BUILD OUT; logs to FF8\LOGS\m3;
no new PSP build before its log is read. Container psp/rel_m3 (never touch), layout folder psp/vk (never make).
Asked Ffm: does the second movie look and sound right, how does its last second (hand-over, no fade) look.

**What is in it:** second movie = disc 1 stream 2 (sector 154,480, 199 pictures) from the Remastered's
disc00_01h.avi, frame offset 0 (the intro's is +28: MEASURE PER MOVIE, change-profile correlation); player picks
<mvdir>/d<disc>_<sector>.ffv per movie, "movie <file>" = single-file fallback (intro_rm.ffv already on the PSP).

**Two faults found in the emulator, fixed:** (1) MovieOpen sits in several RAM snapshots of the field module and
gen.py hooked only one: gen.py idle_of() now hooks every snapshot whose 24 words at 0x801E8104 match (new code
folders recomp/genj PSP, gencj x86 = psxrcj, exact aae53db0). (2) A movie mid-game found the renderer's list open
(gestall), sceGeDrawSync never finished, the player gave up after 2 s: mv_play now calls ge_ren_present(...,0,0)
first.

**Build recipe (no new PGO run needed while rt.c, ref/*, ge_ff8.c are unchanged since tmg):** new folder, copy
run_tmg/gcda, `make G=genj PGO=use FSEC=-ffunction-sections TITLE=...`; then g_a_rt.o from layout/split_rt.py
<gen>/a_rt.S (NOT in the Makefile: without it 104 units are misplaced); vi's ff8_v3.x + place_v2.txt; link.sh with
BLOG = the build log's link line; mk.py units + check (725 placed, 0 misplaced). Model vk: 63.96 / 41.95 / 24.97 /
20.55 ms (= m1's). Exact with the route off: 80/80, 73/73 profile, 57/57 quick.

**Next (phase 1, 52 more pure movies):** per movie = PS1 timing and sound + Remastered file + measured offset. The
x86 reference reaches only the movies on our route, so the other movies' PS1 sound must be decoded from the disc's
own movie sound sectors (SMN / SMR), format still to work out; pictures come from the Remastered only.

**m3 RAN, read 2026-11-16 20:30 (psp/psp_m3, log d480db9c):** both movies from files, 0 late, 0 bad, sound complete, state = the emulator's; BUT Ffm saw a purple pixelated picture at the second movie's end, reproduced in the emulator (my delivery check missed frames 8,627-8,785). Causes: (1) MovieOpen is only "get ready": this movie is opened at 19,320 (route off) and first shown at 20,133, 813 frames later, so the file played 13.5 s early; (2) the "last 16 pictures by the game" hand-over: the game leaves the shortened movie after 2 pictures and shows 24-bit data in 15-bit mode (why: UNKNOWN). Next: hook at the real start of showing, new hand-over, every hand-over frame looked at in the emulator before a PSP build (cfg "showlog F" / "showlogn N" logs each drawn picture). No build is out.
