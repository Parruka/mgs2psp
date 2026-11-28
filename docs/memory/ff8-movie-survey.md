---
name: ff8-movie-survey
description: FF8 PS1 movies on all four discs classified 2026-11-16 by each picture's flag byte (24-bit = pure movie picture): 54 pure, 36 mixed, 14 game-drawn; method, files, plan
metadata:
  type: project
---

Survey for [[ff8-movies]] (Ffm's card 2026-11-16 "A. Video files, Remastered pictures": all PURE full-screen movies
from video files). Notes: /mnt/project-files/ff8/stage5_notes.md "MOVIE SURVEY"; list
/mnt/project-files/ff8/movie_survey/movies_all.txt (disc, stream, lba, pictures, seconds, class, flag runs) and
scan_movies.py; container trial/survey/.

**Method:** each movie picture starts with an "SMN" sector (raw offset 24: "SM", type N/R/J, index, last index, u16
frame); its first data byte = the picture's flags (names from the public decompile's movie.c, verified): 0x01 24-bit,
0x02 fade in, 0x04 fade out, 0x08 mask on, 0x10 mask off, 0x20 hide field, 0x40 hide actors. 24-bit = the PS1's
24-bit display mode, where the game cannot draw over the picture = pure. Verified on our route's two movies (0
triangles / rectangles drawn while they play); [I] for the rest.

**Result:** 110 streams = 6 audio-only (2 sectors a picture; stream 0 of every disc) + 104 movies (31/34/32/7 a
disc; the Remastered has 105 files): PURE all through 54 movies = 2,204 s (64%; disc 1: 14, disc 2: 15, disc 3: 20,
disc 4: 5 = 1,012 s incl. the 472.7 s ending); MIXED 36 = 1,072 s (24-bit part 664 s; 12 start pure, 24 start
game-drawn); GAME-DRAWN all through 14 = 173 s. Every pure movie ends like the intro: 4 last pictures 0x40/0x50.
Size as built ~1.1 GB for the 54 [I].

**Correction:** the 199-picture movie at route frames 31,349-32,141 (disc 1 stream 2, lba 154,480 = Remastered
disc00_01) is PURE, not "in-game" as older notes say.

**Plan told to Ffm 2026-11-16 19:27:** phase 1 = the 54 pure movies with the intro's method, starting with disc 1
stream 2 (next trial build shows two movies); phase 2 = pure stretches inside mixed movies (needs a mid-movie
switch), decided after phase 1. Still to look at: each pure movie's last second under the 16-picture tail.
