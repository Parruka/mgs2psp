MGS2 harness gl3 Snake and guards (MGS2T_gl3) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl2m (the game's picture on the PSP, the build that ran to the end) with the characters drawn: Snake, the crew and the
guards, lit, skinned and with up to three layers each, computed exactly as the PS2 computes them. Everything else is gl2m's: the
game's code, the route, the timing windows and the diagnostics; the Media Engine is off, and the game's state hashes must stay the
PC's.

Two changes make room for the characters' textures (the guards' scene needs up to 2.2 MB of textures in one frame; gl2m had room
for 1.75 MB):
- smaller stacks for the harness's helper threads (48 and 24 KB instead of 128 and 64; the most any of your 74 runs used was 10.2
  and 5.3 KB), which gives the textures 640 KB more (2.6 MB, of which up to 2.3 MB hold textures);
- room pieces that lie wholly off the screen are no longer prepared: the PS2 draws no pixel of them, so the picture is the same
  (checked frame by frame on the PC), and about 4 in 10 of the room pieces indoors (half on the deck) are left out. The guards'
  scene then needs 1.95 MB. The fourth timing setting turns this off to time it, and one of its windows falls on the guards'
  scene: 2.2 of 2.3 MB, it still fits (checked in the emulator with the PSP's memory size).

The characters cost time: in the emulator an indoor step with the picture on went from 42 to 60 ms, most of it the lighting of each
vertex, which is what I will speed up next. The timing windows will show the PSP's own figures.

What you will see: as gl2m, now with the characters. The logos, title and menus; a black
opening apart from some text; the codec screen about minutes 15 to 21 and again about 30 to 33 (faces and text not drawn yet:
the text box is plain grey); a plain grey picture inside a viewfinder about minute 28 (that picture is a full-colour image, which
the PSP side does not convert yet); from about minute 35 Snake on the aft deck in the rain; from about minute 42
indoors: Snake in the corridors, the guard in the hallway, the alert and the evasion, with the four timing settings taking turns
(about 13 minutes). Still missing: the radar's map (the radar box stays nearly empty), and a few surfaces close to the camera
show as flat colour. The black band over the corner during the alert is the PS2's own picture.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl3 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl3/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). The gl2m folder can stay or go.
3. Start "MGS2 harness gl3 Snake and guards" from the Game menu. The crash catcher's self-test is offered as before; press O (or
   wait 15 seconds) to start the run.
4. Please run it on the cheap battery, charged, with the USB cable pulled out before the start, to the end (about 55 minutes to
   "Press X to exit"; gl2m took 50). Do not put the PSP to sleep. HOME quits at any time.
5. If it switches itself off or crashes, please note roughly when (minutes in) if you saw it, photograph any red text, and send
   log.txt, battery.txt and crash.txt.
6. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files. The run writes them next to the EBOOT on the memory stick: copy them to the PC (for example into
   LOGS\gl3) before that folder is deleted or a new build is copied over it.

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No register writes: the Media Engine is not started.
