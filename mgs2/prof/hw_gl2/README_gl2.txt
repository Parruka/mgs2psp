MGS2 harness gl2 texture blocks (MGS2T_gl2) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl1 (the game's own picture on the PSP) with one change: new textures are converted for the PSP a whole block of the
PS2's graphics memory at a time instead of one texel at a time. The pictures are the same bytes as gl1's (checked on the PC for
every texture of both routes, and in the emulator frame by frame over three stretches of the route); only the moments when
many new textures arrive at once get much shorter (in the emulator the longest single step goes from about 0.6 s to 0.17 s).
Everything else is gl1's: the game's code and the indoor route are v30's, the Media Engine is off, and the game's state is not
touched, so the state hashes must stay the PC's.

What you will see (checked in the emulator with shots at about 30 points from the title to the end; the minutes are estimates
from gl1's run, which reached minute 16, and may be off by a few minutes later in the run):
- The logos, the title screen and the menus.
- About minutes 2 to 15: the opening is BLACK apart from some subtitles (for example "The whole thing stank..."). That is expected:
  that scene (the bridge in the rain) is drawn by the PS2's vector-unit programs, and those have no translation yet. It is the
  same in the emulator. gl1's README was wrong to promise the opening; sorry for that.
- About minutes 15 to 21: the codec (radio) screen, its frame and the frequency 141.12. The faces and the text are not drawn yet
  (dark panels and a grey box).
- From vblank 15700 to 16010 (about minutes 15 to 17) the harness checks the game's state at every vblank, so it moves slowly for
  about 2 minutes. That is expected too. gl1 switched itself off in this part, at minute 16.
- About minutes 21 to 34: mostly black again, with a few short pictures: the Tanker deck for about half a minute with the subtitle
  "All right. Let's look at your gear." (about minute 26), a camera viewfinder over a plain grey screen (about minutes 28 to 30;
  what the camera sees is not drawn yet), and the codec screen again (about minutes 31 to 34).
- From about minute 34: the Tanker deck with the life bar, radar and weapon boxes, then about 8 minutes indoors (the game's 2
  minutes indoors). It runs at the harness's speed, slower than the real game, so it looks like slow motion: that is the speed we
  are working on, now visible.
- Not drawn yet: Snake, the guards and a few other things (the same vector-unit programs; that is the next step). The rooms, the
  HUD (life bar, radar box, item boxes), the text and the alert sign are drawn.
- From vblank 31800 (the 2 minutes indoors) four settings take turns every 100 steps, for the timing: all on, picture off (the
  last frame stays on the screen for those 100 steps, a few seconds), the CPU waiting for each frame, and the old way of
  reading the vector unit's memory. That is expected.
- The crash catcher (its red record goes over the game's picture), battery.txt, and a 2-second limit on every wait for the graphics
  chip, as in gl1. Six frames are saved as shot_<vblank>.bmp next to the EBOOT.

About gl1's switch-off: gl1 switched itself off about 16 minutes in, with the game state exact and no crash record: the same kind of
stop as the earlier unexplained ones, this time with the Media Engine off. gl2 does the same game work as gl1 up to that point
(only the texture conversion differs, as above), so this run also shows whether it happens again at the same place.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl2 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl2/ (EBOOT.PBP, the eight ovl_*.bin
   files, SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). The gl1 folder can stay or go.
3. Start "MGS2 harness gl2 texture blocks" from the Game menu. The crash catcher's self-test is offered as before; press O (or
   wait 15 seconds) to start the run.
4. Please run it on the cheap battery, charged, with the USB cable pulled out before the start, to the end (about 45 to 50
   minutes). One run is enough. Do not put the PSP to sleep. HOME quits at any time.
5. If it switches itself off again, please note roughly when (minutes in) if you saw it, and send log.txt, battery.txt and
   crash.txt anyway. If a crash happens, the red text stays on the screen: please photograph it. If the picture looks wrong
   (garbage, flicker, wrong colours), a photo or a short video and the moment it happened help a lot.
6. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt,
   steps.csv and the six shot_*.bmp files.

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed (333 MHz as always). No register writes: the Media Engine is not started.
