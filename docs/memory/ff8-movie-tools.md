---
name: ff8-movie-tools
description: FF8 phase 1 tool chain: a pure movie's .ffv from the disc + the Remastered file (sound format on the disc, picture timing choice, which Remastered file, converter and batch tools); results in ff8-movie-lineup
metadata:
  type: project
  modified: 2026-11-17T12:24:11.606Z
---

Phase 1 of Ffm's card 2026-11-16 19:19 (all pure movies from video files; [[ff8-m3]], [[ff8-movie-survey]]). Notes: ff8/stage5_notes.md "PHASE 1 TOOL CHAIN" and "2026-11-17 12:25". Tools in /mnt/project-files/ff8/movie_survey/ (working copies /home/claude/ff8/trial/tools/).

- **Movie sound on the disc (mvsound.py):** in each picture's SMN / SMR sectors: "AKAO" header at 0xF8, SPU ADPCM from data offset 0x168, 2,940 samples a picture and channel, 44,100 Hz, SMN left, SMR right, decoder history carries across pictures. Against the reference's capture: intro correlation 1.00000 (gain 0.9912); second movie 0.993, the rest = the field's own sound under the movie (not in the stream).
- **Picture timing:** disc-made files show picture i at sample (i + 2) x 2,940 (OUR CHOICE inside the PS1's 1.1-3.9 picture lag); PS1 frame of picture i = 4 x i.
- **Which Remastered file (mvmap.py, table mvmap_2026-11-16.txt):** the j-th stream WITH pictures of disc N = `data\movies\disc<N-1>_<j>h.avi` in other.zzz (discs 3-4 start with two sound-only streams, discs 1-2 with one). Its size-based offset is NOT reliable (wrong on disc 1 stream 13).
- **Offset:** by pictures and by sound, rule and results in [[ff8-movie-lineup]] (mvthumb.py parses the PS1 pictures from the disc: 8 SMJ sectors x 2,040 bytes = one BS v2 frame, 20 x 14 macroblocks in columns, MPEG-1 run/level table, DC of each Y block = a 40 x 28 grey picture; mvalign2.py, mvpath.py, mvaud.py, mvfull.py, mvfine.py; mvalign.py's change score alone is NOT enough).
- **mkmovie_disc.py** `<disc .bin> <lba> <pictures> <remastered movie> <out.ffv> [offset|auto|map:<file>] [delay 2] [388 272 400] [q 90]`; pass the decided offset ("auto" is the old size method); repeats the last frame if the Remastered file ends inside the last 16 pictures. batch_all.py `<mvmap table> <disc N> <disc image> <other.zzz> <out> [lba=offset ...]`; mvcheck.py checks a finished file end to end.
- **Done 2026-11-17:** all 54 pure movies, on the PC in FF8\Movies\mvd: [[ff8-movie-lineup]].
- **Answered 2026-11-17 (notes "13:45"):** play-time clock and the sound fade at a movie's end: [[ff8-movie-fade-clock]]. mvfade.py <in.ffv> <out.ffv> puts the game's fade into a file (NOT yet applied to the 54 files).
- **Open:** phase 2 (mixed movies); the game's own music stands still while a file plays (sound stage).
Converted movie files are game-derived: container or Ffm's PC only.
