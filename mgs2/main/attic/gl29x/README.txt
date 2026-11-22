MGS2 gl29x, the fix for the deck's black shadow marks (MGS2T_gl29x) - personal use only, do not share (it contains code built from
your MGS2 disc).

What it is: gl29's EBOOT and files, unchanged, with a new mgs2.cfg. Nothing else in the game changes.
  - From vblank 29,490 to the end (the rain wait and the walk), every shadow mark on the deck floor is drawn the fixed way: with its
    floor's own corner points (gl29's "X"). In gl29's run this put back all the missing shadow in the wait, and every walk capture
    with it matched the emulator. The mark moves by less than a pixel against the emulator; you should not see that.
  - The rain wait's 20 test pictures are gone, so the wait no longer pauses.
  - A few marks each frame (about 2 in the walk) can't be drawn this way yet and are drawn as before.

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. During the walk the picture pauses 9 times and freezes for a quarter second about
  every 3 seconds (captures). Logs come back as usual in LOGS\gl29x."

What it saves next to the EBOOT (about 25 MB in all):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28 and gl29. No new hardware writes, nothing written to flash, 333 MHz as always, no power or clock setting.

What the run should show: all hashes equal hashref.txt (72e0ca4c); the pictures up to vblank 24,786 equal gl28's; every frame's list
equals the emulator's (the log's digests, each with arm X from 29,490); no R presses in the walk.

How it runs: as gl29's. Claude renames the folder on the stick from MGS2T_gl29 to MGS2T_gl29x over PSPLink (only mgs2.cfg and this
README change) and starts it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)"
so the logs can be copied to LOGS\gl29x. A short run, about as long as gl29's.
