---
name: ff8-movie-fade-clock
description: FF8 file movies, two answered questions (2026-11-17): the play-time clock counts every vblank incl. movies (addresses, exact repair), and the game fades a movie's stream sound in 20 steps at its end (mvfade.py)
metadata:
  type: project
---

Notes: ff8/stage5_notes.md "2026-11-17 13:45". Follows [[ff8-movie-tools]], [[ff8-movie-lineup]]; method: our disassembly /home/claude/ff8/exe.dis + names from the public decompile's symbol list ([[ff8-decomp]]) + reference states (`psxref --load states3/<f>.st --save <frame> <file>`, RAM at file offset 560).

**Play-time clock [C]:** VsyncHandler 0x8001167C, every vblank: if u16 0x80083794 (paused) == 0 and u8 0x8005F10C (g_vsyncSkip) == 0: ticker 0x8005F154 += 2191; at 131,072 the seconds u32 0x80078048 += 1 (ticker &= 0xFFFF). Second ticker 0x8005F15C the same way drives the countdown timer u32 0x8007804C (if not 0: -1). Measured across 4 stretches of the reference (through the intro and the second movie): exactly 2191 x frames. So play time counts through movies.
- A file movie stops the game: play time and a running countdown lose the movie's length (all 54 pure movies = 37 min).
- **Repair (BUILT in p1 2026-11-17, cfg `mvclock 1`, frames = round(pictures x 3.98843) + 3 for disc-made files: [[ff8-p1]]):** at the file's end run the handler's time part once per replaced PS1 frame. List it in the deviations when it ships.
- Not repaired by this: the game's sound engine (music under / after a movie) also stands still: sound stage.

**Fade of a movie's sound at its end:**
- [C] ~25 of the 54 files are loud in their last 5 pictures (rms 3,000-11,300), so the fade is audible.
- Code: sound command E5 (handler 0x8001E940): step = trunc((target - vol) / time); function 0x8001A674 on each 4th sound tick adds it to g_sndStream (0x80077298) +64, voices get vol >> 1. Sound engine = 240 Hz [C: 4.0116 ticks a frame], so a step = 735 samples.
- [C, second movie] the movie code sends E5(0x14, 0) when the stream's picture counter (+0x20) reaches pictures - 5: 20 steps of -1,625 from 32,512 down to 12; first step 15,475-16,184 samples before the stream's end, silent for the last ~0.4 picture. That all movies do this: [I].
- **Tools:** movie_survey/mvfade.py <in.ffv> <out.ffv>; mvfade_set.py plan|apply <folder> <md5 before> <md5 after> (pure Python, in place, md5-guarded, safe to repeat).
- **APPLIED 2026-11-17 13:50 to all 54 files** in the container and on the PC (FF8\Movies\mvd: 54 md5 OK with the new md5.txt; old list = md5_before_fade.txt; lists in movie_survey/mvd_md5_2026-11-17.txt (before) and mvd_md5_faded_2026-11-17.txt). 3 files unchanged (silent tails: intro, d4_150080, d4_193560). The PSP's FF8T/mvd/d1_154480.ffv is still the unfaded one: deliver d4a9fe4e with the next harness build.
