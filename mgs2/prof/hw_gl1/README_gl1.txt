MGS2 harness gl1 live picture (MGS2T_gl1) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: the first build that shows the game's own picture on the PSP. The game's graphics commands (what the PS2 sends to
its graphics chip) are translated while the game runs and drawn by the PSP's graphics chip (GE) into two frame buffers; each
finished frame is shown at the next screen refresh while the game already works on the next one.
- The game's code and the indoor route are v30's. The Media Engine is off: the sound model runs on the main CPU as in your v30
  runs 3 and 4 (nothing is played yet). Nothing in the game's state is touched, so the state hashes must stay the PC's.
- What you will see: the picture from the first second on (title screens, the opening, the Tanker, then the 2 minutes indoors).
  It runs at the harness's speed, slower than the real game, so it looks like slow motion: that is the speed we are working on,
  now visible.
- Not drawn yet: Snake, the guards and a few other things (the game draws them with vector-unit programs that have no
  translation yet; that is the next step). The rooms, the HUD (life bar, radar box, item boxes), the text and the alert sign are
  drawn.
- From vblank 31800 (the 2 minutes indoors) four settings take turns every 100 steps, for the timing: all on, picture off (the
  last frame stays on the screen for those 100 steps, a few seconds), the CPU waiting for each frame, and the old way of
  reading the vector unit's memory. That is expected.
- At a few moments a single step takes much longer (up to about 0.7 s in the emulator): when the game suddenly needs many new
  textures (at the alert, 54 in one frame), they are converted for the PSP in one go. Expected for now; it is the next thing I
  will speed up.
- New for this first run with the picture: the crash catcher of the Media Engine builds (its red record now goes over the game's
  picture), battery.txt, and a 2-second limit on every wait for the graphics chip: if the chip ever stops, the log says so, the
  harness's text comes back on the screen and the run goes on without the picture.
- Six frames are saved as shot_<vblank>.bmp next to the EBOOT, to compare with the emulator's.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl1 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl1/ (EBOOT.PBP, the eight ovl_*.bin
   files, SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder).
3. Start "MGS2 harness gl1 live picture" from the Game menu. The crash catcher's self-test is offered as before; press O (or
   wait 15 seconds) to start the run.
4. Please run it on the cheap battery, charged, with the USB cable pulled out before the start, to the end (about 45 minutes,
   maybe more: a bit longer than v30). One run is enough.
   Do not put the PSP to sleep. HOME quits at any time.
5. If a crash happens, the red text stays on the screen: please photograph it. If the picture looks wrong (garbage, flicker,
   wrong colours), a photo or a short video and the moment it happened help a lot.
6. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt,
   steps.csv and the six shot_*.bmp files.

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed (333 MHz as always). No register writes: the Media Engine is not started.
