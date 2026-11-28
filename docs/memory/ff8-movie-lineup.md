---
name: ff8-movie-lineup
description: FF8 phase 1 DONE 2026-11-17: all 54 pure movies converted, checked, on the PC (FF8\Movies\mvd, 1.07 GB); line-up rule (offset 0 unless every picture measure agrees), sound-track check, the three exceptions, what stays open
metadata:
  type: project
---

Phase 1 of Ffm's card 2026-11-16 19:19 ([[ff8-movie-tools]], [[ff8-movie-survey]]). Notes: ff8/stage5_notes.md "2026-11-17 13:30"; every number in movie_survey/lineup_2026-11-17.txt; md5s mvd_md5_2026-11-17.txt.

**State (sound tails faded in place 2026-11-17 13:50: [[ff8-movie-fade-clock]]; md5s = mvd_md5_faded_2026-11-17.txt):** 54 disc-made files d<disc>_<lba>.ffv (Remastered pictures 388x272 on 400x272, PS1 sound from the disc, picture i at sample (i + 2) x 2,940) on the PC in `FF8\Movies\mvd\` + md5.txt, all md5-checked there; container trial/mvbatch (52) + trial/mvd + trial/mvd_intro (ephemeral, 967 MB: may be deleted when space is needed). Only the intro and disc 1 stream 2 have played on the PSP (m7 passed: [[ff8-m7]]). mvcheck.py: 54 of 54 OK. Largest file 232 MB (the 7,091-picture ending), largest picture 55,658 bytes; the memory stick needs 1.07 GB.

**Discs 2-4 again:** on the PC `_claude_tmp/ff8/mvsec.py <N> "<disc N .bin>" mvsec_d<N>.bin` (90 / 142 / 357 MB), stage, container mvsparse.py puts the sectors back into a sparse disc image; tools run unchanged. Files over 25 MB cannot be committed to the PC in one piece: split into 20 MB parts into `_claude_tmp/ff8`, `cat` on the PC, md5, then empty the parts to 0 bytes.

**Line-up facts [C]:**
- 52 of 54 Remastered files carry the PS1 stream's own sound at lag -28 samples in every third (mvaud.py): file time = stream time. Exceptions: disc 1 stream 13 (no sound in the file), the intro (sound 25.99 pictures later).
- The intro's pictures are 28 frames later but its sound 26: the Remastered shows a picture 2 pictures after its sound = the PS1's lag = our "delay 2" (independent confirmation).
- Picture measures can be off by 1-3 frames: a pan or scroll with slightly different framing looks like a time offset (credits: 2 frames = 4 text rows), a fade moves the mean absolute difference; mvalign's change score cannot separate neighbours in slow cut-free movies. mvmap's size-based offsets are unreliable.

**RULE:** offset 0 unless mad, votes, dif (mvalign2.py) AND the robust path (mvpath.py, penalty 20) agree on another one for the whole movie or up to a black stretch; never change the offset inside a moving scene. Close calls: mvfull.py (full-size decode + best shift).

**Offsets:** 51 movies 0; intro 28; disc 3 stream 25 (lba 260149) +2; disc 4 stream 7 (lba 193560, credits) -2 up to picture 1680 then 0 (map file; the change is in black). The converter repeats the last frame when the Remastered file ends early, only inside the game-shown last 16 pictures (disc 4 stream 6: 3 frames; disc 3 stream 25: 2).

**Open (kept at 0, doubt of one frame = 67 ms):** disc 3 stream 13 pictures 0-193, stream 32 pictures 207-298, stream 33 pictures 0-111 and 954-994 (-1 or 0).

**Not done:** no build plays files 3-54 (only two movies on the scripted route); no viewer; the 36 mixed movies = phase 2.
