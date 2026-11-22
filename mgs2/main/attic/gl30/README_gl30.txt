MGS2 gl30, the last of the deck's black shadow marks (MGS2T_gl30) - personal use only, do not share (it contains code built from
your MGS2 disc).

What it is: gl29x with a new EBOOT (and the 8 files beside it, rebuilt with it). The settings in mgs2.cfg are gl29x's; only its
first lines (the notes) change. Nothing else in the game changes.
  - gl29x drew every shadow mark on the deck floor with its floor's own corner points, except about 2 marks a frame in the walk
    that it could not match to a floor piece. Those were drawn the old way, so they could still lose patches: in gl29x's run one
    capture near the end of the walk (live_31012) still had a lost patch of shadow. The reason: each of those marks covers only
    part of its floor piece. gl30 matches them too and draws them on their floor piece's own corner points. In the PC check run
    no mark is left over (0 drawn the old way, against 1,941 in gl29x over the same frames).
  - The matching is also much less work. In gl29's run the matching made each walk frame's picture list about 4 ms (main CPU) to
    9 ms (Media Engine) slower to build; gl30 looks only at floor pieces right next to each mark. How much this saves on the PSP
    is one of the things this run measures.
  - The picture is the same as gl29x's except on those marks, which should now keep their shadow (they move by less than a pixel
    against the emulator, as the other marks did in gl29x; you should not see that).

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. During the walk the picture pauses 9 times and freezes for a quarter second about
  every 3 seconds (captures). Logs come back as usual in LOGS\gl30."

What it saves next to the EBOOT (about 25 MB in all, as gl29x):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28, gl29 and gl29x. No new hardware writes, nothing written to flash, 333 MHz as always, no power or clock
setting.

What the run should show: all hashes equal hashref.txt (72e0ca4c); the pictures up to vblank 24,786 equal gl28's; every frame's
list equals the emulator's (the log's digests, each with arm X from 29,490 and the new 'sub' count: about 2 a frame in the walk,
fallbacks 0); every walk capture, live_31012 included, matching the emulator; no R presses in the walk; the walk's list-building
time ('list build' on the 'gl29 dz window' line) lower than gl29x's.

How it runs: as gl29x's. Claude copies the new files over MGS2T_gl29x on the stick over PSPLink (renaming it MGS2T_gl30) and starts
it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)" so the logs can be
copied to LOGS\gl30. A short run, about as long as gl29x's.
