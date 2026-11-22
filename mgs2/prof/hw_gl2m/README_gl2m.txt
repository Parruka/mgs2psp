MGS2 harness gl2m memory fix (MGS2T_gl2m) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl2d (the game's picture on the PSP with the system diagnostics in battery.txt) with one fix, for the crash you
photographed. Nothing else changes: the game's code, the route, the picture, the timing windows and the diagnostics are gl2d's, the
Media Engine is off, and the game's state hashes must stay the PC's.

Why: at vblank 26108 (the second codec call) the game loads 1.1 MB from the disc in one go. The harness used to copy every 32 KB piece
of it into a waiting area in memory and hand the pieces to the game only at the end of the load, so the whole 1.1 MB sat in memory at
once. With the picture on, the PSP has about 0.9 MB free there: the copy found no memory and the harness wrote to address 0, which is
the crash on your photo. Now each piece goes straight into the game's memory as it arrives, in the same order, so nothing waits and the
game sees exactly the same data at the same moment (checked on the PC on all four routes, and in the emulator with the PSP's smaller
memory: the old build crashes there, this one does not). If memory ever runs short again, the harness now stops with a message
("the machine stopped, code 7") instead of crashing.

What you will see: the same as gl2d: the logos, title and menus; a black opening apart from some subtitles; the codec screen about
minutes 15 to 21 (faces and text not drawn yet); mostly black again with a few short pictures; from about minute 34 the Tanker deck
with the life bar, radar and weapon boxes, then about 8 minutes indoors with the four timing settings taking turns. Snake and the
guards are not drawn yet. Past minute 31 (where gl2 and gl2d stopped) is new ground on the PSP with the picture on.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl2m from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl2m/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). The gl2d folder can stay or go.
3. Start "MGS2 harness gl2m memory fix" from the Game menu. The crash catcher's self-test is offered as before; press O (or wait 15
   seconds) to start the run.
4. Please run it on the cheap battery, charged, with the USB cable pulled out before the start, to the end (about 45 to 50 minutes).
   Do not put the PSP to sleep. HOME quits at any time.
5. If it switches itself off or crashes, please note roughly when (minutes in) if you saw it, photograph any red text, and send
   log.txt, battery.txt and crash.txt.
6. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files. The run writes them next to the EBOOT on the memory stick: copy them to the PC (for example into
   LOGS\gl2m) before that folder is deleted or a new build is copied over it.

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No register writes: the Media Engine is not started.
