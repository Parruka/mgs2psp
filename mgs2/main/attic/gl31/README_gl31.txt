MGS2 gl31, the ship drawn the new way in turns, no flick after the codec, shadows fixed on the whole run (MGS2T_gl31) - personal
use only, do not share (it contains code built from your MGS2 disc).
DRAFT by the build agent (2026-11-20): not delivered; the lead finishes this text before delivery.

What it is: gl30 with a new EBOOT (and the 8 files beside it, rebuilt with it) and three settings changed in mgs2.cfg. Each is
marked 'gl31' in mgs2.cfg; every other setting is gl30's.
  1. The ship's meshes drawn a new way, in turns (MGS2_NAT_CYCLE=0,0,3,3). Until now the game's own code built the drawing
     packets of every ship mesh (hull, deck, structures) each step. The new way skips that code: it writes a 48-byte record for
     each group of the mesh and the picture is drawn from the records. From vblank 24,001 the run takes turns of 100 vblanks:
     old, old, new, new. The ship is drawn in every turn (the 'ship not drawn' timing arm is in the code but not used). In the
     emulator the pictures of the old and the new turns are the same as gl30's. This is the new code's first run on a real PSP:
     the run measures what it saves, on the main CPU and on the Media Engine.
  2. No list is drawn into the buffer on the screen, from the start (MGS2_GL_FLIPWAIT_FROM=0, was 30500). In gl30's run you
     saw a flick right after codec call 2; its log has 1 list drawn into the buffer on the screen in the turn 28,800-28,900,
     where the wait for the screen was still off in every other turn pair. Now the wait is on in every turn.
  3. The shadow fix (arm X: each shadow mark drawn with its floor piece's own corner points) on every frame of the run, not only
     from vblank 29,490. In gl30's run the deck after the codec was still drawn the old way until 29,490, with the old holes,
     and the shadows came late. In the PC check run, from the codec's end to 29,490 every mark is matched to a floor piece
     (0 drawn the old way). Earlier in the run, in the cutscene before the deck, 192 of 224 marks (17 frames) are still drawn
     the old way (no floor piece matches them); those look as in gl30.

What you do (the same as last time, plus one look):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. Watch the ship too: every 100 vblanks it changes between the old and the new
  way of drawing; if any part of the ship flickers, goes missing or changes look, say when. After codec call 2 there should
  be no flick, and the deck's shadows should be there from the first frame. During the walk the picture pauses 9 times and
  freezes for a quarter second about every 3 seconds (captures). Logs come back as usual in LOGS\gl31."

What it saves next to the EBOOT (about 25 MB in all, as gl30):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28, gl29, gl29x and gl30. No new hardware writes, no new system calls, nothing written to flash, 333 MHz
as always, no power or clock setting. New in memory: at the start the build asks the system for two blocks of 108 KB outside
the game's heap (the records' arrays), with the call the picture code already uses for its own block. gl30's run on the PSP
had 224 KB in one piece there (432 KB in all), so both should fit. If the system gave only one, the pictures stay the same and
the log says 'ONE ARRAY ONLY'; if it gave none, the ship would not be drawn in the new turns and the log says 'NOT MADE'.

What the run should show:
  - all 62 hashes equal hashref.txt (177108d4: a new reference, the PC twin's run with the same turns; with the builder skipped
    its packet memory keeps older bytes, so the state hash differs from gl30's 72e0ca4c from vblank 24,300 on);
  - every frame's list from 29,490 to 31,400 equal to the emulator's and the PC twin's (916 digests, each with arm X; in the
    walk about 2 'sub' a frame, fallbacks 0);
  - near the start a 'native step 1:' line: the arrays made, and the system's memory outside the heap before and after
    (emulator: largest block 224 KB, total 432 KB before; 116 KB, 216 KB after. gl30's PSP run had the same 224 and 432
    before; the PSP's figures after are this run's to show);
  - 75 'nat turn' lines, one per 100 vblanks from 24,000: in every line 'the builder run instead 0', 'recorded with no array 0',
    'early fills 0', 'dropped 0'; the new turns' times against the old turns', separately for the Media Engine's turns and
    the main CPU's turns (this is the run's measurement);
  - three 'gl29 dz window' lines: 0-29,489 (emulator: 4,875 marks, 4,239 + 444 matched, 192 the old way, all before the
    deck), 29,490-31,400 (12,247 marks, 10,306 + 1,941 matched, 0 the old way, as gl30) and one empty line after 31,400;
  - the 'gl27 display' lines: 'flip wait on' and 'lists drawn into the shown buffer 0' in every turn (emulator: 315 of 315);
  - the 16 picture pauses each with a 'the flip wait kept' line and no 'GAVE UP' line;
  - every walk capture matching the emulator; no R presses.

How it runs: as gl30's. Claude copies the new files over MGS2T_gl30 on the stick over PSPLink (renaming it MGS2T_gl31) and
starts it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)" so the logs
can be copied to LOGS\gl31. A short run, about as long as gl30's.
Keep the USB cable in, as in gl30: PSPLink stays on during the run, so the cable powers the PSP.
